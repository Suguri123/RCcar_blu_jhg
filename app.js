const state = {
  port: null,
  writer: null,
  device: null,
  characteristic: null,
  connected: false,
};

const $ = (selector) => document.querySelector(selector);

function setConnection(connected, name = '연결 안 됨') {
  state.connected = connected;
  $('#statusDot').classList.toggle('connected', connected);
  $('#connectionState').textContent = connected ? name : '연결 안 됨';
  $('#connectButton').innerHTML = connected ? '연결 끊기 ❌' : '연결하기 🔌';
}

// GATT operation busy 충돌 방지를 위한 전송 큐
let isSending = false;
let queuedCommand = null;

async function send(command) {
  const directionMap = { 'F': '전진 🔺', 'B': '후진 🔻', 'L': '좌회전 ◀', 'R': '우회전 ▶', 'S': '정지 ■' };
  $('#commandReadout').textContent = `${command === 'S' ? '대기 중' : '운전 중'} / ${directionMap[command] || command}`;

  if (!state.connected) {
    $('#transportLabel').textContent = '블루투스를 먼저 연결해 주세요!';
    return;
  }

  if (isSending) {
    queuedCommand = command;
    return;
  }

  isSending = true;
  try {
    const bytes = new TextEncoder().encode(command);

    if (state.writer) {
      await state.writer.write(bytes);
      $('#transportLabel').textContent = `전송 완료: ${command}`;
    } else if (state.characteristic) {
      await writeCharacteristic(state.characteristic, bytes);
      $('#transportLabel').textContent = `전송 완료: ${command} (${state.device?.name || 'BLE'})`;
    }
  } catch (error) {
    console.error('명령 전송 실패:', error);
    $('#transportLabel').textContent = `전송 에러: ${error.message || 'GATT 전송 오류'}`;
  } finally {
    isSending = false;
    if (queuedCommand !== null) {
      const next = queuedCommand;
      queuedCommand = null;
      send(next);
    }
  }
}

// HM-10 및 각종 BLE 모듈 호환 쓰기 함수
async function writeCharacteristic(char, bytes) {
  // 1. writeValueWithoutResponse (HM-10 표준)
  if (typeof char.writeValueWithoutResponse === 'function') {
    try {
      await char.writeValueWithoutResponse(bytes);
      return;
    } catch (err) {
      console.warn('writeValueWithoutResponse 실패, fallback 시도:', err);
    }
  }
  // 2. writeValueWithResponse
  if (typeof char.writeValueWithResponse === 'function') {
    try {
      await char.writeValueWithResponse(bytes);
      return;
    } catch (err) {
      console.warn('writeValueWithResponse 실패, fallback 시도:', err);
    }
  }
  // 3. 기존 표준 writeValue
  await char.writeValue(bytes);
}

// USB 시리얼 연결
async function connectSerial() {
  if (!('serial' in navigator)) throw new Error('Web Serial 미지원');
  state.port = await navigator.serial.requestPort();
  await state.port.open({ baudRate: 9600 });
  state.writer = state.port.writable.getWriter();
  setConnection(true, 'USB 케이블 연결됨 ⚡');
  $('#transportLabel').textContent = 'USB 케이블 연결됨 (9600 baud)';
}

