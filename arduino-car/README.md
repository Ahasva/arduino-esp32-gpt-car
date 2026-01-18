# Arduino Car Firmware

This sketch runs on an Arduino (UNO) and controls:
- Motors (via H-bridge)
- IR proximity sensors
- Ultrasonic distance sensor

## Communication
- Receives commands via Serial:
  - F = forward
  - B = backward
  - L = left
  - R = right
  - S = stop
- Sends telemetry:
  S:<left><center><right>,<distance_cm>

## Upload
1. Open `SunFounder_car_project_with_ESP32_v2.ino` in Arduino IDE
2. Select correct board & port
3. Upload