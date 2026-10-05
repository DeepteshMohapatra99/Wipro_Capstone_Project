# UML Diagrams

All diagrams are written in Mermaid and render directly on GitHub.

---

## 1. Class Diagram (user-space application)

```mermaid
classDiagram
    direction LR

    class App {
        -Options opt_
        -ParkingSensor sensor_
        -Logger logger_
        -Display display_
        -AlertManager alerts_
        +run() int
        +singleReading() void
        -liveMonitor(mode) void
        -setDistance() void
        -setSpeed() void
        -setThresholds() void
        -showThresholds() void
        -showDriverStats() void
        -viewLog() void
        -resetSensor() void
    }

    class ParkingSensor {
        -string path_
        -int fd_
        +ParkingSensor(path)
        +~ParkingSensor()
        +read() ps_reading
        +readText() string
        +setDistance(cm) void
        +setMode(mode) void
        +setSpeed(cmPerTick) void
        +setThresholds(t) void
        +thresholds() ps_thresholds
        +reset() void
        -control(request, arg, what) void
    }

    class AlertManager {
        -optional~Zone~ current_
        -optional~Zone~ previous_
        -int transitions_
        +toZone(raw) Zone$
        +zoneName(z) string$
        +message(z) string$
        +beepIntervalMs(z) int$
        +update(z) bool
        +current() optional~Zone~
        +previous() optional~Zone~
        +transitions() int
        +clear() void
    }

    class Display {
        -bool useColor_
        +distanceBar(cm) string$
        +modeName(mode) string$
        +showReading(r) void
        +showLiveLine(r) void
        +showThresholds(t) void
        +colorize(text, z) string
    }

    class Logger {
        -string path_
        -ofstream out_
        +log(level, msg) void
        +tail(lines) vector~string~
        +levelName(level) string$
    }

    class SensorException {
        -int err_
        +error() int
    }

    class Zone {
        <<enumeration>>
        Safe
        Caution
        Danger
        Stop
    }

    class LogLevel {
        <<enumeration>>
        Info
        Warn
        Alert
        Error
    }

    class ps_reading {
        <<struct>>
        +int32_t distance_cm
        +int32_t zone
        +int32_t mode
        +int32_t speed_cm
        +uint32_t sample_count
    }

    class ps_thresholds {
        <<struct>>
        +int32_t caution_cm
        +int32_t danger_cm
        +int32_t stop_cm
    }

    App *-- ParkingSensor
    App *-- Logger
    App *-- Display
    App *-- AlertManager
    ParkingSensor ..> SensorException : throws
    ParkingSensor ..> ps_reading : returns
    ParkingSensor ..> ps_thresholds : uses
    AlertManager ..> Zone
    Display ..> AlertManager : uses
    Logger ..> LogLevel
    SensorException --|> runtime_error
```

## 2. Kernel Driver Structure (component view)

```mermaid
classDiagram
    class ps_device {
        <<kernel struct>>
        +dev_t devt
        +cdev cdev
        +struct_class* cls
        +device* device
        +proc_dir_entry* proc
        +timer_list timer
        +bool stopping
        +spinlock_t lock
        +int32_t distance_cm
        +int32_t mode
        +int32_t speed_cm
        +int32_t zone
        +ps_thresholds thr
        +uint32_t sample_count
        +ulong zone_changes
        +ulong open_count
    }

    class file_operations {
        <<kernel interface>>
        +ps_open()
        +ps_release()
        +ps_read()
        +ps_write()
        +ps_ioctl()
    }

    class SensorLogic {
        <<static functions>>
        +ps_take_sample()
        +ps_set_distance()
        +ps_set_mode()
        +ps_set_speed()
        +ps_set_thresholds()
        +ps_reset()
        +ps_update_zone_locked()
    }

    class Timer {
        <<softirq>>
        +ps_timer_fn()
    }

    class Procfs {
        +ps_proc_show()
    }

    file_operations --> SensorLogic
    Timer --> ps_device : updates
    SensorLogic --> ps_device : locks + updates
    Procfs --> ps_device : reads snapshot
```

---

