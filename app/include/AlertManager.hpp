#ifndef ALERT_MANAGER_HPP
#define ALERT_MANAGER_HPP

#include <optional>
#include <string>

enum class Zone { Safe, Caution, Danger, Stop };

// Converts driver zones into driver-facing alerts (message + beep rate)
// and tracks zone transitions. Contains no I/O so it can be unit tested
// without the kernel module.
class AlertManager {
public:
    static Zone toZone(int rawZone);             // throws std::invalid_argument
    static std::string zoneName(Zone z);
    static std::string message(Zone z);
    static int beepIntervalMs(Zone z);           // 0 = silent

    // Feed the latest zone. Returns true if it differs from the previous one.
    bool update(Zone z);

    std::optional<Zone> current() const { return current_; }
    std::optional<Zone> previous() const { return previous_; }
    int transitions() const { return transitions_; }
    void clear();

private:
    std::optional<Zone> current_;
    std::optional<Zone> previous_;
    int transitions_ = 0;
};

#endif // ALERT_MANAGER_HPP
