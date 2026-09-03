#include <SoftwareSerial.h>

/*
 * =========================================================
 * 제주여고 로봇동아리 - 블루투스 & 라인트레이싱 RC카
 * =========================================================
 *
 * [블루투스 명령 프로토콜]
 * 1 : 전진
 * 2 : 후진
 * 3 : 좌회전
 * 4 : 우회전
 * 5 : 정지
 * 6 : 라인트레이싱 모드 시작
 * 7 : 라인트레이싱 모드 종료 (일반 조종으로 복귀)
 *
 * [라인트레이싱 동작 규칙]
 * - 6번 수신 시 라인트레이싱 모드 시작
 * - 라인트레이싱 중에는 1~6번 명령을 수신해도 모두 무시
 * - 7번을 수신해야만 모드가 종료되고 일반 블루투스 조종으로 복귀
 *
 * [오른쪽 라인센서(D10) 자율 주행 로직]
 * - 검은색 감지   → 우회전 (turnRight)
 * - 검은색 미감지 → 전진   (forward)
 *
 * ---------------------------------------------------------
 * [핀 연결 구성]
 *
 * 1. HM-10 블루투스
 *    - HM-10 TX  → Arduino D3 (RX)
 *    - HM-10 RX  ← Arduino D2 (TX)
 *    - HM-10 GND → Arduino GND
 *    - HM-10 VCC → Arduino 5V
 *
 * 2. L9110S 모터 드라이버
 *    - A 모터: A-IA → Arduino D9, A-IB → Arduino D8
 *    - B 모터: B-IA → Arduino D5, B-IB → Arduino D4
 *
 * 3. 오른쪽 라인센서 (디지털 적외선 센서)
 *    - OUT / DO  → Arduino D10
 *    - VCC       → Arduino 5V
 *    - GND       → Arduino GND
 * =========================================================
 */

// HM-10 블루투스 설정: SoftwareSerial(RX, TX)
SoftwareSerial bluetooth(3, 2);

// L9110S 모터 핀 설정
const int motorA_IA = 9;
const int motorA_IB = 8;
const int motorB_IA = 5;
const int motorB_IB = 4;

// 오른쪽 라인센서 핀 설정
const int lineSensorPin = 10;

// 모터 주행 속도 (0 ~ 255)
const int driveSpeed = 200;

// 적외선 센서 검은색 감지 신호 기준
// (대부분의 디지털 적외선 센서 모듈은 검은색에서 반사가 안 되어 HIGH를 출력합니다.
//  만약 사용하시는 센서가 검은색에서 LOW를 출력한다면 LOW로 변경해 주세요.)
const int BLACK_DETECTED = HIGH;

// 라인트레이싱 모드 활성화 여부
bool isLineTracing = false;

// 모터 제어 함수 선언
void forward();
void backward();
void turnLeft();
void turnRight();
void stopMotors();
void controlCommand(char command);
void handleLineTracing();

void setup() {
  // 모터 핀 출력 모드 설정
  pinMode(motorA_IA, OUTPUT);
  pinMode(motorA_IB, OUTPUT);
  pinMode(motorB_IA, OUTPUT);
  pinMode(motorB_IB, OUTPUT);

  // 라인센서 핀 입력 모드 설정
  pinMode(lineSensorPin, INPUT);

  // 초기 상태: 모터 정지
  stopMotors();

  // PC 시리얼 모니터 (9600 baud)
  Serial.begin(9600);

  // HM-10 블루투스 통신 (9600 baud)
  bluetooth.begin(9600);

  Serial.println("==========================================");
  Serial.println("  Bluetooth & Line-Tracing RC Car Ready");
  Serial.println("==========================================");
  Serial.println("1: Forward  | 2: Backward");
  Serial.println("3: Left     | 4: Right    | 5: Stop");
  Serial.println("6: Line-Tracing START");
  Serial.println("7: Line-Tracing STOP (Return to Manual)");
  Serial.println("Right Line Sensor Pin: D10");
  Serial.println("==========================================");
}

