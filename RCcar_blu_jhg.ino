#include <SoftwareSerial.h>

/*
 * =========================================================
 * HM-10 블루투스 RC카 + 2센서 라인트레이싱
 * =========================================================
 *
 * [블루투스 명령]
 * 1 : 전진
 * 2 : 후진
 * 3 : 좌회전
 * 4 : 우회전
 * 5 : 정지
 * 6 : 라인트레이싱 시작
 * 7 : 라인트레이싱 종료
 *
 * ---------------------------------------------------------
 * [HM-10]
 *
 * HM-10 TX → Arduino D3 (RX)
 * HM-10 RX → Arduino D2 (TX)
 *
 * ---------------------------------------------------------
 * [L9110S 모터]
 *
 * A-IA → D9
 * A-IB → D8
 *
 * B-IA → D5
 * B-IB → D4
 *
 * ---------------------------------------------------------
 * [라인센서]
 *
 * 오른쪽 센서 → D10
 * 왼쪽 센서   → D11
 *
 * 검은색 감지 = LOW 기준
 *
 * 왼쪽 검은색  → 좌회전
 * 오른쪽 검은색 → 우회전
 * 양쪽 흰색    → 전진
 * 양쪽 검은색  → 전진
 *
 * ---------------------------------------------------------
 * [속도]
 *
 * 일반 조작       : 150
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
// 라인센서 핀
// =========================================================

// 오른쪽 라인센서
const int rightLineSensor = 10;

// 왼쪽 라인센서
const int leftLineSensor = 11;


// =========================================================
// 속도 설정
// =========================================================

// 일반 블루투스 조작 속도
const int driveSpeed = 150;

// 라인트레이싱 속도
const int lineTraceSpeed = 80;


// =========================================================
// 라인트레이싱 모드
// false : 일반 조작
// true  : 라인트레이싱
// =========================================================
bool lineTracingMode = false;


// =========================================================
// 초기 설정
// =========================================================
void setup() {

  // 모터 출력
  pinMode(motorA_IA, OUTPUT);
  pinMode(motorA_IB, OUTPUT);

  pinMode(motorB_IA, OUTPUT);
  pinMode(motorB_IB, OUTPUT);


  // 라인센서 입력
  pinMode(rightLineSensor, INPUT);
  pinMode(leftLineSensor, INPUT);


  // 시작 시 정지
  stopMotors();


  // 시리얼 모니터
  Serial.begin(9600);


  // HM-10
  bluetooth.begin(9600);


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
  // HM-10 블루투스 명령
  // =======================================================
  if (bluetooth.available()) {

    char command = bluetooth.read();


    if (command != '\r' && command != '\n') {

      Serial.print("[Bluetooth RX] ");
      Serial.println(command);

      controlCommand(command);
    }
  }


  // =======================================================
  // 시리얼 모니터 테스트
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
  // 라인트레이싱 실행
  // =======================================================
  if (lineTracingMode == true) {

    lineTracing();
  }
}


// =========================================================
// 명령 처리
// =========================================================
void controlCommand(char command) {


  // =======================================================
  // 라인트레이싱 모드
  // =======================================================
  if (lineTracingMode == true) {


    // 7번만 라인트레이싱 종료 가능
    if (command == '7') {

      lineTracingMode = false;

      stopMotors();

      Serial.println("============================");
      Serial.println(">>> LINE TRACING STOP");
      Serial.println(">>> MANUAL CONTROL MODE");
      Serial.println("============================");
    }


    // 다른 명령은 모두 무시
    else {

      Serial.println(">>> LINE TRACING MODE");
      Serial.println(">>> Command ignored");
      Serial.println(">>> Press 7 to exit");
    }


    return;
  }


  // =======================================================
  // 일반 블루투스 조작
  // =======================================================
  switch (command) {


    // 1 : 전진
    case '1':

      Serial.println(">>> FORWARD");

      forward();

      break;


    // 2 : 후진
    case '2':

      Serial.println(">>> BACKWARD");

      backward();

      break;


    // 3 : 좌회전
    case '3':

      Serial.println(">>> LEFT");

      turnLeft();

      break;


    // 4 : 우회전
    case '4':

      Serial.println(">>> RIGHT");

      turnRight();

      break;


    // 5 : 정지
    case '5':

      Serial.println(">>> STOP");

      stopMotors();

      break;


    // 6 : 라인트레이싱 시작
    case '6':

      Serial.println("============================");
      Serial.println(">>> LINE TRACING START");
      Serial.println(">>> SPEED : 100");
      Serial.println(">>> Press 7 to exit");
      Serial.println("============================");

      lineTracingMode = true;

      break;


    // 7
    case '7':

      stopMotors();

      Serial.println(">>> MANUAL CONTROL MODE");

      break;


    default:

      Serial.println(">>> UNKNOWN COMMAND");

      break;
  }
}


// =========================================================
// 라인트레이싱
// =========================================================
void lineTracing() {


  // 센서 값 읽기
  int leftSensorValue =
    digitalRead(leftLineSensor);

  int rightSensorValue =
    digitalRead(rightLineSensor);


  // =======================================================
  // 왼쪽 센서만 검은색
  // → 왼쪽으로 회전
  // =======================================================
  if (leftSensorValue == LOW &&
      rightSensorValue == HIGH) {

    lineTurnLeft();

    Serial.println(
      "[LINE] LEFT BLACK -> LEFT"
    );
  }


  // =======================================================
  // 오른쪽 센서만 검은색
  // → 오른쪽으로 회전
  // =======================================================
  else if (leftSensorValue == HIGH &&
           rightSensorValue == LOW) {

    lineTurnRight();

    Serial.println(
      "[LINE] RIGHT BLACK -> RIGHT"
    );
  }


  // =======================================================
  // 양쪽 모두 흰색
  // → 전진
  // =======================================================
  else if (leftSensorValue == HIGH &&
           rightSensorValue == HIGH) {

    lineForward();

    Serial.println(
      "[LINE] WHITE / WHITE -> FORWARD"
    );
  }


  // =======================================================
  // 양쪽 모두 검은색
  // → 전진
  // =======================================================
  else {

    lineForward();

    Serial.println(
      "[LINE] BLACK / BLACK -> FORWARD"
    );
  }


  delay(30);
}


// =========================================================
// 일반 모드 전진
// 속도 150
// =========================================================
void forward() {

  // 왼쪽 A 모터
  analogWrite(
    motorA_IA,
    driveSpeed
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 B 모터
  analogWrite(
    motorB_IA,
    driveSpeed
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 일반 모드 후진
// =========================================================
void backward() {

  // A 모터
  digitalWrite(
    motorA_IA,
    LOW
  );

  digitalWrite(
    motorA_IB,
    HIGH
  );


  // B 모터
  digitalWrite(
    motorB_IA,
    LOW
  );

  digitalWrite(
    motorB_IB,
    HIGH
  );
}


// =========================================================
// 일반 모드 좌회전
// =========================================================
void turnLeft() {

  // 왼쪽 A 모터 정지
  digitalWrite(
    motorA_IA,
    LOW
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 B 모터 전진
  analogWrite(
    motorB_IA,
    driveSpeed
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 일반 모드 우회전
// =========================================================
void turnRight() {

  // 왼쪽 A 모터 전진
  analogWrite(
    motorA_IA,
    driveSpeed
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 B 모터 정지
  digitalWrite(
    motorB_IA,
    LOW
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 라인트레이싱 전진
// 속도 100
// =========================================================
void lineForward() {

  // 왼쪽 모터
  analogWrite(
    motorA_IA,
    lineTraceSpeed
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 모터
  analogWrite(
    motorB_IA,
    lineTraceSpeed
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 라인트레이싱 좌회전
//
// 왼쪽 모터 정지
// 오른쪽 모터 전진
// =========================================================
void lineTurnLeft() {

  // 왼쪽 A 모터 정지
  digitalWrite(
    motorA_IA,
    LOW
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 B 모터 전진
  analogWrite(
    motorB_IA,
    lineTraceSpeed
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 라인트레이싱 우회전
//
// 왼쪽 모터 전진
// 오른쪽 모터 정지
// =========================================================
void lineTurnRight() {

  // 왼쪽 A 모터 전진
  analogWrite(
    motorA_IA,
    lineTraceSpeed
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // 오른쪽 B 모터 정지
  digitalWrite(
    motorB_IA,
    LOW
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}


// =========================================================
// 모터 정지
// =========================================================
void stopMotors() {

  // A 모터
  digitalWrite(
    motorA_IA,
    LOW
  );

  digitalWrite(
    motorA_IB,
    LOW
  );


  // B 모터
  digitalWrite(
    motorB_IA,
    LOW
  );

  digitalWrite(
    motorB_IB,
    LOW
  );
}
