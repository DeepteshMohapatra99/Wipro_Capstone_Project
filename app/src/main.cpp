// parking_monitor - user-space dashboard for the Virtual Parking Sensor driver.
//
// Talks to /dev/parksensor through the ParkingSensor class (open/read/ioctl),
// shows the distance to the obstacle, raises zone alerts, beeps faster as the
// obstacle gets closer, applies an automatic brake in the STOP zone and logs
// every event to a file.

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <signal.h>
#include <unistd.h>

#include "AlertManager.hpp"
#include "Display.hpp"
#include "Logger.hpp"
#include "ParkingSensor.hpp"
#include "SensorException.hpp"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void onSigint(int)
{
    g_stop = 1;
}

// Installs the Ctrl+C handler only while live monitoring is running and
// restores the previous behaviour afterwards (RAII).
class SigintGuard {
public:
    SigintGuard()
    {
        g_stop = 0;
        struct sigaction sa {};
        sa.sa_handler = onSigint;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, &old_);
    }
    ~SigintGuard() { sigaction(SIGINT, &old_, nullptr); }

    SigintGuard(const SigintGuard&) = delete;
    SigintGuard& operator=(const SigintGuard&) = delete;

private:
    struct sigaction old_ {};
};

struct Options {
    std::string device = PS_DEVICE_PATH;
    std::string logFile = "parking_log.txt";
    bool color = true;
    bool bell = false;
    bool autoBrake = true;
    bool once = false;
};

// Reads an integer in [min, max]; re-asks on bad input. nullopt means EOF.
std::optional<int> readInt(const std::string& prompt, int min, int max)
{
    std::string line;
    while (true) {
        std::cout << prompt << std::flush;
        if (!std::getline(std::cin, line))
            return std::nullopt;

        std::istringstream iss(line);
        int value;
        char extra;
        if ((iss >> value) && !(iss >> extra) && value >= min && value <= max)
            return value;

        std::cout << "  Please enter a number between " << min << " and " << max << ".\n";
    }
}

class App {
public:
    explicit App(const Options& opt)
        : opt_(opt), sensor_(opt.device), logger_(opt.logFile), display_(opt.color)
    {
    }

    int run();
    void singleReading();

private:
    void showMenu() const;
    void liveMonitor(std::optional<ps_mode> mode);
    void setDistance();
    void setSpeed();
    void setThresholds();
    void showThresholds();
    void showDriverStats();
    void viewLog();
    void resetSensor();

    Options opt_;
    ParkingSensor sensor_;
    Logger logger_;
    Display display_;
    AlertManager alerts_;
};

void App::showMenu() const
{
    std::cout << "\n========== Virtual Parking Sensor ==========\n"
              << "  1. Take a single reading\n"
              << "  2. Reverse park (live monitor)\n"
              << "  3. Drive forward (live monitor)\n"
              << "  4. Live monitor without changing mode\n"
              << "  5. Set distance manually\n"
              << "  6. Set vehicle speed\n"
              << "  7. Set zone thresholds\n"
              << "  8. Show zone thresholds\n"
              << "  9. Show driver statistics (/proc)\n"
              << " 10. View log file\n"
              << " 11. Reset sensor\n"
              << "  0. Exit\n"
              << "============================================\n";
}

int App::run()
{
    logger_.log(LogLevel::Info, "Application started, device " + sensor_.path());
    std::cout << "Connected to " << sensor_.path() << ", logging to " << logger_.path() << '\n';

    while (true) {
        showMenu();
        std::optional<int> choice = readInt("Choice: ", 0, 11);
        if (!choice || *choice == 0)
            break;

        try {
            switch (*choice) {
            case 1:  singleReading(); break;
            case 2:  liveMonitor(PS_MODE_REVERSING); break;
            case 3:  liveMonitor(PS_MODE_FORWARD); break;
            case 4:  liveMonitor(std::nullopt); break;
            case 5:  setDistance(); break;
            case 6:  setSpeed(); break;
            case 7:  setThresholds(); break;
            case 8:  showThresholds(); break;
            case 9:  showDriverStats(); break;
            case 10: viewLog(); break;
            case 11: resetSensor(); break;
            }
        } catch (const SensorException& e) {
            std::cout << "  Error: " << e.what() << '\n';
            logger_.log(LogLevel::Error, e.what());
        }
    }

    logger_.log(LogLevel::Info, "Application exited");
    std::cout << "Goodbye.\n";
    return 0;
}

void App::singleReading()
{
    ps_reading r = sensor_.read();
    display_.showReading(r);

    Zone z = AlertManager::toZone(r.zone);
    logger_.log(z == Zone::Safe ? LogLevel::Info : LogLevel::Warn,
                "Reading " + std::to_string(r.distance_cm) + " cm, zone " +
                    AlertManager::zoneName(z));
}

