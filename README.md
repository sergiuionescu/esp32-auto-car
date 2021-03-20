# ESP32 autonomous toy car
The projects contains software for assisting a toy car navigation.

#Components

- Logic board: 1xESP32 (Plusivo)
- Motor controller: 1xTB6612FNG
- Ultrasound sensor: 3x
- 1xTest board
- 3xUltrasound sensor mounts
- Car Chassis
- 2x5V motors

#Circuit connection

| ESP32 | TB6612FNG | HC-SRO4 FR | HC-SRO4 FL | HC-SRO4 FM | Motor Left | Motor Right |
|-------|------------|------------|------------|------------|------------|-------------|
|       | VM(9V)     |            |            |            |            |             |
| VCC   | VCC        |            |            |            |            |             |
| GND   | GND        |            |            |            |            |             |
| GND   | GND        |            |            |            |            |             |
| GND   | GND        |            |            |            |            |             |
| D18   | AIN1       |            |            |            |            |             |
| D19   | AIN2       |            |            |            |            |             |
| D23   | PWMA       |            |            |            |            |             |
| D27   | STBY       |            |            |            |            |             |
| D26   | BIN1       |            |            |            |            |             |
| D25   | BIN2       |            |            |            |            |             |
| D33   | PWMB       |            |            |            |            |             |
| D5    |            | Echo       |            |            |            |             |
| D4    |            | Trig       |            |            |            |             |
| GND   |            | Gnd        |            |            |            |             |
| VIN   |            | Vcc        |            |            |            |             |
| D14   |            |            | Echo       |            |            |             |
| D32   |            |            | Trig       |            |            |             |
| GND   |            |            | Gnd        |            |            |             |
| VIN   |            |            | Vcc        |            |            |             |
| D12   |            |            |            | Echo       |            |             |
| D13   |            |            |            | Trig       |            |             |
| GND   |            |            |            | Gnd        |            |             |
| VIN   |            |            |            | Vcc        |            |             |
|       | A1         |            |            |            | A1         |             |
|       | A2         |            |            |            | A2         |             |
|       | B1         |            |            |            |            | B1          |
|       | B2         |            |            |            |            | B2          |