/*
 * parking_sensor_ioctl.h - Interface shared by the kernel driver and user space.
 *
 * This header is included by:
 *   - driver/parking_sensor.c   (kernel space, C)
 *   - app/ and tests/           (user space, C++)
 *
 * It defines the ioctl commands, the data structures exchanged with the
 * driver and the distance -> zone classification rule, so that both sides
 * always agree on the same values.
 */
#ifndef PARKING_SENSOR_IOCTL_H
#define PARKING_SENSOR_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#define PS_DEVICE_NAME   "parksensor"
#define PS_DEVICE_PATH   "/dev/parksensor"
#define PS_PROC_NAME     "parksensor"

/* Physical limits of the simulated ultrasonic sensor (like an HC-SR04). */
#define PS_MIN_DISTANCE_CM      0
#define PS_MAX_DISTANCE_CM      400

/* Default values used at load time and after a reset. */
#define PS_DEFAULT_DISTANCE_CM  300
#define PS_DEFAULT_SPEED_CM     5    /* cm moved per update tick           */
#define PS_DEFAULT_CAUTION_CM   150  /* distance <= 150 cm -> CAUTION      */
#define PS_DEFAULT_DANGER_CM    80   /* distance <=  80 cm -> DANGER       */
#define PS_DEFAULT_STOP_CM      30   /* distance <=  30 cm -> STOP         */
#define PS_UPDATE_INTERVAL_MS   200  /* kernel timer period                */
#define PS_MAX_SPEED_CM         50

/* Proximity zones reported by the sensor. */
enum ps_zone {
	PS_ZONE_SAFE = 0,
	PS_ZONE_CAUTION,
	PS_ZONE_DANGER,
	PS_ZONE_STOP
};

/* Movement mode of the simulated vehicle. */
enum ps_mode {
	PS_MODE_IDLE = 0,    /* vehicle stopped, distance constant    */
	PS_MODE_REVERSING,   /* vehicle moving towards the obstacle   */
	PS_MODE_FORWARD      /* vehicle moving away from the obstacle */
};

/* One sensor sample, returned by PS_IOC_GET_READING. */
struct ps_reading {
	int32_t  distance_cm;
	int32_t  zone;          /* enum ps_zone  */
	int32_t  mode;          /* enum ps_mode  */
	int32_t  speed_cm;
	uint32_t sample_count;  /* number of samples taken since load/reset */
};

/* Zone thresholds. Must satisfy: caution > danger > stop >= 0. */
struct ps_thresholds {
	int32_t caution_cm;
	int32_t danger_cm;
	int32_t stop_cm;
};

#define PS_IOC_MAGIC 'p'

#define PS_IOC_GET_READING     _IOR(PS_IOC_MAGIC, 1, struct ps_reading)
#define PS_IOC_SET_DISTANCE    _IOW(PS_IOC_MAGIC, 2, int32_t)
#define PS_IOC_SET_MODE        _IOW(PS_IOC_MAGIC, 3, int32_t)
#define PS_IOC_SET_SPEED       _IOW(PS_IOC_MAGIC, 4, int32_t)
#define PS_IOC_SET_THRESHOLDS  _IOW(PS_IOC_MAGIC, 5, struct ps_thresholds)
#define PS_IOC_GET_THRESHOLDS  _IOR(PS_IOC_MAGIC, 6, struct ps_thresholds)
#define PS_IOC_RESET           _IO(PS_IOC_MAGIC, 7)

/* Distance -> zone rule, shared so kernel and user space never disagree. */
static inline int ps_classify(int32_t distance_cm, const struct ps_thresholds *t)
{
	if (distance_cm <= t->stop_cm)
		return PS_ZONE_STOP;
	if (distance_cm <= t->danger_cm)
		return PS_ZONE_DANGER;
	if (distance_cm <= t->caution_cm)
		return PS_ZONE_CAUTION;
	return PS_ZONE_SAFE;
}

/* Returns 1 if the thresholds are valid, 0 otherwise. */
static inline int ps_thresholds_valid(const struct ps_thresholds *t)
{
	return t->stop_cm >= PS_MIN_DISTANCE_CM &&
	       t->danger_cm > t->stop_cm &&
	       t->caution_cm > t->danger_cm &&
	       t->caution_cm <= PS_MAX_DISTANCE_CM;
}

#endif /* PARKING_SENSOR_IOCTL_H */
