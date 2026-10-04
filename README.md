# Virtual Parking Sensor – Linux Device Driver & C++ Dashboard

[![Build and Test](https://github.com/DeepteshMohapatra99/Wipro_Capstone_Project/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/DeepteshMohapatra99/Wipro_Capstone_Project/actions/workflows/build-and-test.yml)

A Linux **kernel character device driver** that simulates an ultrasonic rear
parking sensor, plus a **C++ user-space dashboard** that reads the distance to
the obstacle, raises proximity alerts, beeps faster as the car gets closer and
applies an **automatic brake** before a collision.

> Capstone Project – Linux Device Drivers, System Programming & C++
> Language: C (kernel) / C++17 (user space) · Platform: Linux only

---

## Table of Contents
1. [Features](#features)
2. [Architecture](#architecture)
3. [Project Structure](#project-structure)
4. [Requirements](#requirements)
5. [Build & Run](#build--run)
6. [Using the Application](#using-the-application)
7. [Testing from the Shell](#testing-from-the-shell)
8. [Running the Test Suites](#running-the-test-suites)
9. [Driver Interface Reference](#driver-interface-reference)
10. [Documentation](#documentation)
11. [Limitations & Future Work](#limitations--future-work)

---

## Features

**Kernel driver (`driver/parking_sensor.c`)**
- Character device `/dev/parksensor` with dynamic major/minor number
- `read()` returns a text sample, `write()` accepts text commands
- `ioctl()` binary API: get reading, set distance/mode/speed, set/get thresholds, reset
- Kernel timer (200 ms) moves the simulated car (reversing / forward) with sensor noise
- Zone classification: **SAFE → CAUTION → DANGER → STOP**
- `/proc/parksensor` statistics (samples, zone changes, opens)
- Spinlock protection between process context and the timer softirq
- Module parameter `start_distance`

**C++ application (`app/`)**
- Menu-driven dashboard with colored zone display and an ASCII distance bar
- Live monitoring mode with zone change alerts and beep rate per zone
- Auto-brake: stops the car when the STOP zone is reached
- Timestamped event log (`parking_log.txt`)
- RAII device wrapper, custom exceptions, safe Ctrl+C handling (`sigaction`)

---

## Architecture

```
 USER SPACE                                            KERNEL SPACE
┌──────────────────────────────────────┐          ┌──────────────────────────────────┐
│ parking_monitor (C++)                │          │ parking_sensor.ko (C)            │
│                                      │  open    │                                  │
│  main / App ── menu, live monitor    │  read    │  file_operations                 │
│    │                                 │  write   │   open/read/write/ioctl/release  │
│    ├── ParkingSensor (RAII, ioctl) ──┼─────────▶│        │                         │
│    ├── AlertManager (zones, beeps)   │  ioctl   │        ▼                         │
│    ├── Display (colors, bar)         │          │  sensor state  ◀── kernel timer  │
│    └── Logger (parking_log.txt)      │◀─────────┼── (spinlock)       every 200 ms  │
│                                      │ reading  │        │                         │
└──────────────────────────────────────┘          │        ▼                         │
          ▲ cat / echo (shell)                    │  /proc/parksensor (statistics)   │
          └───────────── /dev/parksensor ─────────┤                                  │
                                                  └──────────────────────────────────┘
```

Full design, UML class/sequence/state diagrams: [docs/03_System_Design.md](docs/03_System_Design.md)
and [docs/UML_Diagrams.md](docs/UML_Diagrams.md).

---

## Project Structure

```
virtual-parking-sensor/
├── README.md                     # this file
├── Makefile                      # top level: build, load, run, test
├── include/
│   └── parking_sensor_ioctl.h    # shared kernel/user API (ioctl codes, structs)
├── driver/
│   ├── parking_sensor.c          # kernel module (character driver)
│   └── Makefile                  # Kbuild makefile
├── app/
│   ├── include/                  # ParkingSensor, AlertManager, Display, Logger, SensorException
│   ├── src/                      # implementations + main.cpp
│   └── Makefile
├── tests/
│   ├── test_framework.hpp        # minimal C++ test framework
│   ├── unit_tests.cpp            # logic tests (no driver needed)
│   ├── driver_tests.cpp          # integration tests against /dev/parksensor
│   └── Makefile
└── docs/
    ├── 01_Project_Introduction.md
    ├── 02_Requirements_and_Plan.md
    ├── 03_System_Design.md
    ├── UML_Diagrams.md
    ├── 04_Implementation_Progress.md
    ├── 05_Testing.md
    └── 06_Final_Report.md
```

---

## Requirements

- Linux (tested target: Ubuntu 22.04 / 24.04, kernel 5.15 – 6.x)
- GCC / G++ with C++17, `make`
- Kernel headers for the running kernel

```bash
make setup        # installs them on Ubuntu/Debian (apt) or Fedora (dnf)
```

Manual installation:

```bash
sudo apt install build-essential linux-headers-$(uname -r)                 # Ubuntu / Debian
sudo dnf install gcc gcc-c++ make kernel-devel-$(uname -r) elfutils-libelf-devel   # Fedora
```

> Secure Boot: if `insmod` fails with *"Key was rejected by service"*, disable
> Secure Boot in the VM/BIOS settings or sign the module.

---

## Build & Run

```bash
git clone <your-repo-url> virtual-parking-sensor
cd virtual-parking-sensor

make                 # build driver, app and tests
make load            # insert module  -> creates /dev/parksensor
make run             # start the dashboard
make unload          # remove module when finished
```

Optional: start with the car at a different distance:

```bash
make load START=120
```

Kernel log messages:

```bash
make logs            # = sudo dmesg | grep parksensor
```

---

## Using the Application

```
========== Virtual Parking Sensor ==========
  1. Take a single reading
  2. Reverse park (live monitor)
  3. Drive forward (live monitor)
  4. Live monitor without changing mode
  5. Set distance manually
  6. Set vehicle speed
  7. Set zone thresholds
  8. Show zone thresholds
  9. Show driver statistics (/proc)
 10. View log file
 11. Reset sensor
  0. Exit
============================================
```

Example live session (option 2):

```
Live monitoring - press Ctrl+C to stop
 155 cm  SAFE     [CAR]...............|WALL
  >> Zone SAFE -> CAUTION at 149 cm: Obstacle nearby - slow down
  79 cm  DANGER   [CAR].......|WALL
  >> Zone DANGER -> STOP at 29 cm: STOP! Obstacle at the bumper
  AUTO-BRAKE engaged at 29 cm - vehicle stopped
```

Command line options:

| Option | Meaning |
|---|---|
| `--device PATH` | use another device node |
| `--log FILE` | log file (default `parking_log.txt`) |
| `--once` | print one reading and exit |
| `--bell` | audible beeps (terminal bell) |
| `--no-color` | plain output |
| `--no-autobrake` | disable automatic braking |

---

## Testing from the Shell

The driver can be used without the application:

```bash
cat /dev/parksensor                       # distance=300 cm zone=SAFE mode=IDLE
echo "dist 60"      > /dev/parksensor     # set distance
echo "speed 10"     > /dev/parksensor     # cm per 200 ms tick
echo "mode reverse" > /dev/parksensor     # start reversing
cat /dev/parksensor
echo "mode idle"    > /dev/parksensor
echo "reset"        > /dev/parksensor
cat /proc/parksensor                      # statistics
```

---

## Running the Test Suites

```bash
make unit            # 11 unit tests, no driver required
make load
make integration     # 14 driver integration tests
# or both:
make test
```

To build, load, test, run a dashboard demo and save every output as evidence
in [docs/test-results/](docs/test-results/), run a single command:

```bash
make evidence        # results only
make publish         # results + git commit + git push
```

Test plan and test cases: [docs/05_Testing.md](docs/05_Testing.md)

---

## Driver Interface Reference

| ioctl | Argument | Description |
|---|---|---|
| `PS_IOC_GET_READING` | `struct ps_reading *` (out) | distance, zone, mode, speed, sample count |
| `PS_IOC_SET_DISTANCE` | `int32_t *` | 0 – 400 cm |
| `PS_IOC_SET_MODE` | `int32_t *` | `IDLE`, `REVERSING`, `FORWARD` |
| `PS_IOC_SET_SPEED` | `int32_t *` | 1 – 50 cm per tick |
| `PS_IOC_SET_THRESHOLDS` | `struct ps_thresholds *` | caution > danger > stop |
| `PS_IOC_GET_THRESHOLDS` | `struct ps_thresholds *` (out) | current thresholds |
| `PS_IOC_RESET` | – | restore defaults |

Default zones: **SAFE** > 150 cm ≥ **CAUTION** > 80 cm ≥ **DANGER** > 30 cm ≥ **STOP**.

Errors: `EINVAL` (bad value), `EFAULT` (bad user pointer), `ENOTTY` (unknown ioctl).

---

## Documentation

| Stage | Document |
|---|---|
| 1 – Introduction | [docs/01_Project_Introduction.md](docs/01_Project_Introduction.md) |
| 2 – Requirements (PRD) & Plan | [docs/02_Requirements_and_Plan.md](docs/02_Requirements_and_Plan.md) |
| 3 – System Design & Architecture | [docs/03_System_Design.md](docs/03_System_Design.md), [docs/UML_Diagrams.md](docs/UML_Diagrams.md) |
| 4 – Implementation & Prototype | [docs/04_Implementation_Progress.md](docs/04_Implementation_Progress.md) |
| 5 – Testing & Integration | [docs/05_Testing.md](docs/05_Testing.md) |
| 6 – Final Report | [docs/06_Final_Report.md](docs/06_Final_Report.md) |

---

## Limitations & Future Work

- Simulated sensor only. A real HC-SR04 on a Raspberry Pi could replace the
  timer with GPIO + interrupt based echo timing behind the same ioctl API.
- One sensor; real cars use 4–6 (multiple minor numbers would support this).
- Polling from user space; `poll()`/`select()` support would allow
  event-driven zone notifications.

See [docs/06_Final_Report.md](docs/06_Final_Report.md) for details.

---

**Author:** Deeptesh Mohapatra · Capstone Project 2026
