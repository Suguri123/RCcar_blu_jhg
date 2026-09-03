#include <SoftwareSerial.h>

/*
 * =========================================================
 * HM-10 블루투스 RC카 + 2채널 라인트레이싱 제어
 * =========================================================
 *
 * [블루투스 명령]
 * 1 : 전진
 * 2 : 후진
 * 3 : 좌회전
 * 4 : 우회전
 * 5 : 정지
 * 6 : 라인트레이싱 모드 시작
 * 7 : 라인트레이싱 모드 종료 (일반 조종으로 복귀)
 *
 * ---------------------------------------------------------
 * [HM-10 블루투스 연결]
 *
 * HM-10 TX  → Arduino D3 (RX)
 * HM-10 RX  ← Arduino D2 (TX)
 * HM-10 GND → Arduino GND
 * HM-10 VCC → Arduino 5V
 *
 * ※ SoftwareSerial(RX, TX)
 *
 * ---------------------------------------------------------
 * [L9110S 모터 드라이버]
 *
 * 왼쪽 A 모터
 * A-IA → Arduino D9
 * A-IB → Arduino D8
 *
 * 오른쪽 B 모터
 * B-IA → Arduino D5
 * B-IB → Arduino D4
 *
 * ---------------------------------------------------------
 * [2채널 라인센서]
 *
 * 왼쪽 센서   OUT → Arduino D11
 * 오른쪽 센서 OUT → Arduino D10
 *
 * [센서 감지 기준 (일반적인 적외선 센서 모듈)]
 * LOW  = 검은색 감지 (라인)
 * HIGH = 흰색 감지   (바닥)
 *
 * [2채널 자율주행 알고리즘]
 * 1. 왼쪽: 흰색(HIGH), 오른쪽: 흰색(HIGH) → 전진 (Forward)
 * 2. 왼쪽: 검은색(LOW), 오른쪽: 흰색(HIGH) → 좌회전 (Turn Left)
 * 3. 왼쪽: 흰색(HIGH), 오른쪽: 검은색(LOW) → 우회전 (Turn Right)
 * 4. 왼쪽: 검은색(LOW), 오른쪽: 검은색(LOW) → 정지 (Stop - 교차로/정지선)
 *
 * ---------------------------------------------------------
 * [주행 속도 설정]
 *
 * 수동 조작       : 150
 * 라인트레이싱    : 100
 *
 * =========================================================
 */


// =========================================================
// HM-10 블루투스 설정: SoftwareSerial(RX, TX)
// =========================================================
SoftwareSerial bluetooth(3, 2);


// =========================================================
// 모터 핀 설정
// =========================================================

// 왼쪽 A 모터
const int motorA_IA = 9;
const int motorA_IB = 8;

// 오른쪽 B 모터
const int motorB_IA = 5;
const int motorB_IB = 4;


// =========================================================
// 2채널 라인센서 핀 설정
// =========================================================
const int rightLineSensor = 10; // 오른쪽 라인센서 (D10)
const int leftLineSensor  = 11; // 왼쪽 라인센서   (D11)


// =========================================================
// 모터 속도 설정 (0 ~ 255)
// =========================================================

// 일반 수동 블루투스 조작 속도
const int driveSpeed = 150;

// 라인트레이싱 정밀 주행 속도
const int lineTraceSpeed = 100;


// =========================================================
// 라인트레이싱 모드 상태 변수
//
// false : 일반 블루투스 수동 조종
// true  : 센서 기반 라인트레이싱 자율 주행
// =========================================================
bool lineTracingMode = false;


// =========================================================
// 함수 원형 선언
// =========================================================
void forward();
void backward();
void turnLeft();
void turnRight();
void stopMotors();
void lineForward();
void lineTurnLeft();
void lineTurnRight();
void lineTracing();
void controlCommand(char command);


