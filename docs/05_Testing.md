# Stage 5 – Testing, Integration & Improvement

## 5.1 Test Strategy

| Level | What is tested | How | Needs driver |
|---|---|---|---|
| Unit | Zone rule, threshold validation, `AlertManager`, `Display`, `Logger` | `tests/unit_tests.cpp` | No |
| Integration | C++ `ParkingSensor` ↔ kernel driver (ioctl, read, write, procfs, timer) | `tests/driver_tests.cpp` | Yes |
| System | Complete application scenarios from the user's point of view | Manual test cases below | Yes |
| Robustness | Module load/unload cycles, invalid input, concurrent access | Shell commands below | Yes |

Both automated suites use a small self-written C++ framework
(`tests/test_framework.hpp`) – no external libraries.

## 5.2 How to Run

```bash
make                # build everything
make unit           # unit tests
make load
make integration    # integration tests
make unload
```

Each suite prints `[PASS]` / `[FAIL]` per test and a summary, and returns a
non-zero exit code on failure.

## 5.3 Unit Test Cases (`unit_tests`)

| ID | Test | Expected |
|---|---|---|
| UT-01 | `classify_default_thresholds` | 400/151 → SAFE, 150/81 → CAUTION, 80/31 → DANGER, 30/0 → STOP (boundaries inclusive) |
| UT-02 | `classify_custom_thresholds` | Zones follow custom limits 100/50/10 |
| UT-03 | `thresholds_validation` | Defaults valid; equal, reversed, negative, > 400 invalid |
| UT-04 | `zone_conversion` | Raw values 0–3 map to `Zone`; 42 and -1 throw |
| UT-05 | `zone_names` | SAFE, CAUTION, DANGER, STOP |
| UT-06 | `beeps_get_faster_when_closer` | SAFE silent; CAUTION > DANGER > STOP > 0 ms |
| UT-07 | `alert_state_transitions` | First update true, same zone false, two transitions counted, `clear()` resets |
| UT-08 | `distance_bar` | 1 dot per 10 cm, clamped to 0–400 cm |
| UT-09 | `mode_names` | IDLE, REVERSING, FORWARD, unknown → "?" |
| UT-10 | `colorize_disabled_returns_plain_text` | No escape codes when color is off |
| UT-11 | `logger_writes_and_tails` | Entries appended with level, `tail(n)` returns last n |

## 5.4 Integration Test Cases (`driver_tests`)

| ID | Test | Expected |
|---|---|---|
| IT-01 | `open_missing_device_fails` | `SensorException` with `ENOENT` |
| IT-02 | `reset_restores_defaults` | 300 cm, SAFE, IDLE, speed 5, thresholds 150/80/30 |
| IT-03 | `zones_follow_distance` | 200 SAFE, 120 CAUTION, 50 DANGER, 10 STOP |
| IT-04 | `invalid_values_are_rejected` | Distance -1/401, speed 0/51, mode 3, bad thresholds → `EINVAL`; state unchanged |
| IT-05 | `custom_thresholds_change_zone` | 100 cm becomes STOP with thresholds 120/110/105 |
| IT-06 | `idle_distance_is_constant` | 123 cm unchanged after 600 ms |
| IT-07 | `reversing_reduces_distance` | 300 → between 200 and 300 after 1 s at 10 cm/tick |
| IT-08 | `forward_increases_distance` | 100 → between 100 and 200 after 1 s |
| IT-09 | `vehicle_stops_at_zero` | Distance 0, mode IDLE, zone STOP |
| IT-10 | `sample_counter_increments` | 1, then 2 after reset |
| IT-11 | `text_read_interface` | `read()` text contains distance, zone, mode |
| IT-12 | `text_write_interface` | `dist 77` sets 77 cm; unknown command → `EINVAL` |
| IT-13 | `unknown_ioctl_is_rejected` | `ENOTTY` |
| IT-14 | `proc_statistics_available` | `/proc/parksensor` contains `distance_cm`, `zone_changes` |

## 5.5 System Test Cases (manual)