void loop() {
  // -------------------------------------------------------
  // 1. 블루투스(HM-10) 명령 수신 처리
  // -------------------------------------------------------
  if (bluetooth.available()) {
    char command = bluetooth.read();

    // 엔터 및 줄바꿈 문자 무시
    if (command != '\r' && command != '\n') {
      Serial.print("[Bluetooth RX] ");
      Serial.println(command);
      controlCommand(command);
    }
  }

  // -------------------------------------------------------
  // 2. PC 시리얼 모니터 테스트용 수신 처리
  // -------------------------------------------------------
  if (Serial.available()) {
    char command = Serial.read();

    if (command != '\r' && command != '\n') {
      Serial.print("[Serial RX] ");
      Serial.println(command);
      controlCommand(command);
    }
  }

  // -------------------------------------------------------
  // 3. 라인트레이싱 모드 실행 중일 때 센서 기반 자율 주행
  // -------------------------------------------------------
  if (isLineTracing) {
    handleLineTracing();
  }
}

// =========================================================
// 명령 처리 함수
// =========================================================
void controlCommand(char command) {
  // [핵심 요구사항] 라인트레이싱 중에는 1~6번 명령 무시, 오직 7번만 처리!
  if (isLineTracing) {
    if (command == '7') {
      isLineTracing = false;
      stopMotors();
      Serial.println(">>> [7] 라인트레이싱 모드 종료! 일반 블루투스 조종 복귀");
    } else {
      Serial.print(">>> 라인트레이싱 주행 중입니다. 명령 무시됨: ");
      Serial.println(command);
    }
    return;
  }

  // 일반 블루투스 수동 조종 모드
  switch (command) {
    case '1':
      Serial.println(">>> [1] 전진");
      forward();
      break;

    case '2':
      Serial.println(">>> [2] 후진");
      backward();
      break;

    case '3':
      Serial.println(">>> [3] 좌회전");
      turnLeft();
      break;

    case '4':
      Serial.println(">>> [4] 우회전");
      turnRight();
      break;

    case '5':
      Serial.println(">>> [5] 정지");
      stopMotors();
      break;

    case '6':
      isLineTracing = true;
      Serial.println(">>> [6] 라인트레이싱 모드 시작!");
      break;

    case '7':
      // 이미 정지 상태일 때 7을 받아도 안전하게 정지 유지
      stopMotors();
      Serial.println(">>> [7] 라인트레이싱 모드 종료 (이미 일반 모드)");
      break;

    default:
      Serial.print(">>> 알 수 없는 명령: ");
      Serial.println(command);
      break;
  }
}

// =========================================================
// 오른쪽 라인센서 자율주행 알고리즘
// 검은색 감지   → 우회전 (turnRight)
// 검은색 미감지 → 전진   (forward)
// =========================================================
void handleLineTracing() {
  int sensorValue = digitalRead(lineSensorPin);

  if (sensorValue == BLACK_DETECTED) {
    // 검은색 감지 → 우회전
    turnRight();
  } else {
    // 검은색 미감지 → 전진
    forward();
  }
}

// =========================================================
// 모터 제어 기본 동작 함수
// =========================================================

// 전진: 양쪽 모터 전진 회전
void forward() {
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 후진: 양쪽 모터 후진 회전
void backward() {
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, HIGH);

  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, HIGH);
}

// 좌회전: 오른쪽 모터 전진 + 왼쪽 모터 정지 (또는 역회전)
void turnLeft() {
  // A 모터 정지, B 모터 전진
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 우회전: 왼쪽 모터 전진 + 오른쪽 모터 정지 (또는 역회전)
void turnRight() {
  // A 모터 전진, B 모터 정지
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}

// 정지: 양쪽 모터 정지
void stopMotors() {
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}