// =========================================================
// 초기 설정 (setup)
// =========================================================
void setup() {

  // 모터 핀 출력 설정
  pinMode(motorA_IA, OUTPUT);
  pinMode(motorA_IB, OUTPUT);

  pinMode(motorB_IA, OUTPUT);
  pinMode(motorB_IB, OUTPUT);

  // 2채널 라인센서 입력 설정
  pinMode(rightLineSensor, INPUT);
  pinMode(leftLineSensor, INPUT);

  // 전원을 켰을 때 모터 정지
  stopMotors();

  // PC 시리얼 모니터 (9600 baud)
  Serial.begin(9600);

  // HM-10 블루투스 (9600 baud)
  bluetooth.begin(9600);

  // 시리얼 모니터 안내 문구
  Serial.println("==========================================");
  Serial.println(" 2-Channel Line Tracing RC Car Ready");
  Serial.println("==========================================");
  Serial.println("1 : Forward   | 2 : Backward");
  Serial.println("3 : Left      | 4 : Right      | 5 : Stop");
  Serial.println("6 : Line Tracing START (Auto Run)");
  Serial.println("7 : Line Tracing STOP  (Manual Mode)");
  Serial.println("Sensors -> Left: D11 | Right: D10");
  Serial.println("==========================================");
}


// =========================================================
// 반복 실행 (loop)
// =========================================================
void loop() {

  // =======================================================
  // 1. HM-10 블루투스 명령 수신 처리
  // =======================================================
  if (bluetooth.available()) {

    char command = bluetooth.read();

    // 엔터 및 줄바꿈 문자는 무시
    if (command != '\r' && command != '\n') {

      Serial.print("[Bluetooth RX] ");
      Serial.println(command);

      controlCommand(command);
    }
  }


  // =======================================================
  // 2. PC 시리얼 모니터 명령 테스트 처리
  // =======================================================
  if (Serial.available()) {

    char command = Serial.read();

    if (command != '\r' && command != '\n') {

      Serial.print("[Serial RX] ");
      Serial.println(command);

      controlCommand(command);
    }
  }


  // =======================================================
  // 3. 6번 명령으로 라인트레이싱 모드가 활성화된 경우 자율 주행
  // =======================================================
  if (lineTracingMode == true) {

    lineTracing();
  }
}


// =========================================================
// 블루투스 명령 처리 함수
// =========================================================
void controlCommand(char command) {

  // =======================================================
  // 현재 라인트레이싱 모드인 경우
  // =======================================================
  if (lineTracingMode == true) {

    // 7을 눌렀을 때만 라인트레이싱 모드 종료
    if (command == '7') {

      lineTracingMode = false;

      stopMotors();

      Serial.println("============================");
      Serial.println(">>> LINE TRACING STOP");
      Serial.println(">>> MANUAL CONTROL MODE");
      Serial.println("============================");
    }

    // 라인트레이싱 중에는 1~6 및 다른 명령을 모두 무시
    else {

      Serial.println(">>> LINE TRACING MODE");
      Serial.println(">>> Command ignored");
      Serial.println(">>> Press 7 to exit");
    }

    return;
  }


  // =======================================================
  // 일반 블루투스 수동 조종 모드
  // =======================================================
  switch (command) {

    // -----------------------------------------------------
    // 1 : 전진
    // -----------------------------------------------------
    case '1':

      Serial.println(">>> FORWARD");

      forward();

      break;


    // -----------------------------------------------------
    // 2 : 후진
    // -----------------------------------------------------
    case '2':

      Serial.println(">>> BACKWARD");

      backward();

      break;


    // -----------------------------------------------------
    // 3 : 좌회전
    // -----------------------------------------------------
    case '3':

      Serial.println(">>> LEFT");

      turnLeft();

      break;


    // -----------------------------------------------------
    // 4 : 우회전
    // -----------------------------------------------------
    case '4':

      Serial.println(">>> RIGHT");

      turnRight();

      break;


    // -----------------------------------------------------
    // 5 : 정지
    // -----------------------------------------------------
    case '5':

      Serial.println(">>> STOP");

      stopMotors();

      break;


    // -----------------------------------------------------
    // 6 : 라인트레이싱 시작
    // -----------------------------------------------------
    case '6':

      Serial.println("============================");
      Serial.println(">>> LINE TRACING START");
      Serial.println(">>> SPEED : 100");
      Serial.println(">>> Press 7 to exit");
      Serial.println("============================");

      // 라인트레이싱 모드 활성화 -> loop()에서 센서 감지 주행 시작!
      lineTracingMode = true;

      break;


    // -----------------------------------------------------
    // 7 : 일반 모드에서는 정지
    // -----------------------------------------------------
    case '7':

      Serial.println(">>> MANUAL CONTROL MODE");

      stopMotors();

      break;


    // -----------------------------------------------------
    // 알 수 없는 명령
    // -----------------------------------------------------
    default:

      Serial.println(">>> UNKNOWN COMMAND");

      break;
  }
}


