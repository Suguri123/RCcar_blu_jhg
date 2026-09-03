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
  const directionMap = {
    '1': '전진 🔺',
    '2': '후진 🔻',
    '3': '좌회전 ◀',
    '4': '우회전 ▶',
    '5': '정지 ■',
    '6': '라인트레이싱 시작 〰️',
    '7': '라인트레이싱 종료 ⏹️',
  };

  if (command === '6') {
    $('#commandReadout').textContent = '라인트레이싱 주행 중 〰️';
  } else if (command === '5' || command === '7') {
    $('#commandReadout').textContent = '대기 중 / 정지';
  } else {
    $('#commandReadout').textContent = `운전 중 / ${directionMap[command] || command}`;
  }

  if (!state.connected) {
    $('#transportLabel').textContent = '블루투스를 먼저 연결해 주세요!';
    return;
  }

  // 정지(5) 및 라인트레이싱 종료(7) 명령은 이전 큐를 덮어쓰고 최우선 처리
  if (command === '5' || command === '7') {
    queuedCommand = null;
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
      $('#transportLabel').textContent = `전송 완료: ${command} (${directionMap[command] || ''})`;
    } else if (state.characteristic) {
      await writeCharacteristic(state.characteristic, bytes);
      $('#transportLabel').textContent = `전송 완료: ${command} (${directionMap[command] || ''})`;
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
    document.querySelectorAll('.control-button').forEach(btn => btn.classList.remove('active'));
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

// 라인트레이싱 모드 상태
let isLineTracing = false;

async function startLineTracing() {
  if (!state.connected) {
    alert('블루투스를 먼저 연결해 주세요! 🔌');
    return;
  }
  isLineTracing = true;
  $('#lineStartButton')?.classList.add('active');
  $('#commandReadout').textContent = '라인트레이싱 주행 중 〰️';
  $('#transportLabel').textContent = '라인트레이싱 모드 동작 중 (종료: 라인종료 버튼)';
  await send('6');
}

async function stopLineTracing() {
  if (!state.connected) return;
  isLineTracing = false;
  $('#lineStartButton')?.classList.remove('active');
  const stopBtn = $('#lineStopButton');
  if (stopBtn) {
    stopBtn.classList.add('active');
    setTimeout(() => stopBtn.classList.remove('active'), 250);
  }
  $('#commandReadout').textContent = '대기 중 / 정지';
  $('#transportLabel').textContent = '라인트레이싱 종료 -> 일반 조종 복귀';
  await send('7');
}

// 연결 해제
async function disconnect() {
  if (isLineTracing) {
    await stopLineTracing();
  } else {
    await send('5');
  }

  document.querySelectorAll('.control-button').forEach((btn) => btn.classList.remove('active'));
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

// 라인트레이싱 시작 버튼 (전진 오른쪽)
$('#lineStartButton')?.addEventListener('click', (event) => {
  event.preventDefault();
  startLineTracing();
});

// 라인트레이싱 종료 버튼 (우회전 아래쪽)
$('#lineStopButton')?.addEventListener('click', (event) => {
  event.preventDefault();
  stopLineTracing();
});

// 방향 조종 버튼 누름 제어 (누르고 있는 동안만 움직이고, 떼면 멈춤)
document.querySelectorAll('.control-button[data-command]').forEach((button) => {
  const command = button.dataset.command;

  const press = (event) => {
    event.preventDefault();

    // 라인트레이싱 중에는 1~6 무시됨
    if (isLineTracing) {
      if (command === '5') {
        // 정지 버튼을 누르면 안전하게 7(라인종료) 전송
        stopLineTracing();
      } else {
        $('#transportLabel').textContent = '라인트레이싱 중입니다! (종료하려면 라인종료 버튼 클릭)';
      }
      return;
    }

    try { button.setPointerCapture(event.pointerId); } catch (_) {}
    button.classList.add('active');
    send(command);
  };

  const release = (event) => {
    event.preventDefault();
    if (isLineTracing) return;

    try { button.releasePointerCapture(event.pointerId); } catch (_) {}
    if (button.classList.contains('active')) {
      button.classList.remove('active');
      if (command !== '5') {
        send('5');
      }
    }
  };

  button.addEventListener('pointerdown', press);
  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('contextmenu', (e) => e.preventDefault());
});

// 키보드 조작 (누르고 있는 동안 이동, 떼면 정지 / 6: 라인시작, 7: 라인종료)
const keyCommands = {
  ArrowUp: '1', w: '1', W: '1',
  ArrowDown: '2', s: '2', S: '2',
  ArrowLeft: '3', a: '3', A: '3',
  ArrowRight: '4', d: '4', D: '4',
  ' ': '5',
};

const activeKeys = new Set();

window.addEventListener('keydown', (event) => {
  if (event.key === '6') {
    event.preventDefault();
    if (!isLineTracing) startLineTracing();
    return;
  }
  if (event.key === '7' || event.key === 'Escape') {
    event.preventDefault();
    if (isLineTracing) stopLineTracing();
    return;
  }

  const command = keyCommands[event.key];
  if (!command || activeKeys.has(event.key)) return;
  event.preventDefault();

  if (isLineTracing) {
    if (command === '5') stopLineTracing();
    return;
  }

  activeKeys.add(event.key);
  send(command);
});

window.addEventListener('keyup', (event) => {
  if (isLineTracing) return;
  if (!activeKeys.delete(event.key)) return;
  event.preventDefault();
  send('5');
});

$('#helpButton').addEventListener('click', () => $('#helpDialog').showModal());
$('#closeHelp').addEventListener('click', () => $('#helpDialog').close());
window.addEventListener('blur', () => {
  if (isLineTracing) {
    stopLineTracing();
  } else {
    send('5');
  }
});
