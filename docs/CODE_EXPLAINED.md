# Code Explained (Simple Version)

This page explains the project in simple words, file by file, so you can
understand it and explain it in your evaluation.

---

## 1. The idea in one picture

```
   YOU
    │  type "2" in the menu (reverse park)
    ▼
 ┌──────────────────────┐   "what is the distance?"   ┌──────────────────────────┐
 │  C++ APP             │ ──────────────────────────▶ │  DRIVER (inside Linux)   │
 │  app/src/main.cpp    │                             │  driver/parking_sensor.c │
 │                      │ ◀────────────────────────── │                          │
 │  shows warning,      │   "60 cm, DANGER"           │  fake sensor: distance   │
 │  brakes at STOP      │                             │  goes down every 0.2 s   │
 └──────────────────────┘                             └──────────────────────────┘
```

- The **driver** pretends to be a parking sensor fixed on the back of a car.
- The **app** is the screen inside the car that shows the distance and warns you.
- They talk to each other through a special file: **`/dev/parksensor`**.

### The 4 warning zones

| Distance | Zone | What the app does |
|---|---|---|
| more than 150 cm | SAFE | nothing |
| 150 cm or less | CAUTION | slow beeps |
| 80 cm or less | DANGER | fast beeps |
| 30 cm or less | STOP | very fast beeps + **auto-brake** (stops the car) |

---

## 2. The driver – `driver/parking_sensor.c` (C, runs inside the Linux kernel)

A **driver** is code that runs inside the operating system and gives programs
access to a device. Our device is fake (simulated), but the driver is built
exactly like a real one.

### What happens when the driver is loaded (`sudo insmod parking_sensor.ko`)
Function **`ps_init()`**:
1. `alloc_chrdev_region()` – asks Linux for a device number (major/minor).
2. `cdev_add()` – tells Linux "these are my functions for open/read/write".
3. `class_create()` + `device_create()` – makes the file `/dev/parksensor` appear.
4. `proc_create_single()` – makes the file `/proc/parksensor` (statistics).
5. `timer_setup()` + `mod_timer()` – starts a timer that runs every 200 ms.

If any step fails, it undoes the previous steps (the `goto err_...` lines),
so nothing is left behind.

### What happens when the driver is removed (`sudo rmmod parking_sensor`)
Function **`ps_exit()`** stops the timer and removes everything in reverse order.

### The timer – `ps_timer_fn()`
Runs every 200 ms (5 times per second), like the car moving:
- mode **REVERSING** → distance goes **down** by the speed (default 5 cm)
- mode **FORWARD** → distance goes **up**
- mode **IDLE** → distance stays the same
- adds ±1 cm random "noise", like a real sensor
- if the distance reaches 0 cm or 400 cm, the car stops.

### The functions programs can call (`file_operations`)
| Function | Called when a program does... | What our driver does |
|---|---|---|
| `ps_open()` | `open("/dev/parksensor")` | counts how many times it was opened |
| `ps_read()` | `read()` or `cat /dev/parksensor` | returns text like `distance=60 cm zone=DANGER mode=IDLE` |
| `ps_write()` | `write()` or `echo "dist 60" > /dev/parksensor` | understands commands: `dist`, `speed`, `mode`, `reset` |
| `ps_ioctl()` | `ioctl()` | the main way the C++ app talks to the driver (see below) |
| `ps_release()` | `close()` | nothing special |

### ioctl commands (the "remote control" of the driver)
| Command | Meaning |
|---|---|
| `PS_IOC_GET_READING` | give me the distance, zone, mode |
| `PS_IOC_SET_DISTANCE` | put the car at X cm |
| `PS_IOC_SET_MODE` | start reversing / drive forward / stop |
| `PS_IOC_SET_SPEED` | how fast the car moves |
| `PS_IOC_SET_THRESHOLDS` / `GET_THRESHOLDS` | change / see the zone limits |
| `PS_IOC_RESET` | back to default values |

### Important kernel ideas used
| Idea | Where | Why |
|---|---|---|
| `copy_to_user` / `copy_from_user` | read, write, ioctl | The kernel must never touch program memory directly – these copy safely. |
| **spinlock** (`spin_lock_bh`) | everywhere the distance is used | The timer and the app can change the distance at the same time; the lock lets only one do it at a time. |
| Input checking | `ps_set_distance()` etc. | Wrong values (like 999 cm) are rejected with the error `EINVAL`. |
| `pr_info()` | zone changes | Writes messages to the kernel log – see them with `dmesg`. |
| Module parameter `start_distance` | top of file | Start the car at another distance: `insmod parking_sensor.ko start_distance=120`. |

