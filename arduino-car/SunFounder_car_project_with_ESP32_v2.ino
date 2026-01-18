#define DECODE_NEC
#define triggerPin 7
#define echoPin 4

#include <IRremote.hpp>

// DEBUG & STATUS
unsigned long lastStatus = 0;
unsigned long lastPrint = 0;

// MOTOR
int motorSpeed = 200;
String receivedString = "";

const int A_BACK = 10;
const int A_FWD  = 5;
const int B_BACK = 6;
const int B_FWD  = 9;

// IR PROXIMITY
const int IR_LEFT = 8;
const int IR_RIGHT = 12;

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(11, true);

  // MOTOR pins
  pinMode(A_BACK, OUTPUT);
  pinMode(A_FWD, OUTPUT);
  pinMode(B_BACK, OUTPUT);
  pinMode(B_FWD, OUTPUT);

  // ULTRASONIC pins
  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // IR PROXIMITY pins
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  stopMotor();
}

void loop() {
  receiveUIData();

  if (IrReceiver.decode()) {
    switch (IrReceiver.decodedIRData.command) {
      case 0x18: forwards(motorSpeed); break;
      case 0x52: backwards(motorSpeed); break;
      case 0x08: turningLeft(motorSpeed); break;
      case 0x5A: turningRight(motorSpeed); break;
      case 0x1C: stopMotor(); break;

      case 0x09: // plus
        motorSpeed += 10;
        motorSpeed = constrain(motorSpeed, 0, 255);
        Serial.println(motorSpeed);
        break;

      case 0x15: // minus
        motorSpeed -= 10;
        motorSpeed = constrain(motorSpeed, 0, 255);
        Serial.println(motorSpeed);
        break;
    }
    IrReceiver.resume();
  }

  // Read distance to obstacle with ULTRASONIC sensor
  float distance;
  readUltrasonicSensor(distance);

  // Read IR proximity sensor
  bool leftIRSensor, rightIRSensor;
  readIRProximitySensor(leftIRSensor, rightIRSensor);

  if (millis() - lastStatus >= 100) {
    lastStatus = millis();
    sendObstacleStatus(leftIRSensor, distance, rightIRSensor);
  }

  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    Serial.print("L=");
    Serial.print(leftIRSensor);
    Serial.print(leftIRSensor ? " (clear)" : " (obstacle)");
    Serial.print("  R=");
    Serial.print(rightIRSensor);
    Serial.print(rightIRSensor ? " (clear)" : " (obstacle)");
    Serial.print("  D=");
    Serial.println(distance);
  }

/*
  if (distance < 8) {
  backwards(motorSpeed);
  delay(300);
  turningLeft(motorSpeed);
  delay(300);
  stopMotor();
  } else if (!leftIRSensor) {
    turningRight(motorSpeed);
    delay(500);
    forwards(motorSpeed);
  } else if (!rightIRSensor) {
    turningLeft(motorSpeed);
    delay(500);
    forwards(motorSpeed);
  }
*/
  delay(10); // 10
}

/*
|----------------------|
| CAR & TOOL FUNCTIONS |
|----------------------|
*/

void receiveUIData() {
  if (Serial.available() > 0) {
    char receivedByte = Serial.read();

    if (receivedByte == '\n') {
      //Serial.println(receivedString);
      // Logic to control car
      if (receivedString == "F") {
        forwards(motorSpeed);
      } else if (receivedString == "B") {
        backwards(motorSpeed);
      } else if (receivedString == "L") {
        turningLeft(motorSpeed);
      } else if (receivedString == "R") {
        turningRight(motorSpeed);
      } else if (receivedString == "S") {
        stopMotor();
      }
      receivedString = "";
    } else {
      receivedString += receivedByte;
    }
  }
}

// MOTORS
void forwards(int speed) {
  digitalWrite(A_BACK, LOW);
  analogWrite(A_FWD, speed);

  digitalWrite(B_BACK, LOW);
  analogWrite(B_FWD, speed);
}

void backwards(int speed) {
  analogWrite(A_BACK, speed);
  digitalWrite(A_FWD, LOW);

  analogWrite(B_BACK, speed);
  digitalWrite(B_FWD, LOW);
}

void turningLeft(int speed) {
  digitalWrite(A_BACK, LOW);
  analogWrite(A_FWD, speed);

  analogWrite(B_BACK, speed);
  digitalWrite(B_FWD, LOW);
}

void turningRight(int speed) {
  analogWrite(A_BACK, speed);
  digitalWrite(A_FWD, LOW);

  digitalWrite(B_BACK, LOW);
  analogWrite(B_FWD, speed);
}

void stopMotor() {
  digitalWrite(A_BACK, LOW);
  digitalWrite(A_FWD, LOW);
  digitalWrite(B_BACK, LOW);
  digitalWrite(B_FWD, LOW);
}

// ULTRASONIC
float readDistance() {
  float duration, distance;

  // Sending pulse from ULTRASONIC sensor
  digitalWrite(triggerPin, LOW); // Ensure TRIG is "LOW" (clean start)
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH); // Set TRIG "HIGH" for 10 microseconds (see next line)
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW); // Set TRIG "LOW" again

  // Receiving pulse form ULTRASONIC sensor (pulseIn() is doing the timing) and distance conversion to cm/microsecond
  duration = pulseIn(echoPin, HIGH, 30000UL); // 30ms timeout (~5m max range)
  if (duration == 0) {
    return 999.0; // no echo -> assume far away, NOT obstacle
  }
  // 343 m/s -> 0.0343 cm/microsecond
  // devision by 2 is necessary, if only the distance to the measured obstactle is of interest
  distance = ((duration * 0.0343) / 2); 
  return distance;
}

// The param outDistance looks to the address of the variable put into function call (inside loop()) 
void readUltrasonicSensor(float &outDistance) {
  outDistance = readDistance();
}

// INFRARED
void readIRProximitySensor(bool &outLeftIRSensor, bool &outRightIRSensor) {
  outLeftIRSensor = digitalRead(IR_LEFT);
  outRightIRSensor = digitalRead(IR_RIGHT);
}

// OBSTACLE STATUS
void sendObstacleStatus(bool leftIR, float distanceCm, bool rightIR) {
  bool centerObstacle = (distanceCm < 8);

  char s0 = leftIR ? '1' : '0';
  char s1 = centerObstacle ? '0' : '1';
  char s2 = rightIR ? '1' : '0';

  Serial.print("S:");
  Serial.print(s0);
  Serial.print(s1);
  Serial.print(s2);
  Serial.print(',');
  Serial.println(distanceCm, 1);
}