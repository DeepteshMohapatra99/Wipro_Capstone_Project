// Unit tests: pure logic, no kernel module needed.
//   make -C tests unit

#include <cstdio>

#include "AlertManager.hpp"
#include "Display.hpp"
#include "Logger.hpp"
#include "parking_sensor_ioctl.h"
#include "test_framework.hpp"

static const ps_thresholds kDefaults{PS_DEFAULT_CAUTION_CM, PS_DEFAULT_DANGER_CM,
                                     PS_DEFAULT_STOP_CM};

// ---- shared classification rule (used by the driver too) -------------------

TEST(classify_default_thresholds)
{
    CHECK_EQ(ps_classify(400, &kDefaults), PS_ZONE_SAFE);
    CHECK_EQ(ps_classify(151, &kDefaults), PS_ZONE_SAFE);
    CHECK_EQ(ps_classify(150, &kDefaults), PS_ZONE_CAUTION);
    CHECK_EQ(ps_classify(81, &kDefaults), PS_ZONE_CAUTION);
    CHECK_EQ(ps_classify(80, &kDefaults), PS_ZONE_DANGER);
    CHECK_EQ(ps_classify(31, &kDefaults), PS_ZONE_DANGER);
    CHECK_EQ(ps_classify(30, &kDefaults), PS_ZONE_STOP);
    CHECK_EQ(ps_classify(0, &kDefaults), PS_ZONE_STOP);
}

TEST(classify_custom_thresholds)
{
    ps_thresholds t{100, 50, 10};
    CHECK_EQ(ps_classify(101, &t), PS_ZONE_SAFE);
    CHECK_EQ(ps_classify(100, &t), PS_ZONE_CAUTION);
    CHECK_EQ(ps_classify(50, &t), PS_ZONE_DANGER);
    CHECK_EQ(ps_classify(10, &t), PS_ZONE_STOP);
}

TEST(thresholds_validation)
{
    ps_thresholds equal{80, 80, 30};
    ps_thresholds reversed{30, 80, 150};
    ps_thresholds negative{150, 80, -1};
    ps_thresholds tooLarge{401, 80, 30};

    CHECK(ps_thresholds_valid(&kDefaults));
    CHECK(!ps_thresholds_valid(&equal));
    CHECK(!ps_thresholds_valid(&reversed));
    CHECK(!ps_thresholds_valid(&negative));
    CHECK(!ps_thresholds_valid(&tooLarge));
}

// ---- AlertManager ----------------------------------------------------------

TEST(zone_conversion)
{
    CHECK(AlertManager::toZone(PS_ZONE_SAFE) == Zone::Safe);
    CHECK(AlertManager::toZone(PS_ZONE_CAUTION) == Zone::Caution);
    CHECK(AlertManager::toZone(PS_ZONE_DANGER) == Zone::Danger);
    CHECK(AlertManager::toZone(PS_ZONE_STOP) == Zone::Stop);
    CHECK_THROWS(AlertManager::toZone(42));
    CHECK_THROWS(AlertManager::toZone(-1));
}

TEST(zone_names)
{
    CHECK_EQ(AlertManager::zoneName(Zone::Safe), std::string("SAFE"));
    CHECK_EQ(AlertManager::zoneName(Zone::Caution), std::string("CAUTION"));
    CHECK_EQ(AlertManager::zoneName(Zone::Danger), std::string("DANGER"));
    CHECK_EQ(AlertManager::zoneName(Zone::Stop), std::string("STOP"));
}

TEST(beeps_get_faster_when_closer)
{
    CHECK_EQ(AlertManager::beepIntervalMs(Zone::Safe), 0);
    CHECK(AlertManager::beepIntervalMs(Zone::Caution) > AlertManager::beepIntervalMs(Zone::Danger));
    CHECK(AlertManager::beepIntervalMs(Zone::Danger) > AlertManager::beepIntervalMs(Zone::Stop));
    CHECK(AlertManager::beepIntervalMs(Zone::Stop) > 0);
}

TEST(alert_state_transitions)
{
    AlertManager a;
    CHECK(!a.current().has_value());

    CHECK(a.update(Zone::Safe));          // initial state
    CHECK_EQ(a.transitions(), 0);
    CHECK(!a.update(Zone::Safe));         // no change
    CHECK(a.update(Zone::Caution));       // SAFE -> CAUTION
    CHECK(a.update(Zone::Danger));        // CAUTION -> DANGER
    CHECK_EQ(a.transitions(), 2);
    CHECK(a.current() == Zone::Danger);
    CHECK(a.previous() == Zone::Caution);

    a.clear();
    CHECK(!a.current().has_value());
    CHECK_EQ(a.transitions(), 0);
}

// ---- Display ---------------------------------------------------------------

TEST(distance_bar)
{
    CHECK_EQ(Display::distanceBar(0), std::string("[CAR]|WALL"));
    CHECK_EQ(Display::distanceBar(120), std::string("[CAR]............|WALL"));
    CHECK_EQ(Display::distanceBar(9), std::string("[CAR]|WALL"));
    CHECK_EQ(Display::distanceBar(-5), Display::distanceBar(0));        // clamped
    CHECK_EQ(Display::distanceBar(999), Display::distanceBar(400));     // clamped
    CHECK_EQ(Display::distanceBar(400).size(), std::string("[CAR]|WALL").size() + 40);
}

TEST(mode_names)
{
    CHECK_EQ(Display::modeName(PS_MODE_IDLE), std::string("IDLE"));
    CHECK_EQ(Display::modeName(PS_MODE_REVERSING), std::string("REVERSING"));
    CHECK_EQ(Display::modeName(PS_MODE_FORWARD), std::string("FORWARD"));
    CHECK_EQ(Display::modeName(7), std::string("?"));
}

TEST(colorize_disabled_returns_plain_text)
{
    Display plain(false);
    Display colored(true);
    CHECK_EQ(plain.colorize("STOP", Zone::Stop), std::string("STOP"));
    CHECK(colored.colorize("STOP", Zone::Stop) != "STOP");
}

// ---- Logger ----------------------------------------------------------------

TEST(logger_writes_and_tails)
{
    const char* path = "unit_test_log.tmp";
    std::remove(path);
    {
        Logger log(path);
        log.log(LogLevel::Info, "first entry");
        log.log(LogLevel::Warn, "second entry");
        log.log(LogLevel::Alert, "third entry");
    }
    Logger log(path);
    auto last = log.tail(2);
    CHECK_EQ(last.size(), static_cast<std::size_t>(2));
    CHECK(last[0].find("[WARN] second entry") != std::string::npos);
    CHECK(last[1].find("[ALERT] third entry") != std::string::npos);
    CHECK_EQ(log.tail(100).size(), static_cast<std::size_t>(3));
    std::remove(path);
}

int main()
{
    std::cout << "=== Unit tests ===\n";
    return runAllTests();
}
