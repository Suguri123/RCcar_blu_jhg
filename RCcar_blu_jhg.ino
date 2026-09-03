#include <SoftwareSerial.h>

/*
 * =========================================================
 * HM-10 블루투스 RC카 + 라인트레이싱 제어
 * =========================================================
 *
 * [블루투스 명령]
 * 1 : 전진
 * 2 : 후진
 * 3 : 좌회전
 * 4 : 우회전
 * 5 : 정지
 * 6 : 라인트레이싱 모드 시작
 * 7 : 라인트레이싱 모드 종료
 *
 * ---------------------------------------------------------
 * [HM-10 연결]
 *
 * HM-10 TX  → Arduino D3 (RX)
 * HM-10 RX  ← Arduino D2 (TX)
 * HM-10 GND → Arduino GND
 *
 * ※ SoftwareSerial(RX, TX)
 *
 * ---------------------------------------------------------
 * [L9110S 모터 드라이버]
 *
 * A 모터
 * A-IA → Arduino D9
 * A-IB → Arduino D8
 *
 * B 모터
 * B-IA → Arduino D5
 * B-IB → Arduino D4
 *
 * ---------------------------------------------------------
 * [라인센서]
 *
 * 오른쪽 라인센서 OUT → Arduino D10
 *
 * 검은색 감지 → 우회전
 * 흰색 감지   → 전진
 *
 * ---------------------------------------------------------
 * [속도]
 *
 * 수동 조작       : 150
 * 라인트레이싱    : 100
 *
 * =========================================================
 */


// =========================================================
// HM-10 블루투스
// SoftwareSerial(RX, TX)
// =========================================================
SoftwareSerial bluetooth(3, 2);


// =========================================================
// 모터 핀
// =========================================================

// 왼쪽 A 모터
const int motorA_IA = 9;
const int motorA_IB = 8;

// 오른쪽 B 모터
const int motorB_IA = 5;
const int motorB_IB = 4;


// =========================================================
// 오른쪽 라인센서
// =========================================================
const int rightLineSensor = 10;


// =========================================================
// 모터 속도 설정
// =========================================================

// 일반 블루투스 조작 속도
const int driveSpeed = 150;

// 라인트레이싱 속도
const int lineTraceSpeed = 100;


// =========================================================
// 라인트레이싱 모드 상태
//
// false : 일반 블루투스 조종
// true  : 라인트레이싱
// =========================================================
bool lineTracingMode = false;


// =========================================================
// 초기 설정
// =========================================================
void setup() {

  // 모터 핀 출력 설정
  pinMode(motorA_IA, OUTPUT);
  pinMode(motorA_IB, OUTPUT);

  pinMode(motorB_IA, OUTPUT);
  pinMode(motorB_IB, OUTPUT);

  // 라인센서 입력 설정
  pinMode(rightLineSensor, INPUT);

  // 전원을 켰을 때 모터 정지
  stopMotors();

  // PC 시리얼 모니터
  Serial.begin(9600);

  // HM-10 블루투스
  bluetooth.begin(9600);


  // 시리얼 모니터 안내
  Serial.println("============================");
  Serial.println(" Bluetooth RC Car Ready");
  Serial.println("============================");
  Serial.println("1 : Forward");
  Serial.println("2 : Backward");
  Serial.println("3 : Left");
  Serial.println("4 : Right");
  Serial.println("5 : Stop");
  Serial.println("6 : Line Tracing START");
  Serial.println("7 : Line Tracing STOP");
  Serial.println("============================");
}


// =========================================================
// 반복 실행
// =========================================================
void loop() {

  // =======================================================
  // HM-10 블루투스 명령 수신
  // =======================================================
  if (bluetooth.available()) {

    char command = bluetooth.read();

    // 엔터, 줄바꿈 문자는 무시
    if (command != '\r' && command != '\n') {

      Serial.print("[Bluetooth RX] ");
      Serial.println(command);

      controlCommand(command);
    }
  }


  // =======================================================
  // 시리얼 모니터에서도 명령 테스트 가능
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
  // 라인트레이싱 모드 실행
  // =======================================================
  if (lineTracingMode == true) {

    lineTracing();
  }
}


