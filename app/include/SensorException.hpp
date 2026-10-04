#ifndef SENSOR_EXCEPTION_HPP
#define SENSOR_EXCEPTION_HPP

#include <stdexcept>
#include <string>

// Thrown when a system call on the sensor device fails.
// Keeps the errno value so callers can react to specific errors.
class SensorException : public std::runtime_error {
public:
    SensorException(const std::string& what, int err);

    int error() const noexcept { return err_; }

private:
    int err_;
};

#endif // SENSOR_EXCEPTION_HPP