## 3. Sequence Diagram – Reverse parking with auto-brake

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant App as App (main.cpp)
    participant PS as ParkingSensor
    participant AM as AlertManager
    participant Log as Logger
    participant K as /dev/parksensor (driver)
    participant T as Kernel timer

    User->>App: choose "2. Reverse park"
    App->>PS: setMode(REVERSING)
    PS->>K: ioctl(PS_IOC_SET_MODE)
    K-->>PS: 0
    App->>Log: log("Live monitor started")

    loop every 200 ms
        T->>K: ps_timer_fn(): distance -= speed ± 1, update zone
    end

    loop every 100 ms until STOP / Ctrl+C
        App->>PS: read()
        PS->>K: ioctl(PS_IOC_GET_READING)
        K-->>PS: ps_reading {distance, zone, mode}
        PS-->>App: ps_reading
        App->>AM: update(zone)
        alt zone changed
            AM-->>App: true
            App->>User: "Zone SAFE -> CAUTION at 149 cm"
            App->>Log: log(WARN, transition)
        else same zone
            AM-->>App: false
        end
        App->>User: live line + beep (rate by zone)
    end

    Note over App,K: zone == STOP while REVERSING
    App->>PS: setMode(IDLE)
    PS->>K: ioctl(PS_IOC_SET_MODE, IDLE)
    App->>User: "AUTO-BRAKE engaged at 29 cm"
    App->>Log: log(ALERT, "AUTO-BRAKE engaged")
```

## 4. Sequence Diagram – Module load and first read

```mermaid
sequenceDiagram
    actor Admin
    participant Kernel
    participant Drv as parking_sensor.ko
    participant udev
    participant Shell

    Admin->>Kernel: insmod parking_sensor.ko
    Kernel->>Drv: ps_init()
    Drv->>Kernel: alloc_chrdev_region, cdev_add
    Drv->>Kernel: class_create, device_create
    Kernel->>udev: uevent
    udev-->>Admin: /dev/parksensor (0666)
    Drv->>Kernel: proc_create_single, timer_setup, mod_timer
    Shell->>Drv: open + read (cat /dev/parksensor)
    Drv-->>Shell: "distance=300 cm zone=SAFE mode=IDLE"
```

---

## 5. State Machine – Proximity zones

Zone transitions depend only on the distance and the thresholds
(defaults: caution 150, danger 80, stop 30 cm).

```mermaid
stateDiagram-v2
    state "SAFE (no beep)" as SAFE
    state "CAUTION (beep every 800 ms)" as CAUTION
    state "DANGER (beep every 300 ms)" as DANGER
    state "STOP (beep every 100 ms + auto-brake)" as STOP

    [*] --> SAFE : distance > 150

    SAFE --> CAUTION : distance <= 150
    CAUTION --> SAFE : distance > 150
    CAUTION --> DANGER : distance <= 80
    DANGER --> CAUTION : distance > 80
    DANGER --> STOP : distance <= 30
    STOP --> DANGER : distance > 30

```

## 6. State Machine – Vehicle mode (driver)

```mermaid
stateDiagram-v2
    state "IDLE (distance constant)" as IDLE
    state "REVERSING (distance -= speed ± 1 per tick)" as REVERSING
    state "FORWARD (distance += speed ± 1 per tick)" as FORWARD

    [*] --> IDLE : module loaded / reset

    IDLE --> REVERSING : SET_MODE(REVERSING)
    IDLE --> FORWARD : SET_MODE(FORWARD)
    REVERSING --> IDLE : SET_MODE(IDLE) / auto-brake / Ctrl+C
    REVERSING --> IDLE : distance reached 0 cm
    FORWARD --> IDLE : SET_MODE(IDLE) / Ctrl+C
    FORWARD --> IDLE : distance reached 400 cm
    REVERSING --> FORWARD : SET_MODE(FORWARD)
    FORWARD --> REVERSING : SET_MODE(REVERSING)
```

## 7. State Machine – Application

```mermaid
stateDiagram-v2
    [*] --> Connecting
    Connecting --> Menu : device opened
    Connecting --> [*] : open failed (driver not loaded)

    Menu --> SingleReading : 1
    SingleReading --> Menu
    Menu --> LiveMonitor : 2 / 3 / 4
    LiveMonitor --> Menu : auto-brake / limit reached / Ctrl+C
    Menu --> Configure : 5 / 6 / 7 / 11
    Configure --> Menu
    Menu --> Info : 8 / 9 / 10
    Info --> Menu
    Menu --> [*] : 0 or EOF
```
