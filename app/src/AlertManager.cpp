#include "AlertManager.hpp"

#include <stdexcept>

#include "parking_sensor_ioctl.h"

Zone AlertManager::toZone(int rawZone)
{
    switch (rawZone) {
    case PS_ZONE_SAFE:    return Zone::Safe;
    case PS_ZONE_CAUTION: return Zone::Caution;
    case PS_ZONE_DANGER:  return Zone::Danger;
    case PS_ZONE_STOP:    return Zone::Stop;
    default:
        throw std::invalid_argument("Unknown zone value " + std::to_string(rawZone));
    }
}

std::string AlertManager::zoneName(Zone z)
{
    switch (z) {
    case Zone::Safe:    return "SAFE";
    case Zone::Caution: return "CAUTION";
    case Zone::Danger:  return "DANGER";
    case Zone::Stop:    return "STOP";
    }
    return "?";
}

std::string AlertManager::message(Zone z)
{
    switch (z) {
    case Zone::Safe:    return "Path clear";
    case Zone::Caution: return "Obstacle nearby - slow down";
    case Zone::Danger:  return "Obstacle very close - prepare to stop";
    case Zone::Stop:    return "STOP! Obstacle at the bumper";
    }
    return "";
}

// Like a real parking aid: the closer the obstacle, the faster the beeps.
int AlertManager::beepIntervalMs(Zone z)
{
    switch (z) {
    case Zone::Safe:    return 0;
    case Zone::Caution: return 800;
    case Zone::Danger:  return 300;
    case Zone::Stop:    return 100;
    }
    return 0;
}

// The first update also returns true (initial state), but only real
// changes between two zones are counted as transitions.
bool AlertManager::update(Zone z)
{
    if (current_ && *current_ == z)
        return false;

    previous_ = current_;
    current_ = z;
    if (previous_)
        ++transitions_;
    return true;
}

void AlertManager::clear()
{
    current_.reset();
    previous_.reset();
    transitions_ = 0;
}
