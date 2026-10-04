#include "ParkingSensor.hpp"
#include "SensorException.hpp"

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

ParkingSensor::ParkingSensor(const std::string& path)
    : path_(path), fd_(::open(path.c_str(), O_RDWR))
{
    if (fd_ < 0)
        throw SensorException("Cannot open " + path_ + " (is the driver loaded?)", errno);
}

ParkingSensor::~ParkingSensor()
{
    if (fd_ >= 0)
        ::close(fd_);
}

void ParkingSensor::control(unsigned long request, void* arg, const char* what) const
{
    if (::ioctl(fd_, request, arg) < 0)
        throw SensorException(what, errno);
}

ps_reading ParkingSensor::read() const
{
    ps_reading r{};
    control(PS_IOC_GET_READING, &r, "Read sensor");
    return r;
}

std::string ParkingSensor::readText() const
{
    // The driver returns one line per open(), so use a fresh descriptor.
    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0)
        throw SensorException("Cannot open " + path_, errno);

    char buf[128];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    int err = errno;
    ::close(fd);

    if (n < 0)
        throw SensorException("Read text sample", err);
    return std::string(buf, static_cast<std::size_t>(n));
}

void ParkingSensor::setDistance(int cm)
{
    int32_t v = cm;
    control(PS_IOC_SET_DISTANCE, &v, "Set distance");
}

void ParkingSensor::setMode(ps_mode mode)
{
    int32_t v = mode;
    control(PS_IOC_SET_MODE, &v, "Set mode");
}

void ParkingSensor::setSpeed(int cmPerTick)
{
    int32_t v = cmPerTick;
    control(PS_IOC_SET_SPEED, &v, "Set speed");
}

void ParkingSensor::setThresholds(const ps_thresholds& t)
{
    ps_thresholds copy = t;
    control(PS_IOC_SET_THRESHOLDS, &copy, "Set thresholds");
}

ps_thresholds ParkingSensor::thresholds() const
{
    ps_thresholds t{};
    control(PS_IOC_GET_THRESHOLDS, &t, "Get thresholds");
    return t;
}

void ParkingSensor::reset()
{
    control(PS_IOC_RESET, nullptr, "Reset sensor");
}