void App::liveMonitor(std::optional<ps_mode> mode)
{
    using namespace std::chrono;

    SigintGuard guard;
    alerts_.clear();

    if (mode)
        sensor_.setMode(*mode);
    logger_.log(LogLevel::Info, "Live monitor started" +
                (mode ? ", mode " + Display::modeName(*mode) : std::string()));
    std::cout << "\nLive monitoring - press Ctrl+C to stop\n";

    auto lastBeep = steady_clock::now();

    while (!g_stop) {
        ps_reading r = sensor_.read();
        Zone z = AlertManager::toZone(r.zone);

        if (alerts_.update(z) && alerts_.previous()) {
            std::string msg = "Zone " + AlertManager::zoneName(*alerts_.previous()) + " -> " +
                              AlertManager::zoneName(z) + " at " +
                              std::to_string(r.distance_cm) + " cm";
            std::cout << '\n' << display_.colorize("  >> " + msg + ": " + AlertManager::message(z), z)
                      << '\n';
            logger_.log(z == Zone::Safe ? LogLevel::Info
                        : z == Zone::Stop ? LogLevel::Alert : LogLevel::Warn, msg);
        }

        display_.showLiveLine(r);

        int interval = AlertManager::beepIntervalMs(z);
        auto now = steady_clock::now();
        if (opt_.bell && interval > 0 && now - lastBeep >= milliseconds(interval)) {
            std::cout << '\a' << std::flush;
            lastBeep = now;
        }

        // Safety feature: stop the car before it hits the obstacle.
        if (opt_.autoBrake && z == Zone::Stop && r.mode == PS_MODE_REVERSING) {
            sensor_.setMode(PS_MODE_IDLE);
            std::string msg = "AUTO-BRAKE engaged at " + std::to_string(r.distance_cm) + " cm";
            std::cout << '\n' << display_.colorize("  " + msg + " - vehicle stopped", z) << '\n';
            logger_.log(LogLevel::Alert, msg);
            return;
        }

        // The driver stops the car itself when it reaches 0 or 400 cm.
        if (mode && *mode != PS_MODE_IDLE && r.mode == PS_MODE_IDLE) {
            std::cout << "\n  Vehicle stopped at " << r.distance_cm << " cm\n";
            logger_.log(LogLevel::Info, "Vehicle stopped at " + std::to_string(r.distance_cm) + " cm");
            return;
        }

        std::this_thread::sleep_for(milliseconds(PS_UPDATE_INTERVAL_MS / 2));
    }

    sensor_.setMode(PS_MODE_IDLE);
    std::cout << "\n  Monitoring stopped by user, vehicle stopped\n";
    logger_.log(LogLevel::Info, "Live monitor stopped by user");
}

void App::setDistance()
{
    auto cm = readInt("  Distance in cm (0-400): ", PS_MIN_DISTANCE_CM, PS_MAX_DISTANCE_CM);
    if (!cm)
        return;
    sensor_.setDistance(*cm);
    logger_.log(LogLevel::Info, "Distance set to " + std::to_string(*cm) + " cm");
    singleReading();
}

void App::setSpeed()
{
    auto speed = readInt("  Speed in cm per tick (1-50): ", 1, PS_MAX_SPEED_CM);
    if (!speed)
        return;
    sensor_.setSpeed(*speed);
    logger_.log(LogLevel::Info, "Speed set to " + std::to_string(*speed) + " cm per tick");
    std::cout << "  Speed updated.\n";
}

void App::setThresholds()
{
    auto caution = readInt("  CAUTION threshold (cm): ", 0, PS_MAX_DISTANCE_CM);
    if (!caution)
        return;
    auto danger = readInt("  DANGER threshold (cm): ", 0, PS_MAX_DISTANCE_CM);
    if (!danger)
        return;
    auto stop = readInt("  STOP threshold (cm): ", 0, PS_MAX_DISTANCE_CM);
    if (!stop)
        return;

    ps_thresholds t{*caution, *danger, *stop};
    if (!ps_thresholds_valid(&t)) {
        std::cout << "  Invalid: thresholds must satisfy CAUTION > DANGER > STOP.\n";
        return;
    }
    sensor_.setThresholds(t);
    logger_.log(LogLevel::Info, "Thresholds set to caution=" + std::to_string(t.caution_cm) +
                                    " danger=" + std::to_string(t.danger_cm) +
                                    " stop=" + std::to_string(t.stop_cm));
    std::cout << "  Thresholds updated.\n";
    display_.showThresholds(t);
}

void App::showThresholds()
{
    display_.showThresholds(sensor_.thresholds());
}

void App::showDriverStats()
{
    std::ifstream proc("/proc/" PS_PROC_NAME);
    if (!proc) {
        std::cout << "  /proc/" PS_PROC_NAME " is not available\n";
        return;
    }
    std::string line;
    while (std::getline(proc, line))
        std::cout << "  " << line << '\n';
}

void App::viewLog()
{
    std::cout << "  Last entries of " << logger_.path() << ":\n";
    for (const std::string& line : logger_.tail(20))
        std::cout << "  " << line << '\n';
}

void App::resetSensor()
{
    sensor_.reset();
    logger_.log(LogLevel::Info, "Sensor reset to defaults");
    std::cout << "  Sensor reset to defaults.\n";
}

void printUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --device PATH    sensor device (default " PS_DEVICE_PATH ")\n"
              << "  --log FILE       log file (default parking_log.txt)\n"
              << "  --once           print one reading and exit\n"
              << "  --bell           beep with the terminal bell\n"
              << "  --no-color       disable colored output\n"
              << "  --no-autobrake   do not stop the car in the STOP zone\n"
              << "  --help           show this help\n";
}

} // namespace

int main(int argc, char* argv[])
{
    Options opt;
    opt.color = ::isatty(STDOUT_FILENO);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc) {
            opt.device = argv[++i];
        } else if (arg == "--log" && i + 1 < argc) {
            opt.logFile = argv[++i];
        } else if (arg == "--once") {
            opt.once = true;
        } else if (arg == "--bell") {
            opt.bell = true;
        } else if (arg == "--no-color") {
            opt.color = false;
        } else if (arg == "--no-autobrake") {
            opt.autoBrake = false;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << '\n';
            printUsage(argv[0]);
            return 2;
        }
    }

    try {
        App app(opt);
        if (opt.once) {
            app.singleReading();
            return 0;
        }
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
