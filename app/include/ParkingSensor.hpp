#ifndef PARKING_SENSOR_HPP
#define PARKING_SENSOR_HPP

#include <string>

#include "parking_sensor_ioctl.h"

// RAII wrapper around the /dev/parksensor character device.
// The device is opened in the constructor and closed in the destructor;
// every failing system call is reported as a SensorException.
class ParkingSensor {
public:
    explicit ParkingSensor(const std::string& path = PS_DEVICE_PATH);
    ~ParkingSensor();

    ParkingSensor(const ParkingSensor&) = delete;
    ParkingSensor& operator=(const ParkingSensor&) = delete;

    ps_reading read() const;               // binary sample via ioctl
    std::string readText() const;          // text sample via read()

    void setDistance(int cm);
    void setMode(ps_mode mode);
    void setSpeed(int cmPerTick);
    void setThresholds(const ps_thresholds& t);
    ps_thresholds thresholds() const;
    void reset();

    const std::string& path() const { return path_; }

private:
    void control(unsigned long request, void* arg, const char* what) const;

    std::string path_;
    int fd_;
};

#endif // PARKING_SENSOR_HPP
