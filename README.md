# Parking Gate Control System

An embedded + computer vision system for automated parking lot access control. An ESP32 microcontroller manages the physical gate hardware, while a Python host handles license plate recognition, QR code generation, payment verification, and database management — communicating over UART.

---

## System Overview

```
[Vehicle] → [ESP32 Sensors] → [Gate State Machine] ──UART──► [Python Host]
                                                                     │
                                                          ┌──────────┴──────────┐
                                                     [OpenCV/OCR]         [SQLite DB]
                                                     [QR Reader]          [Payments]
                                                          │
                                                    UART "RELEASE_GATE"
                                                          │
                                                    [ESP32 opens gate]
```

**Entry flow:** vehicle detected → ESP32 triggers Python → plate read via OCR → QR ticket generated → registered in DB → gate released.

**Exit flow:** vehicle detected → ESP32 triggers Python → QR code scanned → payment verified → gate released or blocked.

---

## Hardware

| Component | Description |
|---|---|
| ESP32 | Main microcontroller (FreeRTOS, dual-core) |
| Servo motor | Gate arm actuator (GPIO 19, LEDC channel 0) |
| Inductive loop sensor | Vehicle presence detection (GPIO 12) |
| Ultrasonic sensor (HC-SR04) | Passage detection for anti-crush (TRIG 13, ECHO 34) |
| Limit switch 0° | Gate closed end-stop (GPIO 14) |
| Limit switch 90° | Gate open end-stop (GPIO 27) |
| Paper/ticket sensor | Ticket present detection (GPIO 16) |
| Red LED | Status indicator — locked / error (GPIO 25) |
| Green LED | Status indicator — open / passing (GPIO 26) |
| Webcam | License plate reading and QR code scanning |
| Thermal printer | Ticket printing (interface defined externally) |

---

## Project Structure

```
├── main.cpp                # ESP32 entry point and hardware instantiation
├── gate_ctrl.hpp           # Gate state machine class
├── hardware_drivers.hpp    # ESP32 peripheral drivers (GPIO, servo, ADC, ultrasonic)
├── uart_handler.hpp        # UART communication task (ESP32 side)
├── gate_client.py          # Serial communication client and event loop (Python side)
├── callbacks.py            # Entry and exit processing logic
├── vision_drivers.py       # OpenCV, EasyOCR, and QR code utilities
└── data_base.py            # SQLite database operations
```

---

## Gate State Machine

The gate operates as a finite state machine with the following states:

```
LOCKED ──(vehicle on loop)──► PROCESSING ──(process_done)──► OPENING ──(90° reached)──► OPENED
   ▲                               │                              │                          │
   │                        (60s timeout)                  (obstruction)              (loop clears)
   │                               ▼                              │                          ▼
   └──────────(0° reached)── CLOSING ◄──────────────────────────┘                       CLOSING
                                   │
                             (10s timeout)
                                   ▼
                            TIMEOUT_ERR ──(area clear)──► CLOSING

                            SECURITY_BREACH (gate forced while closed)
```

**LED indicators:**

| State | Red | Green |
|---|---|---|
| LOCKED | ON | OFF |
| PROCESSING | Blinking (slow) | OFF |
| OPENING / OPENED | OFF | ON |
| CLOSING | ON | OFF |
| TIMEOUT_ERR / SECURITY_BREACH | Blinking (fast) | Blinking (fast) |

---

## Gate Modes

The gate supports two operating modes, set at compile time in `main.cpp`:

**`ENTRY_PRINTING`** — issues a QR ticket on vehicle arrival.
The Python side reads the license plate via OCR, generates a SHA-256 hash, prints a QR code, and registers the entry in the database.

**`EXIT_VALIDATING`** — validates a QR ticket on vehicle departure.
The Python side scans the QR code from the ticket, verifies payment and expiry window in the database, and releases the gate if authorized.

---

## Python Host

### Dependencies

```bash
pip install pyserial opencv-python easyocr qrcode numpy
```

### Running

```bash
python gate_client.py
```

By default, connects to `/dev/ttyUSB0` at 115200 baud. Change the port in `gate_client.py` constructor if needed.

### Payment Logic

The database (`parking.db`, SQLite) stores each vehicle entry by its plate hash. Payment rules:

- First **10 minutes** after entry: free exit (initial courtesy).
- After payment: exit allowed until the next full hour, with a minimum 20-minute grace period.
- Expired payment window: record is reset to unpaid and exit is blocked.

---

## UART Protocol

Communication between ESP32 and the Python host is plain-text over UART0 at 115200 baud.

| Direction | Message | Description |
|---|---|---|
| ESP32 → Python | `[ENTRY_GATE]: ... Requesting processment...` | Vehicle detected at entry |
| ESP32 → Python | `[EXIT_GATE]: ... Requesting processment...` | Vehicle detected at exit |
| Python → ESP32 | `RELEASE_GATE\n` | Authorize gate to open |

---

## ESP32 Build

This project is built with **ESP-IDF**. Ensure the IDF environment is sourced before building.

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Known Limitations

- EasyOCR initializes at module import time, adding startup latency even when OCR is not needed.
- `SECURITY_BREACH` state has no automatic recovery; manual hardware reset is required.

## Observation

- QR display in generate_qr_code uses cv2.waitKey(0) and blocks until the window is closed manually.
      - It's good to Workbench testing
      - To real applications, seting cv2.waitKey(1) is needed