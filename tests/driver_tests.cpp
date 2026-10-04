// Integration tests: user space <-> kernel driver.
// The parking_sensor module must be loaded first (make load).
//   make -C tests integration

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

#include "ParkingSensor.hpp"
#include "SensorException.hpp"
#include "test_framework.hpp"

static void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Returns the errno of a failing call, or 0 if it did not fail.
template <typename Fn>
static int errorOf(Fn fn)
{
    try {
        fn();
    } catch (const SensorException& e) {
        return e.error();
    }
    return 0;
}

TEST(open_missing_device_fails)
{
    CHECK_EQ(errorOf([] { ParkingSensor s("/dev/does_not_exist"); }), ENOENT);
}

TEST(reset_restores_defaults)
{
    ParkingSensor s;
    s.setDistance(42);
    s.setSpeed(20);
    s.reset();

    ps_reading r = s.read();
    CHECK_EQ(r.distance_cm, PS_DEFAULT_DISTANCE_CM);
    CHECK_EQ(r.zone, PS_ZONE_SAFE);
    CHECK_EQ(r.mode, PS_MODE_IDLE);
    CHECK_EQ(r.speed_cm, PS_DEFAULT_SPEED_CM);

    ps_thresholds t = s.thresholds();
    CHECK_EQ(t.caution_cm, PS_DEFAULT_CAUTION_CM);
    CHECK_EQ(t.danger_cm, PS_DEFAULT_DANGER_CM);
    CHECK_EQ(t.stop_cm, PS_DEFAULT_STOP_CM);
}

TEST(zones_follow_distance)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(200);
    CHECK_EQ(s.read().zone, PS_ZONE_SAFE);
    s.setDistance(120);
    CHECK_EQ(s.read().zone, PS_ZONE_CAUTION);
    s.setDistance(50);
    CHECK_EQ(s.read().zone, PS_ZONE_DANGER);
    s.setDistance(10);
    CHECK_EQ(s.read().zone, PS_ZONE_STOP);
}

TEST(invalid_values_are_rejected)
{
    ParkingSensor s;
    s.reset();
    CHECK_EQ(errorOf([&] { s.setDistance(-1); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setDistance(401); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setSpeed(0); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setSpeed(PS_MAX_SPEED_CM + 1); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setMode(static_cast<ps_mode>(3)); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setThresholds({80, 80, 30}); }), EINVAL);
    CHECK_EQ(errorOf([&] { s.setThresholds({30, 80, 150}); }), EINVAL);

    // State must be unchanged after rejected requests.
    CHECK_EQ(s.read().distance_cm, PS_DEFAULT_DISTANCE_CM);
}

TEST(custom_thresholds_change_zone)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(100);
    CHECK_EQ(s.read().zone, PS_ZONE_CAUTION);
    s.setThresholds({120, 110, 105});
    CHECK_EQ(s.read().zone, PS_ZONE_STOP);
    s.reset();
}

TEST(idle_distance_is_constant)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(123);
    sleepMs(600);
    CHECK_EQ(s.read().distance_cm, 123);
}

TEST(reversing_reduces_distance)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(300);
    s.setSpeed(10);
    s.setMode(PS_MODE_REVERSING);
    sleepMs(1000);                         // about 5 timer ticks
    s.setMode(PS_MODE_IDLE);

    int d = s.read().distance_cm;
    CHECK(d < 300);
    CHECK(d > 200);
}

TEST(forward_increases_distance)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(100);
    s.setSpeed(10);
    s.setMode(PS_MODE_FORWARD);
    sleepMs(1000);
    s.setMode(PS_MODE_IDLE);

    int d = s.read().distance_cm;
    CHECK(d > 100);
    CHECK(d < 200);
}

TEST(vehicle_stops_at_zero)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(20);
    s.setSpeed(PS_MAX_SPEED_CM);
    s.setMode(PS_MODE_REVERSING);
    sleepMs(800);

    ps_reading r = s.read();
    CHECK_EQ(r.distance_cm, 0);
    CHECK_EQ(r.mode, PS_MODE_IDLE);
    CHECK_EQ(r.zone, PS_ZONE_STOP);
}

TEST(sample_counter_increments)
{
    ParkingSensor s;
    s.reset();
    CHECK_EQ(s.read().sample_count, 1u);
    CHECK_EQ(s.read().sample_count, 2u);
}

TEST(text_read_interface)
{
    ParkingSensor s;
    s.reset();
    s.setDistance(123);
    std::string text = s.readText();
    CHECK(text.find("distance=123 cm") != std::string::npos);
    CHECK(text.find("zone=CAUTION") != std::string::npos);
    CHECK(text.find("mode=IDLE") != std::string::npos);
}

TEST(text_write_interface)
{
    ParkingSensor s;
    s.reset();

    int fd = ::open(PS_DEVICE_PATH, O_WRONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    const char ok[] = "dist 77\n";
    CHECK_EQ(::write(fd, ok, std::strlen(ok)), static_cast<ssize_t>(std::strlen(ok)));
    CHECK_EQ(s.read().distance_cm, 77);

    const char bad[] = "fly away\n";
    CHECK_EQ(::write(fd, bad, std::strlen(bad)), static_cast<ssize_t>(-1));
    CHECK_EQ(errno, EINVAL);

    ::close(fd);
    s.reset();
}

TEST(unknown_ioctl_is_rejected)
{
    int fd = ::open(PS_DEVICE_PATH, O_RDWR);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    CHECK_EQ(::ioctl(fd, _IO(PS_IOC_MAGIC, 99)), -1);
    CHECK_EQ(errno, ENOTTY);
    ::close(fd);
}

TEST(proc_statistics_available)
{
    std::ifstream proc("/proc/" PS_PROC_NAME);
    CHECK(proc.good());
    std::stringstream content;
    content << proc.rdbuf();
    CHECK(content.str().find("distance_cm") != std::string::npos);
    CHECK(content.str().find("zone_changes") != std::string::npos);
}

int main()
{
    std::cout << "=== Driver integration tests (" PS_DEVICE_PATH ") ===\n";
    return runAllTests();
}