| ID | Scenario | Steps | Expected | Result |
|---|---|---|---|---|
| ST-01 | Start without driver | `make unload`; `./app/parking_monitor` | "Cannot open /dev/parksensor (is the driver loaded?)", exit code 1 | ☐ |
| ST-02 | Single reading | Menu 1 | Card with 300 cm, SAFE, green | ☐ |
| ST-03 | Reverse parking | Menu 2 | Zones SAFE → CAUTION → DANGER → STOP announced, auto-brake at ≤ 30 cm | ☐ |
| ST-04 | Ctrl+C in live mode | Menu 2, press Ctrl+C | "stopped by user", menu shown again, mode IDLE | ☐ |
| ST-05 | Drive forward | Menu 5 → 20; menu 3 | Distance increases, zones go back to SAFE, stops at 400 | ☐ |
| ST-06 | No auto-brake | `--no-autobrake`, menu 2 | Car continues to 0 cm, driver stops it | ☐ |
| ST-07 | Custom thresholds | Menu 7: 200/100/50; menu 8 | New thresholds shown; zones change at new limits | ☐ |
| ST-08 | Invalid threshold input | Menu 7: 50/100/150 | "Invalid: thresholds must satisfy..." | ☐ |
| ST-09 | Invalid menu input | Enter `abc`, `99` | Re-prompt, no crash | ☐ |
| ST-10 | Log file | Menu 10 | Timestamped entries incl. zone changes, auto-brake | ☐ |
| ST-11 | Driver statistics | Menu 9 | Contents of `/proc/parksensor` | ☐ |
| ST-12 | One-shot mode | `./app/parking_monitor --once` | One reading, exit 0 | ☐ |
| ST-13 | Module parameter | `make reload START=50` then `cat /dev/parksensor` | distance=50 cm zone=DANGER | ☐ |

## 5.6 Robustness Tests (shell)

```bash
# Load / unload cycles - no errors, no leftover /dev or /proc entries
for i in $(seq 1 20); do sudo insmod driver/parking_sensor.ko && sudo rmmod parking_sensor; done
ls /dev/parksensor /proc/parksensor      # should not exist afterwards
sudo dmesg | tail                        # no warnings / oopses

# Concurrent access - several readers while the car is moving
make load
echo "mode reverse" > /dev/parksensor
for i in $(seq 1 5); do (for j in $(seq 1 200); do cat /dev/parksensor > /dev/null; done) & done; wait
cat /proc/parksensor                     # samples counted, no crash

# Unload while the timer is running
echo "mode forward" > /dev/parksensor
sudo rmmod parking_sensor                # clean unload, no oops in dmesg

# Invalid input from the shell
echo "dist 999" > /dev/parksensor        # write error: Invalid argument
echo "hello"    > /dev/parksensor        # write error: Invalid argument
sudo insmod driver/parking_sensor.ko start_distance=1000   # rejected: Invalid parameters
```

## 5.7 Test Results

`make evidence` builds the project, loads the driver, runs both automated
suites and a scripted dashboard session (single reading, reverse parking with
auto-brake, /proc statistics, log), and stores all output with the kernel log
in [docs/test-results/](test-results/). The pass/fail summary is in
[test-results/README.md](test-results/README.md).

| Suite | Tests | Result |
|---|---|---|
| Unit | 11 | see [03_unit_tests.txt](test-results/03_unit_tests.txt) |
| Integration | 14 | see [04_driver_tests.txt](test-results/04_driver_tests.txt) |
| System (scripted) | ST-02, ST-03, ST-10, ST-11 | see [05_dashboard_demo.txt](test-results/05_dashboard_demo.txt) |
| System (manual) | 13 | tick the Result column in 5.5 |

## 5.8 Improvements in this Stage
| Area | Improvement |
|---|---|
| Reliability | Safe timer shutdown with `stopping` flag; complete error unwinding in `ps_init()` |
| Concurrency | All state access under spinlock; procfs prints from a snapshot |
| Code quality | `-Wall -Wextra -Wpedantic`, kernel coding style, one class per responsibility |
| Usability | Colored zones, re-prompt on invalid input, `--help` |
| Testability | Shared zone rule and I/O-free `AlertManager` covered by unit tests |
| Portability | Version checks for kernel API changes (5.15 – 6.x) |

## 5.9 Next Stage
Stage 6: final integration, demonstration and final report.