// =========================================================
// 블루투스 명령 처리
// =========================================================
void controlCommand(char command) {

  // =======================================================
  // 현재 라인트레이싱 모드인 경우
  // =======================================================
  if (lineTracingMode == true) {

    // 7을 눌렀을 때만 라인트레이싱 종료
    if (command == '7') {

      lineTracingMode = false;

      stopMotors();

      Serial.println("============================");
      Serial.println(">>> LINE TRACING STOP");
      Serial.println(">>> MANUAL CONTROL MODE");
      Serial.println("============================");
    }

    // 라인트레이싱 중에는
    // 1~6 및 다른 명령을 모두 무시
    else {

      Serial.println(">>> LINE TRACING MODE");
      Serial.println(">>> Command ignored");
      Serial.println(">>> Press 7 to exit");
    }

    return;
  }


  // =======================================================
  // 일반 블루투스 조종 모드
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

      // 라인트레이싱 모드 활성화
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
// 라인트레이싱
// =========================================================
void lineTracing() {

  // 오른쪽 라인센서 값 읽기
  int rightSensorValue = digitalRead(rightLineSensor);


  // -------------------------------------------------------
  // 검은색 감지
  //
  // 일반적인 라인센서 기준
  // LOW = 검은색
  // -------------------------------------------------------
  if (rightSensorValue == LOW) {

    // 검은색을 만나면
    // 속도 100으로 오른쪽 이동
    lineTurnRight();

    Serial.println("[LINE] BLACK -> RIGHT");
  }


  // -------------------------------------------------------
  // 흰색 감지
  // -------------------------------------------------------
  else {

    // 속도 100으로 전진
    lineForward();

    Serial.println("[LINE] WHITE -> FORWARD");
  }


  // 센서 확인 주기
  delay(30);
}


// =========================================================
// 일반 모드 전진
// 속도 : 150
// =========================================================
void forward() {

  // A 모터 전진
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  // B 모터 전진
  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 일반 모드 후진
// =========================================================
void backward() {

  // A 모터 후진
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, HIGH);

  // B 모터 후진
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, HIGH);
}


// =========================================================
// 일반 모드 좌회전
//
// 왼쪽 A 모터 정지
// 오른쪽 B 모터 전진
// =========================================================
void turnLeft() {

  // A 모터 정지
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  // B 모터 전진
  analogWrite(motorB_IA, driveSpeed);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 일반 모드 우회전
//
// 왼쪽 A 모터 전진
// 오른쪽 B 모터 정지
// =========================================================
void turnRight() {

  // A 모터 전진
  analogWrite(motorA_IA, driveSpeed);
  digitalWrite(motorA_IB, LOW);

  // B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 라인트레이싱용 전진
// 속도 : 100
// =========================================================
void lineForward() {

  // A 모터 전진
  analogWrite(motorA_IA, lineTraceSpeed);
  digitalWrite(motorA_IB, LOW);

  // B 모터 전진
  analogWrite(motorB_IA, lineTraceSpeed);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 라인트레이싱용 우회전
// 속도 : 100
//
// 왼쪽 A 모터 전진
// 오른쪽 B 모터 정지
// =========================================================
void lineTurnRight() {

  // A 모터 전진
  analogWrite(motorA_IA, lineTraceSpeed);
  digitalWrite(motorA_IB, LOW);

  // B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}


// =========================================================
// 모터 정지
// =========================================================
void stopMotors() {

  // A 모터 정지
  digitalWrite(motorA_IA, LOW);
  digitalWrite(motorA_IB, LOW);

  // B 모터 정지
  digitalWrite(motorB_IA, LOW);
  digitalWrite(motorB_IB, LOW);
}
