# Stage 1 – Project Introduction

## 1.1 Project Title
**Virtual Parking Sensor – Linux Character Device Driver with a C++ Dashboard**

## 1.2 Project Idea
Modern cars have ultrasonic parking sensors in the rear bumper. While the
driver reverses, the sensor measures the distance to the nearest obstacle and
the car warns with beeps that become faster as the obstacle gets closer, until
a continuous tone means *STOP*.

This project builds the **software stack of such a system on Linux**:

1. A **kernel-space device driver** that behaves like the sensor hardware and
   exposes it as the device file `/dev/parksensor`.
2. A **user-space C++ application** (the car's "dashboard") that reads the
   sensor through standard system calls, classifies the danger level, warns the
   driver, applies an automatic brake and logs all events.

The sensor is **simulated in the kernel** (a kernel timer moves the car and
adds measurement noise), so the project runs on any Linux PC or VM without
special hardware, while using the same driver structure a real sensor driver
would use.

## 1.3 Objective
- Learn and demonstrate how Linux device drivers expose hardware to user space.
- Apply system programming concepts: system calls, file descriptors, ioctl,
  signals, procfs, synchronization.
- Apply C++ object-oriented design: classes, RAII, exceptions, STL.
- Follow a professional development process: requirements → design →
  implementation → testing → delivery, tracked in Git.

## 1.4 Problem Statement
Reversing a vehicle is risky because the driver cannot see obstacles directly
behind the car. A parking assistance system must:

- measure the distance to obstacles continuously,
- translate the raw distance into an understandable warning level,
- react fast enough to stop the vehicle before a collision,
- keep a record of events for diagnostics.

In an embedded Linux system this functionality is split between a **driver**
(owns the hardware, provides data safely to many processes) and an
**application** (decides what the data means and interacts with the user).
The problem solved here is designing and implementing both layers and the
interface between them.

## 1.5 Project Scope

**In scope**
- Loadable kernel module implementing a character device (`/dev/parksensor`).
- Simulated distance sensor 0–400 cm (range of a typical HC-SR04 sensor).
- Vehicle movement simulation: idle, reversing, forward, configurable speed.
- Four proximity zones (SAFE, CAUTION, DANGER, STOP) with configurable thresholds.
- `read`/`write`/`ioctl` interfaces and a `/proc` statistics file.
- C++ dashboard: single reading, live monitoring, alerts, beep pattern,
  auto-brake, configuration menu, event log.
- Unit tests and driver integration tests in C++.
- Documentation of all six stages and UML diagrams.

**Out of scope**
- Real sensor hardware (GPIO / interrupt-based echo measurement).
- Graphical user interface.
- Multiple sensors and camera integration.

## 1.6 Expected Outcome
- A kernel module that loads cleanly, creates `/dev/parksensor`, and unloads
  without leaks.
- A C++ application that shows live distance readings, warns at each zone
  change and stops the car automatically in the STOP zone.
- Automated tests that verify the driver API and the application logic.
- A GitHub repository with source code, README, documentation and diagrams.

## 1.7 Applications
- **Automotive:** rear/front parking assistance, blind-spot detection.
- **Robotics:** obstacle detection for mobile robots and AGVs in warehouses.
- **Industrial:** forklift and conveyor proximity safety.
- **Education:** template for writing any sensor driver – the same structure
  works for temperature, light or gas sensors.

## 1.8 Technologies
| Area | Technology |
|---|---|
| Operating system | Linux (Ubuntu) |
| Kernel module | C, Kbuild, kernel APIs (cdev, timers, spinlocks, procfs) |
| Application | C++17, POSIX system calls |
| Build | GNU Make, GCC/G++ |
| Version control | Git, GitHub |
| Diagrams | Mermaid (rendered by GitHub) |

## 1.9 Next Stage
Stage 2 documents the functional and non-functional requirements (PRD),
the module breakdown and the development plan.
