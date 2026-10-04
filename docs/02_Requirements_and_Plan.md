# Stage 2 – Project Requirements Document (PRD) & Development Plan

## 2.1 Purpose
This document defines what the Virtual Parking Sensor system must do
(functional requirements), how well it must do it (non-functional
requirements), how the work is divided into modules and how it is planned.

## 2.2 Stakeholders / Users
| User | Need |
|---|---|
| Vehicle driver (dashboard user) | Clear distance and warning information, automatic protection |
| Developer / tester | Simple shell interface and test suites to verify the driver |
| Evaluator | Documentation, demonstration, Git history |

## 2.3 Functional Requirements

### Kernel driver
| ID | Requirement |
|---|---|
| FR-D1 | The driver shall register a character device and create `/dev/parksensor` automatically when loaded. |
| FR-D2 | The driver shall simulate a distance in the range 0–400 cm. |
| FR-D3 | The driver shall classify the distance into SAFE, CAUTION, DANGER, STOP zones using thresholds (defaults 150 / 80 / 30 cm). |
| FR-D4 | `read()` shall return a human readable sample (`distance=.. cm zone=.. mode=..`). |
| FR-D5 | `write()` shall accept text commands: `dist <cm>`, `speed <cm>`, `mode idle\|reverse\|forward`, `reset`. |
| FR-D6 | `ioctl()` shall provide: get reading, set distance, set mode, set speed, set/get thresholds, reset. |
| FR-D7 | A kernel timer shall update the distance every 200 ms according to the mode and speed, with ±1 cm noise. |
| FR-D8 | The simulation shall stop the vehicle automatically at 0 cm and 400 cm. |
| FR-D9 | The driver shall reject invalid values with `EINVAL` and unknown ioctls with `ENOTTY`. |
| FR-D10 | The driver shall publish statistics in `/proc/parksensor`. |
| FR-D11 | The driver shall log zone changes and configuration changes to the kernel log. |
| FR-D12 | The initial distance shall be configurable with the module parameter `start_distance`. |

### User application
| ID | Requirement |
|---|---|
| FR-A1 | The application shall open the device and report a clear error if the driver is not loaded. |
| FR-A2 | The application shall display a single reading (distance, zone, status message, mode, visual bar). |
| FR-A3 | The application shall offer a live monitoring mode for reversing and driving forward. |
| FR-A4 | The application shall announce every zone change with a message. |
| FR-A5 | The application shall beep faster when the obstacle is closer (SAFE silent, CAUTION 800 ms, DANGER 300 ms, STOP 100 ms). |
| FR-A6 | The application shall apply an automatic brake (set mode IDLE) when the STOP zone is reached while reversing. |
| FR-A7 | The user shall be able to set distance, speed and thresholds and reset the sensor. |
| FR-A8 | The application shall log events with timestamps to `parking_log.txt` and show the last entries. |
| FR-A9 | Ctrl+C shall stop live monitoring safely (vehicle stopped) and return to the menu. |
| FR-A10 | The application shall show the driver statistics from `/proc/parksensor`. |

## 2.4 Non-Functional Requirements
| ID | Category | Requirement |
|---|---|---|
| NFR-1 | Platform | Runs on Linux only, kernel 5.15 or newer (Ubuntu 22.04/24.04). |
| NFR-2 | Language | Kernel code in C, application and tests in C++17. No other languages. |
| NFR-3 | Reliability | Module load/unload must not leak resources; the timer must be stopped before unload. |
| NFR-4 | Concurrency | Sensor state must be protected against concurrent access (processes and timer). |
| NFR-5 | Safety | All user pointers are accessed only via `copy_to_user`/`copy_from_user`; all inputs are validated. |
| NFR-6 | Performance | Readings refresh every 200 ms; the application polls every 100 ms so a zone change is shown within 300 ms. |
| NFR-7 | Usability | Colored output, clear messages, input validation in menus. |
| NFR-8 | Maintainability | Shared header for kernel/user API, one class per responsibility, Linux kernel coding style for the driver. |
| NFR-9 | Testability | Logic testable without the driver; driver testable through automated integration tests. |
| NFR-10 | Portability | Compatible with kernel API changes (class_create, timer delete, devnode) via version checks. |

## 2.5 Modules
| Module | Location | Responsibility |
|---|---|---|
| Shared API | `include/parking_sensor_ioctl.h` | ioctl codes, data structures, zone rule |
| Kernel driver | `driver/parking_sensor.c` | device registration, file operations, simulation timer, procfs |
| ParkingSensor | `app/src/ParkingSensor.cpp` | RAII device access, ioctl calls |
| AlertManager | `app/src/AlertManager.cpp` | zone conversion, messages, beep rate, transition tracking |
| Display | `app/src/Display.cpp` | terminal output, colors, distance bar |
| Logger | `app/src/Logger.cpp` | timestamped log file |
| Main / App | `app/src/main.cpp` | menu, live monitor, auto-brake, signals, CLI options |
| Tests | `tests/` | unit and integration test suites |

## 2.6 Features Summary
- Simulated ultrasonic sensor with realistic noise
- Three ways to talk to the driver: ioctl (binary), read/write (text), procfs
- Four warning zones with configurable thresholds
- Live parking assistant with beeps and auto-brake
- Event logging and statistics
- Automated tests

## 2.7 Deliverables
| # | Deliverable |
|---|---|
| D1 | Kernel module source + Makefile |
| D2 | C++ application source + Makefile |
| D3 | Unit and integration tests |
| D4 | README.md with build/run instructions |
| D5 | Stage documents 1–6 |
| D6 | UML diagrams (class, sequence, state machine) and architecture diagram |
| D7 | Public GitHub repository with commit history |
| D8 | Live demonstration |

## 2.8 Development Plan

| Phase | Stage | Tasks | Output |
|---|---|---|---|
| 1 | Introduction | Choose idea, define problem & scope | `01_Project_Introduction.md` |
| 2 | Requirements | FR/NFR, modules, deliverables, plan | `02_Requirements_and_Plan.md` |
| 3 | Design | Architecture, data structures, UML, Git setup | `03_System_Design.md`, `UML_Diagrams.md` |
| 4 | Prototype | Driver skeleton (open/read), ioctl, timer; C++ wrapper + menu | Working prototype |
| 5 | Testing | Unit + integration tests, bug fixes, procfs, auto-brake | `05_Testing.md`, test suites |
| 6 | Final | Polish, README, final report, demo | `06_Final_Report.md`, release tag |

### Milestones
| Milestone | Done when |
|---|---|
| M1 | Driver loads, `/dev/parksensor` exists, `cat` returns a sample |
| M2 | ioctl API complete, timer simulation running |
| M3 | C++ dashboard shows live readings and alerts |
| M4 | All tests pass, documentation complete, repository published |

## 2.9 Risks
| Risk | Mitigation |
|---|---|
| Kernel API differs between versions | `LINUX_VERSION_CODE` checks in the driver |
| Kernel crash during development | Develop in a virtual machine, take snapshots |
| Secure Boot blocks unsigned modules | Disable Secure Boot in the VM |
| Race conditions between timer and ioctl | Spinlock around all shared state |

## 2.10 Next Stage
Stage 3 designs the architecture, data structures and UML diagrams based on
these requirements.
