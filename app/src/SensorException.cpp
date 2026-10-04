#include "SensorException.hpp"

#include <cstring>

SensorException::SensorException(const std::string& what, int err)
    : std::runtime_error(what + ": " + std::strerror(err)), err_(err)
{
}