---

## 3. The shared file – `include/parking_sensor_ioctl.h`

Both the driver (C) and the app (C++) include this file, so they always agree on:
- the ioctl command numbers,
- the data they exchange (`struct ps_reading`, `struct ps_thresholds`),
- the zone rule **`ps_classify()`** – distance in, zone out.

---

## 4. The app – `app/` (C++, normal program)

It is split into small classes, each with one job:

| File | Class | Job |
|---|---|---|
| `ParkingSensor.cpp` | **ParkingSensor** | Opens `/dev/parksensor` and calls `ioctl()`. Closes the file automatically in the destructor (**RAII**). |
| `SensorException.cpp` | **SensorException** | An error type. If a system call fails, we `throw` this with the error message. |
| `AlertManager.cpp` | **AlertManager** | Turns a zone into a message and a beep speed. Remembers the last zone to detect a **zone change**. |
| `Display.cpp` | **Display** | Prints to the screen: colors (green/yellow/red) and the bar `[CAR]......\|WALL`. |
| `Logger.cpp` | **Logger** | Writes events with date and time into `parking_log.txt`. |
| `main.cpp` | **App** + `main()` | The menu, and the live parking mode. |

### What happens in "2. Reverse park" – `App::liveMonitor()` in `main.cpp`
```
tell driver: mode = REVERSING
repeat every 100 ms:
    reading = sensor.read()                 ← ioctl to the driver
    if the zone changed → print message + write to log
    print the live line  " 79 cm  DANGER  [CAR].......|WALL"
    beep if it is time
    if zone == STOP → tell driver: mode = IDLE   (AUTO-BRAKE) and finish
    if user pressed Ctrl+C → stop the car and finish
```

### Ctrl+C handling – `SigintGuard` in `main.cpp`
Normally Ctrl+C kills a program. During live mode we catch the signal with
`sigaction()`, stop the car safely and go back to the menu.

---

## 5. The tests – `tests/`

| File | What it tests | Needs the driver? |
|---|---|---|
| `unit_tests.cpp` | Zone rule, AlertManager, Display, Logger (11 tests) | No |
| `driver_tests.cpp` | Real driver: set/get values, wrong values rejected, car moves, text commands, /proc (14 tests) | Yes |
| `test_framework.hpp` | Small home-made test tool: `TEST`, `CHECK`, `CHECK_EQ` | – |

Results from a real Linux run: [test-results/README.md](test-results/README.md) – **25 of 25 passed**.

---

## 6. Commands you need

```bash
make            # compile everything
make load       # load the driver   → /dev/parksensor appears
make run        # start the app
make test       # run all tests
make logs       # see the driver's messages (dmesg)
make unload     # remove the driver
```

Use the driver without the app:
```bash
cat /dev/parksensor                  # read distance
echo "dist 60" > /dev/parksensor     # put the car at 60 cm
echo "mode reverse" > /dev/parksensor
cat /proc/parksensor                 # statistics
```

---

## 7. Questions you may be asked

**Q: What is a character device driver?**
A driver that gives access to a device as a stream of bytes through a file in
`/dev`. Programs use normal `open`, `read`, `write`, `ioctl`, `close`.

**Q: What are major and minor numbers?**
The major number tells Linux which driver owns the device; the minor number
tells the driver which device it is (we have one, minor 0). We get them with
`alloc_chrdev_region()`.

**Q: Why `copy_to_user` and not `memcpy`?**
User memory may be invalid or not loaded. `copy_to_user` checks the address and
handles errors safely; `memcpy` could crash the kernel.

**Q: What is ioctl and why use it?**
A system call for device-specific commands (like "set speed"). It sends
structured binary data, which is faster and safer than parsing text.

**Q: Why a spinlock and not a mutex?**
The timer function runs in "softirq" context where the code is not allowed
to sleep. A mutex can sleep, a spinlock does not.

**Q: What is RAII?**
"Resource Acquisition Is Initialization": the constructor opens the device and
the destructor closes it, so the file is always closed, even after an error.

**Q: What is /proc?**
A virtual filesystem the kernel uses to show information. Our driver creates
`/proc/parksensor` to show statistics.

**Q: Is the sensor real?**
No, it is simulated by a kernel timer. A real HC-SR04 ultrasonic sensor on a
Raspberry Pi could replace the timer while keeping the same ioctl interface,
so the app would not change.

**Q: What are the limitations / future work?**
Only one simulated sensor, the app checks the driver repeatedly (polling), and
it has a text interface only. Future: real hardware with GPIO, `poll()`
support, more sensors, a Qt GUI.