// =========================================================
// 2채널 라인트레이싱 자율 주행 로직
//
// 왼쪽 센서   (D11)
// 오른쪽 센서 (D10)
// LOW  = 검은색 감지
// HIGH = 흰색 감지
// =========================================================
void lineTracing() {

  // 양쪽 라인센서 값 읽기
  int leftVal  = digitalRead(leftLineSensor);
  int rightVal = digitalRead(rightLineSensor);


  // -------------------------------------------------------
  // 1. 둘 다 흰색 바닥 감지 → 정상 직진
  // -------------------------------------------------------
  if (leftVal == HIGH && rightVal == HIGH) {

    lineForward();

    Serial.println("[LINE] 직진 (흰색-흰색)");
  }

  // -------------------------------------------------------
  // 2. 왼쪽만 검은색 감지 → 차량이 오른쪽으로 벗어남 → 좌회전 보정
  // -------------------------------------------------------
  else if (leftVal == LOW && rightVal == HIGH) {

    lineTurnLeft();

    Serial.println("[LINE] 좌회전 (검은색-흰색)");
  }

  // -------------------------------------------------------
  // 3. 오른쪽만 검은색 감지 → 차량이 왼쪽으로 벗어남 → 우회전 보정
  // -------------------------------------------------------
  else if (leftVal == HIGH && rightVal == LOW) {

    lineTurnRight();

    Serial.println("[LINE] 우회전 (흰색-검은색)");
  }

  // -------------------------------------------------------
  // 4. 둘 다 검은색 감지 → 교차로 또는 정지선 도착 → 정지
  // -------------------------------------------------------
  else if (leftVal == LOW && rightVal == LOW) {

    stopMotors();

    Serial.println("[LINE] 정지 (교차로 / 정지선 도달)");
  }


  // 센서 감지 주기 (20ms)
  delay(20);
}


// =========================================================
// [일반 수동 모드] 모터 제어 함수 (속도: 150)
// =========================================================

// 전진
void forward() {

  // 왼쪽 A 모터 전진
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 전진
  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 후진
void backward() {

  // 왼쪽 A 모터 후진
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, HIGH);

  // 오른쪽 B 모터 후진
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, HIGH);
}

// 좌회전: 왼쪽 A 모터 정지, 오른쪽 B 모터 전진
void turnLeft() {

  // 왼쪽 A 모터 정지
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 전진
  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 우회전: 왼쪽 A 모터 전진, 오른쪽 B 모터 정지
void turnRight() {

  // 왼쪽 A 모터 전진
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// [라인트레이싱용] 모터 제어 함수 (속도: 100)
// =========================================================

// 라인트레이싱 전진
void lineForward() {

  // 왼쪽 A 모터 전진
  analogWrite(motorA_IA, lineTraceSpeed);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 전진
  analogWrite(motorB_IA, lineTraceSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 라인트레이싱 좌회전: 왼쪽 A모터 정지, 오른쪽 B모터 전진
void lineTurnLeft() {

  // 왼쪽 A 모터 정지
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 전진
  analogWrite(motorB_IA, lineTraceSpeed);
  digitalWrite(motorB_IB, LOW);
}

// 라인트레이싱 우회전: 왼쪽 A모터 전진, 오른쪽 B모터 정지
void lineTurnRight() {

  // 왼쪽 A 모터 전진
  analogWrite(motorA_IA, lineTraceSpeed);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 모터 완전 정지
// =========================================================
void stopMotors() {

  // 왼쪽 A 모터 정지
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  // 오른쪽 B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}
