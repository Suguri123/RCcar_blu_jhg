const state = { port: null, writer: null, characteristic: null, connected: false };
const $ = (selector) => document.querySelector(selector);

function setConnection(connected, name = '연결 안 됨') {
  state.connected = connected;
  $('#statusDot').classList.toggle('connected', connected);
  $('#connectionState').textContent = connected ? name : '연결 안 됨';
  $('#connectButton').innerHTML = connected ? '연결 끊기 ❌' : '연결하기 🔌';
}

async function send(command) {
  const directionMap = { 'F': '전진 🔺', 'B': '후진 🔻', 'L': '좌회전 ◀', 'R': '우회전 ▶', 'S': '정지 ■' };
  $('#commandReadout').textContent = `${command === 'S' ? '대기 중' : '운전 중'} / ${directionMap[command] || command}`;
  try {
    const bytes = new TextEncoder().encode(command);
    if (state.writer) await state.writer.write(bytes);
    else if (state.characteristic) await state.characteristic.writeValue(bytes);
  } catch (error) { console.warn('명령 전송 실패:', error); }
}

async function connectSerial() {
  if (!('serial' in navigator)) throw new Error('이 브라우저는 Web Serial을 지원하지 않습니다.');
  state.port = await navigator.serial.requestPort();
  await state.port.open({ baudRate: 9600 });
  state.writer = state.port.writable.getWriter();
  setConnection(true, 'RC카 연결됨 ⚡');
}

async function connectBle() {
  if (!('bluetooth' in navigator)) throw new Error('Web Bluetooth를 지원하지 않습니다.');
  const device = await navigator.bluetooth.requestDevice({ acceptAllDevices: true, optionalServices: ['0000ffe0-0000-1000-8000-00805f9b34fb'] });
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService('0000ffe0-0000-1000-8000-00805f9b34fb');
  state.characteristic = await service.getCharacteristic('0000ffe1-0000-1000-8000-00805f9b34fb');
  setConnection(true, device.name || 'RC카 연결됨 ⚡');
}

async function disconnect() {
  await send('S');
  if (state.writer) { state.writer.releaseLock(); state.writer = null; }
  if (state.port) { await state.port.close(); state.port = null; }
  state.characteristic = null; setConnection(false); send('S');
}

$('#connectButton').addEventListener('click', async () => {
  if (state.connected) return disconnect();
  try { await connectSerial(); $('#transportLabel').textContent = 'USB 케이블 연결됨'; }
  catch (serialError) {
    try { await connectBle(); $('#transportLabel').textContent = '블루투스 무선 연결됨'; }
    catch (bleError) { alert('RC카와 연결할 수 없어요. 블루투스가 켜져 있는지 확인하고 다시 시도해 주세요! 😢'); }
  }
});

document.querySelectorAll('.control-button').forEach((button) => {
  const command = button.dataset.command;
  const press = (event) => { event.preventDefault(); button.classList.add('active'); send(command); };
  const release = (event) => { event.preventDefault(); button.classList.remove('active'); if (command !== 'S') send('S'); };
  button.addEventListener('pointerdown', press);
  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('pointerleave', (event) => { if (button.classList.contains('active')) release(event); });
});

const keyCommands = { ArrowUp: 'F', w: 'F', W: 'F', ArrowDown: 'B', s: 'B', S: 'B', ArrowLeft: 'L', a: 'L', A: 'L', ArrowRight: 'R', d: 'R', D: 'R' };
const activeKeys = new Set();
window.addEventListener('keydown', (event) => {
  const command = keyCommands[event.key];
  if (!command || activeKeys.has(event.key)) return;
  event.preventDefault(); activeKeys.add(event.key); send(command);
});
window.addEventListener('keyup', (event) => {
  if (!activeKeys.delete(event.key)) return;
  event.preventDefault(); send('S');
});

$('#helpButton').addEventListener('click', () => $('#helpDialog').showModal());
$('#closeHelp').addEventListener('click', () => $('#helpDialog').close());
window.addEventListener('blur', () => send('S'));
