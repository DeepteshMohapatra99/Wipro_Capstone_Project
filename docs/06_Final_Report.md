# Stage 6 – Final Implementation & Project Report

## 6.1 Summary
The **Virtual Parking Sensor** is a Linux system consisting of a loadable
kernel module that simulates an ultrasonic rear parking sensor and a C++17
dashboard application that turns the sensor data into driver warnings,
audible beep patterns and an automatic brake. Both layers communicate through
the character device `/dev/parksensor` using `read`, `write` and `ioctl`, and
the driver publishes statistics in `/proc/parksensor`.

## 6.2 Final Architecture
See [03_System_Design.md](03_System_Design.md) for the architecture diagram
and [UML_Diagrams.md](UML_Diagrams.md) for class, sequence and state machine
diagrams.

```
 C++ Dashboard (user space)                Kernel module (kernel space)
 ┌───────────────────────────┐   ioctl    ┌──────────────────────────────┐
 │ App: menu, live monitor   │──────────▶│ file_operations              │
 │ ParkingSensor (RAII)      │◀──────────│ sensor state + spinlock      │
 │ AlertManager, Display     │   reading  │ kernel timer (200 ms)        │
 │ Logger → parking_log.txt  │            │ /proc/parksensor             │
 └───────────────────────────┘            └──────────────────────────────┘
```

## 6.3 Implementation Overview

| Part | Files | Size (approx.) |
|---|---|---|
| Shared API | `include/parking_sensor_ioctl.h` | 100 lines |
| Kernel driver | `driver/parking_sensor.c` | 530 lines |
| C++ application | `app/include/*.hpp`, `app/src/*.cpp` | 810 lines |
| Tests | `tests/*.cpp`, `tests/test_framework.hpp` | 480 lines |

### Concepts demonstrated

| Area | Concepts |
|---|---|
| Linux device drivers | Loadable module, `module_init/exit`, dynamic major/minor, `cdev`, device class and udev node, `file_operations`, `copy_to_user`/`copy_from_user`, `ioctl` with `_IOR/_IOW/_IO`, kernel timers, spinlocks (`spin_lock_bh`), procfs (`seq_file`), module parameters, `printk` logging, error unwinding |
| System programming | `open/read/write/ioctl/close`, file descriptors, `errno` handling, signals (`sigaction`), `/proc` filesystem, `isatty`, file I/O |
| C++ | Classes and encapsulation, RAII, custom exceptions, `enum class`, `std::optional`, STL containers, `std::chrono`, lambdas and templates (tests), const-correctness |
| Software engineering | Requirements, design, UML, layered architecture, unit and integration testing, Git branching, documentation |

## 6.4 Demonstration Plan (5–10 minutes)

| Time | Step | Command |
|---|---|---|
| 1 min | Show repository structure and README | GitHub page |
| 1 min | Build and load the driver | `make && make load && ls -l /dev/parksensor` |
| 1 min | Driver from the shell | `cat /dev/parksensor`, `echo "dist 60" > /dev/parksensor`, `cat /proc/parksensor` |
| 3 min | Dashboard: single reading, reverse parking with auto-brake, thresholds, log | `make run` → 1, 2, 7, 10 |
| 1 min | Kernel log | `make logs` |
| 1 min | Tests | `make test` |
| 1 min | Architecture, UML, limitations, future work | `docs/` |

## 6.5 Results
- The driver registers `/dev/parksensor` and `/proc/parksensor` on load and
  removes them on unload.
- The simulation moves the car every 200 ms and reports zone changes.
- The dashboard announces every zone change, beeps at a rate matching the
  zone and stops the car automatically in the STOP zone.
- Invalid input is rejected both in the application and in the driver.
- Test results are recorded in [05_Testing.md](05_Testing.md#57-test-results).

## 6.6 Achievements
- Complete driver ↔ application stack with three kernel interfaces
  (ioctl, read/write, procfs).
- Safe concurrency between process context and timer softirq.
- Clean module unload even while the simulation is running.
- Compatible with multiple kernel versions.
- Automated unit and integration tests written in C++ with no external
  dependencies.
- Full documentation for all six stages, with UML diagrams rendered on GitHub.

## 6.7 Limitations
- The sensor is simulated; no real hardware is accessed.
- Only one sensor (one minor number) is supported.
- The application polls the driver; there is no blocking or event-driven read.
- Beeps use the terminal bell; there is no real buzzer.
- Text user interface only.

## 6.8 Future Improvements
| Improvement | Approach |
|---|---|
| Real hardware | HC-SR04 on Raspberry Pi: GPIO trigger + echo interrupt, measure pulse width with `ktime`, keep the same ioctl API |
| Event-driven alerts | Implement `poll()` + wait queue; wake readers on zone change |
| Multiple sensors | Allocate several minor numbers, one state struct per sensor (rear left/center/right) |
| sysfs attributes | Expose thresholds under `/sys/class/parksensor_class/parksensor/` |
| Real buzzer / LED | PWM or GPIO output driven by the zone |
| GUI | Qt (C++) dashboard showing a car and obstacle graphic |
| Device Tree | Describe the sensor in a DT overlay and use a platform driver |

## 6.9 Conclusion
The project demonstrates a complete, professional development process, from
requirements to tested and documented delivery, for a Linux device driver
and its C++ user-space application. The driver can later be connected to real
sensor hardware without changes to the application.

## 6.10 Submission Checklist
- [ ] Source code pushed to GitHub (`driver/`, `app/`, `include/`, `tests/`)
- [ ] README.md with build and run instructions
- [ ] Stage documents 1–6 in `docs/`
- [ ] UML diagrams (`docs/UML_Diagrams.md`)
- [ ] Screenshots in `docs/images/`
- [ ] Test results filled in `docs/05_Testing.md`
- [ ] Git history with meaningful commits, release tag `v1.0`
- [x] Author name filled in (`README.md`, `MODULE_AUTHOR`)