// BLE 무선 블루투스 연결
async function connectBle() {
  if (!('bluetooth' in navigator)) {
    throw new Error('이 브라우저는 Web Bluetooth를 지원하지 않습니다. Chrome 브라우저를 사용해 주세요.');
  }

  const SERVICE_UUIDS = [
    '0000ffe0-0000-1000-8000-00805f9b34fb', // HM-10 기본 서비스
    '0000fff0-0000-1000-8000-00805f9b34fb', // 일부 호환 모듈
    '6e400001-b5a3-f393-e0a9-e50e24dcca9e', // Nordic UART 서비스
  ];

  const device = await navigator.bluetooth.requestDevice({
    acceptAllDevices: true,
    optionalServices: SERVICE_UUIDS,
  });

  device.addEventListener('gattserverdisconnected', () => {
    setConnection(false);
    state.device = null;
    state.characteristic = null;
    $('#transportLabel').textContent = '블루투스 연결이 끊어졌습니다.';
  });

  const server = await device.gatt.connect();

  // 서비스 탐색
  let targetService = null;
  for (const uuid of SERVICE_UUIDS) {
    try {
      targetService = await server.getPrimaryService(uuid);
      if (targetService) break;
    } catch (_) {}
  }

  if (!targetService) {
    throw new Error('HM-10 블루투스 서비스를 찾지 못했습니다.');
  }

  // 특성(Characteristic) 탐색
  const CHAR_UUIDS = [
    '0000ffe1-0000-1000-8000-00805f9b34fb', // HM-10 데이터 송수신
    '0000fff1-0000-1000-8000-00805f9b34fb',
    '6e400002-b5a3-f393-e0a9-e50e24dcca9e', // Nordic RX
  ];

  let targetChar = null;
  for (const uuid of CHAR_UUIDS) {
    try {
      targetChar = await targetService.getCharacteristic(uuid);
      if (targetChar) break;
    } catch (_) {}
  }

  if (!targetChar) {
    throw new Error('블루투스 송수신 특성을 찾지 못했습니다.');
  }

  state.device = device;
  state.characteristic = targetChar;
  const devName = device.name || 'HM-10';
  setConnection(true, `${devName} 연결됨 ⚡`);
  $('#transportLabel').textContent = `블루투스 준비 완료 (${devName})`;
}

// 연결 해제
async function disconnect() {
  await send('S');
  if (state.writer) {
    try { state.writer.releaseLock(); } catch (_) {}
    state.writer = null;
  }
  if (state.port) {
    try { await state.port.close(); } catch (_) {}
    state.port = null;
  }
  if (state.device && state.device.gatt.connected) {
    try { state.device.gatt.disconnect(); } catch (_) {}
  }
  state.device = null;
  state.characteristic = null;
  setConnection(false);
  $('#transportLabel').textContent = '연결 해제됨';
}

// 연결 버튼 클릭 이벤트
$('#connectButton').addEventListener('click', async () => {
  if (state.connected) return disconnect();

  try {
    await connectBle();
  } catch (bleError) {
    if (bleError.name === 'NotFoundError') {
      return; // 취소함
    }
    try {
      await connectSerial();
    } catch (serialError) {
      if (serialError.name === 'NotFoundError') return;
      alert(`연결 실패: ${bleError.message || bleError}\n블루투스가 켜져 있는지 확인하고 다시 시도해 주세요.`);
    }
  }
});

// 터치 및 마우스 포인터 이벤트 (손가락 미끄러짐 방지용 Pointer Capture 적용)
document.querySelectorAll('.control-button').forEach((button) => {
  const command = button.dataset.command;

  const press = (event) => {
    event.preventDefault();
    button.classList.add('active');
    send(command);
  };

  const release = (event) => {
    event.preventDefault();
    if (button.classList.contains('active')) {
      button.classList.remove('active');
      if (command !== 'S') {
        send('S');
      }
    }
  };

  button.addEventListener('pointerdown', (event) => {
    try { button.setPointerCapture(event.pointerId); } catch (_) {}
    press(event);
  });

  button.addEventListener('pointerup', (event) => {
    try { button.releasePointerCapture(event.pointerId); } catch (_) {}
    release(event);
  });

  button.addEventListener('pointercancel', (event) => {
    try { button.releasePointerCapture(event.pointerId); } catch (_) {}
    release(event);
  });

  button.addEventListener('contextmenu', (e) => e.preventDefault());
});

// 키보드 조작 (WASD, 방향키, 스페이스바=정지)
const keyCommands = {
  ArrowUp: 'F', w: 'F', W: 'F',
  ArrowDown: 'B', s: 'B', S: 'B',
  ArrowLeft: 'L', a: 'L', A: 'L',
  ArrowRight: 'R', d: 'R', D: 'R',
  ' ': 'S',
};

const activeKeys = new Set();

window.addEventListener('keydown', (event) => {
  const command = keyCommands[event.key];
  if (!command || activeKeys.has(event.key)) return;
  event.preventDefault();
  activeKeys.add(event.key);
  send(command);
});

window.addEventListener('keyup', (event) => {
  if (!activeKeys.delete(event.key)) return;
  event.preventDefault();
  send('S');
});

$('#helpButton').addEventListener('click', () => $('#helpDialog').showModal());
$('#closeHelp').addEventListener('click', () => $('#helpDialog').close());
window.addEventListener('blur', () => send('S'));
