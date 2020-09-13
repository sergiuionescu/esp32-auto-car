# ESP32 autonoumous toy car
The projects contains software for assisting a toy car navigation.

#Components

- Logic board: 1xESP32 (Plusivo)
- Motor controller: 1xTB6612FNG
- Ultrasound sensor: 2x
- 1xTest board
- 2xUltrasound sensor mounts
- Car Chasis
- 2x5V motors

#Circuit connection

| ESP32 | TB66112FNG | HC-SRO4 FL | HC-SRO4 FR | HC-SRO4 FM | Motor Left | Motor Right |
|-------|------------|------------|------------|------------|------------|-------------|
| GND   | GND        |            |            |            |            |             |
| GND   | GND        |            |            |            |            |             |
| GND   | GND        |            |            |            |            |             |
| D18   | AIN1       |            |            |            |            |             |
| D19   | AIN2       |            |            |            |            |             |
| D21   | PWMA       |            |            |            |            |             |
| D33   | STBY       |            |            |            |            |             |
| D25   | BIN1       |            |            |            |            |             |
| D26   | BIN2       |            |            |            |            |             |
| D27   | PWMB       |            |            |            |            |             |
| D12   |            | Echo       |            |            |            |             |
| D13   |            | Trig       |            |            |            |             |
| GND   |            | Gnd        |            |            |            |             |
| VIN   |            | Vcc        |            |            |            |             |
| D14   |            |            | Echo       |            |            |             |
| D32   |            |            | Trig       |            |            |             |
| GND   |            |            | Gnd        |            |            |             |
| VIN   |            |            | Vcc        |            |            |             |
| D4    |            |            |            | Echo       |            |             |
| D5    |            |            |            | Trig       |            |             |
| GND   |            |            |            | Gnd        |            |             |
| VIN   |            |            |            | Vcc        |            |             |
|       | A1         |            |            |            | A1         |             |
|       | A2         |            |            |            | A2         |             |
|       | B1         |            |            |            |            | B1          |
|       | B2         |            |            |            |            | B2          |