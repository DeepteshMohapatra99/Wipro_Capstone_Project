#include "Display.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

const char* const kReset = "\033[0m";

const char* zoneColor(Zone z)
{
    switch (z) {
    case Zone::Safe:    return "\033[32m";        // green
    case Zone::Caution: return "\033[33m";        // yellow
    case Zone::Danger:  return "\033[31m";        // red
    case Zone::Stop:    return "\033[1;97;41m";   // bold white on red
    }
    return "";
}

} // namespace

std::string Display::distanceBar(int distanceCm)
{
    int clamped = std::clamp(distanceCm, PS_MIN_DISTANCE_CM, PS_MAX_DISTANCE_CM);
    return "[CAR]" + std::string(static_cast<std::size_t>(clamped / 10), '.') + "|WALL";
}

std::string Display::modeName(int mode)
{
    switch (mode) {
    case PS_MODE_IDLE:      return "IDLE";
    case PS_MODE_REVERSING: return "REVERSING";
    case PS_MODE_FORWARD:   return "FORWARD";
    }
    return "?";
}

std::string Display::colorize(const std::string& text, Zone z) const
{
    if (!useColor_)
        return text;
    return zoneColor(z) + text + kReset;
}

void Display::showReading(const ps_reading& r) const
{
    Zone z = AlertManager::toZone(r.zone);

    std::cout << "+--------------------------------------------------------+\n"
              << "  Distance : " << r.distance_cm << " cm\n"
              << "  Zone     : " << colorize(AlertManager::zoneName(z), z) << '\n'
              << "  Status   : " << AlertManager::message(z) << '\n'
              << "  Mode     : " << modeName(r.mode)
              << " (speed " << r.speed_cm << " cm per tick)\n"
              << "  Visual   : " << distanceBar(r.distance_cm) << '\n'
              << "  Sample # : " << r.sample_count << '\n'
              << "+--------------------------------------------------------+\n";
}

void Display::showLiveLine(const ps_reading& r) const
{
    Zone z = AlertManager::toZone(r.zone);

    std::ostringstream line;
    line << std::setw(4) << r.distance_cm << " cm  "
         << std::left << std::setw(8) << AlertManager::zoneName(z) << ' '
         << std::setw(51) << distanceBar(r.distance_cm);

    std::cout << '\r' << colorize(line.str(), z) << std::flush;
}

void Display::showThresholds(const ps_thresholds& t) const
{
    std::cout << "  SAFE    : distance >  " << t.caution_cm << " cm\n"
              << "  CAUTION : distance <= " << t.caution_cm << " cm\n"
              << "  DANGER  : distance <= " << t.danger_cm << " cm\n"
              << "  STOP    : distance <= " << t.stop_cm << " cm\n";
}
