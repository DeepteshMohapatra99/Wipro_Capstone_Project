#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <string>

#include "AlertManager.hpp"
#include "parking_sensor_ioctl.h"

// Terminal rendering of sensor data (the "dashboard screen" of the car).
class Display {
public:
    explicit Display(bool useColor = true) : useColor_(useColor) {}

    // "[CAR]............|WALL" - one dot per 10 cm.
    static std::string distanceBar(int distanceCm);
    static std::string modeName(int mode);

    void showReading(const ps_reading& r) const;      // multi-line card
    void showLiveLine(const ps_reading& r) const;     // single refreshing line
    void showThresholds(const ps_thresholds& t) const;

    std::string colorize(const std::string& text, Zone z) const;

private:
    bool useColor_;
};

#endif // DISPLAY_HPP
