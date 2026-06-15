# AquaTemp Control — IoT Water Temperature & Quality Controller

![Platform](https://img.shields.io/badge/platform-Arduino%20Mega%202560-00979D)
![Wi-Fi](https://img.shields.io/badge/Wi--Fi-ESP8266%20(ESP--01S)-E7352C)
![Language](https://img.shields.io/badge/language-C%2B%2B-00599C)
![Modeling](https://img.shields.io/badge/modeling-MATLAB%20%2F%20Simulink-E68A00)
![License](https://img.shields.io/badge/license-MIT-3DA639)

An IoT system that **automatically controls the temperature of water** and **monitors its quality**, with a built-in web interface for real-time monitoring and remote control. An **Arduino Mega 2560** runs the sensing and a **PI (Proportional–Integral) control loop** that drives a heating element through a **BTS7960B** power driver using PWM; an **ESP-01S (ESP8266)** Wi-Fi module hosts a small web app you can open from any phone or laptop.

The controller was not hand-tuned by guesswork — it was designed from a **measured plant model**, using system identification in MATLAB and validation in Simulink, then discretized and implemented on the Arduino.

![AquaTemp Control web interface](images/web-interface.png)

---

## Features

- Closed-loop water-temperature control with a **discrete PI controller** derived from an identified first-order plant model.
- Smooth, proportional heating via **PWM** through a solid-state H-bridge driver — no relay chattering or on/off temperature oscillation.
- Real-time **water-quality** monitoring with a TDS (Total Dissolved Solids) sensor.
- **Web dashboard** served directly by the ESP-01S (Wi-Fi Access Point): live readings, a setpoint control, a "target reached" indicator, and a live temperature/PWM chart.
- Live updates without page reloads, using **Server-Sent Events (SSE)**.
- Firmware PWM clamp to avoid overdriving the heating element.

## Architecture

Two microcontrollers cooperate over a UART serial link:

- **Arduino Mega 2560** — reads the sensors, runs the PI loop, generates the PWM command for the heater, and streams the live values.
- **ESP-01S (ESP8266)** — runs as a Wi-Fi Access Point and asynchronous web server; serves the dashboard and relays the new setpoint back to the Arduino.

```
[DS18B20] --1-Wire(D2)--+
                        |
[TDS sensor] --A0-------+--> [Arduino Mega 2560] --PWM--> [BTS7960B] --> [12V heater]
                                   |    ^
                              UART |    | UART   (115200 baud)
                                   v    |
                          [ESP-01S / ESP8266] --Wi-Fi / HTTP--> [Browser dashboard]
```

## Hardware (Bill of Materials)

| Component | Role | Key specs |
|-----------|------|-----------|
| Arduino Mega 2560 | Main controller (sensing + PI loop + PWM) | ATmega2560, 16 MHz, 5 V logic |
| ESP-01S (ESP8266) | Wi-Fi + web server (Access Point) | 802.11 b/g/n, 2.4 GHz, 3.3 V, UART |
| DS18B20 | Water-temperature sensor | digital, 1-Wire, −55…+125 °C, ±0.5 °C, waterproof probe |
| TDS sensor | Water-quality sensor (dissolved solids) | analog, 0–1000 ppm, ±10 % |
| BTS7960B | Power driver (H-bridge) for the heater | 6–27 V, up to 43 A continuous, PWM up to 25 kHz |
| Heating element | Submersible water heater | 12 V DC, ~13 A, ~156 W |
| DC power supply | Powers the heater + driver | 12 V, 20 A, 240 W |
| 4.7 kΩ resistor | 1-Wire pull-up for the DS18B20 | between data line and Vcc |
| ESP-01S adapter | Safe 3.3 V power/level for the ESP | 5 V → 3.3 V regulator adapter |

## Wiring / pin mapping

| Signal | Arduino Mega pin | Notes |
|--------|------------------|-------|
| DS18B20 data | **D2** (1-Wire) | 4.7 kΩ pull-up to Vcc |
| TDS analog out | **A0** | `analogRead` |
| BTS7960B `RPWM` | **D8** | PWM (forward / heating) |
| BTS7960B `LPWM` | **D9** | reverse — unused (heating only) |
| BTS7960B `R_EN` | **D7** | driver enable |
| BTS7960B `L_EN` | **D6** | driver enable |
| To ESP-01S RX | **TX1 (D18)** | UART @ 115200, via 3.3 V adapter |
| From ESP-01S TX | **RX1 (D19)** | UART @ 115200 |

> The ESP-01S is a **3.3 V** device. Power and level-shift it through the adapter — do not connect it straight to 5 V logic.

## Control design (the PI controller)

This is the core of the project.

**1. System identification.** With the heater driven by a constant PWM step (duty = 90/255), the water temperature was recorded over time with [SerialPlot](https://github.com/hyOzd/serialplot) and exported as CSV. The response is that of a **first-order system**.

![Open-loop step captured in SerialPlot](images/serialplot-identification.png)

**2. Plant model.** From the step data (processed in MATLAB — see [`matlab/system_identification.m`](matlab/system_identification.m)) the plant is modeled as a first-order transfer function:

```
          0.67
H(s) = -----------
        700·s + 1
```

i.e. static gain `K = 0.67` and time constant `T = 700 s`. The identified model matches the measurement closely (red = experimental, blue = model):

![Identified model vs. experimental step response](images/step-response.png)

**3. Controller design.** The PI controller was designed by **pole–zero cancellation** in the closed loop, choosing the integral time `Ti = 700 s` equal to the plant time constant, and validated in **Simulink** (reference 60 °C, with a reference-trajectory filter `1/(80·s + 1)` for a smooth ramp). The simulated response reaches the setpoint with no significant overshoot.

![Closed-loop block diagram](images/control-block-diagram.png)

> **Why PI and not PID?** The thermal process is slow — the water temperature changes gradually — so a derivative term adds little and mainly makes the loop sensitive to measurement noise. The proportional term reacts to the error and the integral term removes the steady-state error: enough for stable, accurate control.

**4. Discretization.** For the microcontroller, the continuous PI was discretized with a backward finite-difference approximation `s ≈ (1 − z^-1) / Te`, giving a recurrence that runs every control cycle:

```
u[k] = ε[k] · Kr · (Ti + Te)/Ti  −  ε[k−1] · Kr  +  u[k−1]
```

where `ε[k] = setpoint − measured_temperature`, `Te` is the sampling period and `u[k]` is the PWM command (clamped to a safe maximum). Only the previous error `ε[k−1]` and previous command `u[k−1]` need to be stored, so it is cheap to run on the Arduino.

## Communication protocol

```
Arduino  →  ESP-01S            UART, 115200 baud
  "<temperature>,<tds_ppm>,<temperature_percent>,<pwm>\n"
  example:  52.31,24,98.70,122.17

ESP-01S  →  Arduino            new setpoint from the web UI
  "<setpoint>\n"               (Arduino replies "OK")
```

- **DS18B20 → Arduino** over **1-Wire** (digital, single data line, each device has a unique 64-bit address).
- **TDS → Arduino** over an **analog** input.
- The ESP-01S runs a Wi-Fi **Access Point** (`SSID: ESP8266-AP`) and serves the dashboard at **http://192.168.4.1**; readings are pushed to the browser via **SSE**, and the chart is drawn with **Chart.js** (a sliding window of the last 20 points).

## Repository structure

```
.
├── firmware/
│   ├── arduino_mega/        # main sketch: sensing + PI loop + PWM   (Arduino Mega 2560)
│   └── esp8266/             # web-server / dashboard sketch          (ESP-01S / ESP8266)
├── matlab/
│   └── system_identification.m   # plant identification + model-vs-data plot
├── images/
│   ├── web-interface.png
│   ├── control-block-diagram.png
│   ├── step-response.png
│   └── serialplot-identification.png
└── README.md
```

> Drop your two `.ino` files into `firmware/arduino_mega/` and `firmware/esp8266/`.

## Getting started

### Requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- Board support:
  - **Arduino AVR Boards** (for the Mega 2560)
  - **ESP8266 Boards** — Boards Manager URL: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
- Libraries (Library Manager):
  - Arduino side: `OneWire`, `DallasTemperature`
  - ESP8266 side: `ESPAsyncTCP`, `ESPAsyncWebServer`

The PI controller is implemented by hand in the firmware, so **no external control library is required**.

### Flash the Arduino Mega 2560

1. Open the sketch in `firmware/arduino_mega/`.
2. **Tools → Board → Arduino Mega 2560**, then select the port.
3. Install `OneWire` and `DallasTemperature`.
4. Upload.

### Flash the ESP-01S

1. Use a proper **3.3 V USB programmer / adapter**. If your adapter doesn't switch automatically, pull **GPIO0 to GND** to enter flashing mode.
2. Open the sketch in `firmware/esp8266/`.
3. **Tools → Board → Generic ESP8266 Module** (or your ESP-01S entry), then select the port.
4. Install `ESPAsyncTCP` and `ESPAsyncWebServer`.
5. Upload.

### Wire it up

- Power the ESP-01S through the **5 V → 3.3 V adapter**.
- DS18B20 data → **D2** with a **4.7 kΩ** pull-up to Vcc.
- TDS analog out → **A0**.
- BTS7960B: `RPWM`→D8, `LPWM`→D9, `R_EN`→D7, `L_EN`→D6; power the driver and the 12 V heater from the **12 V / 20 A** supply.
- Arduino `Serial1` (TX1 = D18, RX1 = D19) ↔ ESP-01S UART, through the adapter.

### Run

1. Power everything from the 12 V supply.
2. On your phone/laptop, connect to the Wi-Fi network **`ESP8266-AP`** (default password `12345678` — change it in the ESP sketch).
3. Open **http://192.168.4.1** in a browser.
4. Enter a target temperature, press **Trimite** (Send), and watch the live readings and chart.

> **Note on the live chart:** the dashboard loads Chart.js from a CDN, so the chart only renders if the browser has internet access. The numeric readings and the setpoint control work fully offline. To make the chart work offline too, host `chart.min.js` on the ESP filesystem and serve it locally.

## Calibration

The TDS sensor uses a simple **two-point** calibration: distilled water (0 ppm) and a known NaCl solution. The mapping constant lives in the Arduino sketch (`CALIBRATION_VALUE`).

## Safety notes

- The heater draws high current (~13 A at 12 V). Size wiring, connectors and PSU accordingly, and never power a submersible heater while it is dry / in air.
- The PWM command is clamped in firmware to protect the heating element.
- This is a hobby / educational project, not a certified appliance.

## Roadmap / possible improvements

- **Active cooling** — the BTS7960B is an H-bridge, so it could drive a Peltier element for bidirectional (heat *and* cool) control.
- Additional water-quality sensors (pH, dissolved oxygen).
- Automatic filtration triggered by TDS thresholds.
- Serve `Chart.js` locally so the chart works fully offline.
- A dedicated mobile app and long-term data logging.

## Author

**Ionuț Ionescu** — [GitHub](https://github.com/ionescuionut1708) · [LinkedIn](https://www.linkedin.com/in/ionut1708)

## Background

Originally developed as an academic control-systems project (Automatic Control and Applied Informatics, UNSTPB). *Feel free to edit or remove this section.*

## License

Released under the **MIT License** — add a `LICENSE` file to the repository. *(MIT is a suggestion; choose whichever license you prefer.)*
