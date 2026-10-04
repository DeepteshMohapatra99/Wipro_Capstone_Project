// SPDX-License-Identifier: GPL-2.0
/*
 * parking_sensor.c - Virtual Parking Sensor character device driver.
 *
 * Simulates an ultrasonic distance sensor mounted on the rear bumper of a car.
 * A kernel timer moves the simulated vehicle every PS_UPDATE_INTERVAL_MS
 * (towards the obstacle when REVERSING, away from it when FORWARD) and the
 * driver classifies the distance into SAFE / CAUTION / DANGER / STOP zones.
 *
 * Interfaces exposed to user space:
 *   /dev/parksensor   read()  -> human readable sample   (cat /dev/parksensor)
 *                     write() -> text commands           (echo "dist 120" > ...)
 *                     ioctl() -> binary API used by the C++ application
 *   /proc/parksensor  driver statistics
 *
 * Linux driver concepts used: dynamic major/minor allocation, cdev,
 * device class + udev node creation, file_operations, copy_to/from_user,
 * ioctl, kernel timers, spinlocks, procfs (seq_file) and module parameters.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/version.h>

#include "parking_sensor_ioctl.h"

#define PS_CLASS_NAME "parksensor_class"

/* ---------------------------------------------------------------------- */
/* Module parameter                                                       */
/* ---------------------------------------------------------------------- */

static int start_distance = PS_DEFAULT_DISTANCE_CM;
module_param(start_distance, int, 0444);
MODULE_PARM_DESC(start_distance, "Initial distance to the obstacle in cm (0-400)");

/* ---------------------------------------------------------------------- */
/* Device state                                                           */
/* ---------------------------------------------------------------------- */

struct ps_device {
	/* character device plumbing */
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct proc_dir_entry *proc;

	/* simulation */
	struct timer_list timer;
	bool stopping;               /* set on unload so the timer stops re-arming */

	/* sensor state, protected by lock (shared with the timer softirq) */
	spinlock_t lock;
	int32_t distance_cm;
	int32_t mode;
	int32_t speed_cm;
	int32_t zone;
	struct ps_thresholds thr;

	/* statistics */
	uint32_t sample_count;
	unsigned long zone_changes;
	unsigned long open_count;
};

static struct ps_device ps;

static const char *const zone_names[] = { "SAFE", "CAUTION", "DANGER", "STOP" };
static const char *const mode_names[] = { "IDLE", "REVERSING", "FORWARD" };

static const char *ps_zone_name(int zone)
{
	return (zone >= 0 && zone < (int)ARRAY_SIZE(zone_names)) ? zone_names[zone] : "?";
}

static const char *ps_mode_name(int mode)
{
	return (mode >= 0 && mode < (int)ARRAY_SIZE(mode_names)) ? mode_names[mode] : "?";
}

/* ---------------------------------------------------------------------- */
/* Sensor logic (all *_locked helpers expect ps.lock to be held)          */
/* ---------------------------------------------------------------------- */

static void ps_update_zone_locked(void)
{
	int32_t new_zone = ps_classify(ps.distance_cm, &ps.thr);

	if (new_zone != ps.zone) {
		pr_info("parksensor: zone %s -> %s at %d cm\n",
			ps_zone_name(ps.zone), ps_zone_name(new_zone), ps.distance_cm);
		ps.zone = new_zone;
		ps.zone_changes++;
	}
}

static void ps_reset_locked(void)
{
	ps.distance_cm = start_distance;
	ps.mode = PS_MODE_IDLE;
	ps.speed_cm = PS_DEFAULT_SPEED_CM;
	ps.thr.caution_cm = PS_DEFAULT_CAUTION_CM;
	ps.thr.danger_cm = PS_DEFAULT_DANGER_CM;
	ps.thr.stop_cm = PS_DEFAULT_STOP_CM;
	ps.sample_count = 0;
	ps.zone = ps_classify(ps.distance_cm, &ps.thr);
}

/* Small random jitter (-1, 0 or +1 cm) like a real ultrasonic sensor. */
static int32_t ps_noise(void)
{
	u8 r;

	get_random_bytes(&r, sizeof(r));
	return (int32_t)(r % 3) - 1;
}

static void ps_take_sample(struct ps_reading *r)
{
	spin_lock_bh(&ps.lock);
	ps.sample_count++;
	r->distance_cm = ps.distance_cm;
	r->zone = ps.zone;
	r->mode = ps.mode;
	r->speed_cm = ps.speed_cm;
	r->sample_count = ps.sample_count;
	spin_unlock_bh(&ps.lock);
}

static int ps_set_distance(int32_t cm)
{
	if (cm < PS_MIN_DISTANCE_CM || cm > PS_MAX_DISTANCE_CM)
		return -EINVAL;

	spin_lock_bh(&ps.lock);
	ps.distance_cm = cm;
	ps_update_zone_locked();
	spin_unlock_bh(&ps.lock);
	return 0;
}

static int ps_set_mode(int32_t mode)
{
	if (mode < PS_MODE_IDLE || mode > PS_MODE_FORWARD)
		return -EINVAL;

	spin_lock_bh(&ps.lock);
	ps.mode = mode;
	spin_unlock_bh(&ps.lock);
	pr_info("parksensor: mode set to %s\n", ps_mode_name(mode));
	return 0;
}

static int ps_set_speed(int32_t speed)
{
	if (speed < 1 || speed > PS_MAX_SPEED_CM)
		return -EINVAL;

	spin_lock_bh(&ps.lock);
	ps.speed_cm = speed;
	spin_unlock_bh(&ps.lock);
	return 0;
}

static int ps_set_thresholds(const struct ps_thresholds *t)
{
	if (!ps_thresholds_valid(t))
		return -EINVAL;

	spin_lock_bh(&ps.lock);
	ps.thr = *t;
	ps_update_zone_locked();
	spin_unlock_bh(&ps.lock);
	pr_info("parksensor: thresholds caution=%d danger=%d stop=%d\n",
		t->caution_cm, t->danger_cm, t->stop_cm);
	return 0;
}

static void ps_reset(void)
{
	spin_lock_bh(&ps.lock);
	ps_reset_locked();
	spin_unlock_bh(&ps.lock);
	pr_info("parksensor: reset to defaults\n");
}

/* ---------------------------------------------------------------------- */
/* Kernel timer: moves the simulated vehicle                               */
/* ---------------------------------------------------------------------- */

static void ps_timer_fn(struct timer_list *t)
{
	int32_t step;

	spin_lock(&ps.lock);

	if (ps.mode != PS_MODE_IDLE) {
		step = ps.speed_cm + ps_noise();
		if (ps.mode == PS_MODE_REVERSING)
			ps.distance_cm -= step;
		else
			ps.distance_cm += step;

		ps.distance_cm = clamp_t(int32_t, ps.distance_cm,
					 PS_MIN_DISTANCE_CM, PS_MAX_DISTANCE_CM);
		ps_update_zone_locked();

		/* The car cannot move any further: stop the simulation. */
		if ((ps.mode == PS_MODE_REVERSING && ps.distance_cm == PS_MIN_DISTANCE_CM) ||
		    (ps.mode == PS_MODE_FORWARD && ps.distance_cm == PS_MAX_DISTANCE_CM)) {
			pr_info("parksensor: limit reached at %d cm, vehicle stopped\n",
				ps.distance_cm);
			ps.mode = PS_MODE_IDLE;
		}
	}

	if (!ps.stopping)
		mod_timer(&ps.timer, jiffies + msecs_to_jiffies(PS_UPDATE_INTERVAL_MS));

	spin_unlock(&ps.lock);
}

/* ---------------------------------------------------------------------- */
/* file_operations                                                        */
/* ---------------------------------------------------------------------- */

static int ps_open(struct inode *inode, struct file *file)
{
	spin_lock_bh(&ps.lock);
	ps.open_count++;
	spin_unlock_bh(&ps.lock);
	pr_debug("parksensor: device opened\n");
	return 0;
}

static int ps_release(struct inode *inode, struct file *file)
{
	pr_debug("parksensor: device closed\n");
	return 0;
}

/* Returns one text sample per open(), so "cat /dev/parksensor" terminates. */
static ssize_t ps_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
	struct ps_reading r;
	char kbuf[96];
	size_t n;

	if (*off > 0)
		return 0;

	ps_take_sample(&r);
	n = scnprintf(kbuf, sizeof(kbuf), "distance=%d cm zone=%s mode=%s\n",
		      r.distance_cm, ps_zone_name(r.zone), ps_mode_name(r.mode));
	n = min(n, len);

	if (copy_to_user(buf, kbuf, n))
		return -EFAULT;

	*off += n;
	return n;
}

/*
 * Text command interface, handy for testing from the shell:
 *   dist <cm> | speed <cm> | mode idle|reverse|forward | reset
 */
static ssize_t ps_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
	char kbuf[32];
	char *cmd;
	int val;
	int ret;

	if (len == 0 || len >= sizeof(kbuf))
		return -EINVAL;
	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';
	cmd = strim(kbuf);

	if (!strcmp(cmd, "reset")) {
		ps_reset();
		ret = 0;
	} else if (!strncmp(cmd, "dist ", 5)) {
		ret = kstrtoint(cmd + 5, 10, &val);
		if (!ret)
			ret = ps_set_distance(val);
	} else if (!strncmp(cmd, "speed ", 6)) {
		ret = kstrtoint(cmd + 6, 10, &val);
		if (!ret)
			ret = ps_set_speed(val);
	} else if (!strcmp(cmd, "mode idle")) {
		ret = ps_set_mode(PS_MODE_IDLE);
	} else if (!strcmp(cmd, "mode reverse")) {
		ret = ps_set_mode(PS_MODE_REVERSING);
	} else if (!strcmp(cmd, "mode forward")) {
		ret = ps_set_mode(PS_MODE_FORWARD);
	} else {
		ret = -EINVAL;
	}

	return ret ? ret : (ssize_t)len;
}

static long ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	struct ps_reading r;
	struct ps_thresholds t;
	int32_t val;

	if (_IOC_TYPE(cmd) != PS_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case PS_IOC_GET_READING:
		ps_take_sample(&r);
		return copy_to_user(uarg, &r, sizeof(r)) ? -EFAULT : 0;

	case PS_IOC_SET_DISTANCE:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;
		return ps_set_distance(val);

	case PS_IOC_SET_MODE:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;
		return ps_set_mode(val);

	case PS_IOC_SET_SPEED:
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;
		return ps_set_speed(val);

	case PS_IOC_SET_THRESHOLDS:
		if (copy_from_user(&t, uarg, sizeof(t)))
			return -EFAULT;
		return ps_set_thresholds(&t);

	case PS_IOC_GET_THRESHOLDS:
		spin_lock_bh(&ps.lock);
		t = ps.thr;
		spin_unlock_bh(&ps.lock);
		return copy_to_user(uarg, &t, sizeof(t)) ? -EFAULT : 0;

	case PS_IOC_RESET:
		ps_reset();
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations ps_fops = {
	.owner          = THIS_MODULE,
	.open           = ps_open,
	.release        = ps_release,
	.read           = ps_read,
	.write          = ps_write,
	.unlocked_ioctl = ps_ioctl,
};

/* ---------------------------------------------------------------------- */
/* /proc/parksensor                                                       */
/* ---------------------------------------------------------------------- */

static int ps_proc_show(struct seq_file *m, void *v)
{
	struct ps_reading r;
	struct ps_thresholds thr;
	unsigned long zone_changes, open_count;

	/* Take a consistent snapshot, then print without holding the lock. */
	spin_lock_bh(&ps.lock);
	r.distance_cm = ps.distance_cm;
	r.zone = ps.zone;
	r.mode = ps.mode;
	r.speed_cm = ps.speed_cm;
	r.sample_count = ps.sample_count;
	thr = ps.thr;
	zone_changes = ps.zone_changes;
	open_count = ps.open_count;
	spin_unlock_bh(&ps.lock);

	seq_puts(m, "Virtual Parking Sensor - driver statistics\n");
	seq_printf(m, "device        : %s (major %d, minor %d)\n", PS_DEVICE_PATH,
		   MAJOR(ps.devt), MINOR(ps.devt));
	seq_printf(m, "distance_cm   : %d\n", r.distance_cm);
	seq_printf(m, "zone          : %s\n", ps_zone_name(r.zone));
	seq_printf(m, "mode          : %s\n", ps_mode_name(r.mode));
	seq_printf(m, "speed_cm/tick : %d (tick = %d ms)\n", r.speed_cm,
		   PS_UPDATE_INTERVAL_MS);
	seq_printf(m, "thresholds    : caution<=%d danger<=%d stop<=%d\n",
		   thr.caution_cm, thr.danger_cm, thr.stop_cm);
	seq_printf(m, "samples       : %u\n", r.sample_count);
	seq_printf(m, "zone_changes  : %lu\n", zone_changes);
	seq_printf(m, "opens         : %lu\n", open_count);
	return 0;
}

/* ---------------------------------------------------------------------- */
/* Module init / exit                                                     */
/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
static char *ps_devnode(const struct device *dev, umode_t *mode)
#else
static char *ps_devnode(struct device *dev, umode_t *mode)
#endif
{
	if (mode)
		*mode = 0666;   /* let normal users run the application */
	return NULL;
}

static int __init ps_init(void)
{
	int ret;

	if (start_distance < PS_MIN_DISTANCE_CM || start_distance > PS_MAX_DISTANCE_CM) {
		pr_err("parksensor: start_distance must be %d-%d\n",
		       PS_MIN_DISTANCE_CM, PS_MAX_DISTANCE_CM);
		return -EINVAL;
	}

	spin_lock_init(&ps.lock);
	ps_reset_locked();      /* no concurrency yet, lock not needed */

	ret = alloc_chrdev_region(&ps.devt, 0, 1, PS_DEVICE_NAME);
	if (ret) {
		pr_err("parksensor: alloc_chrdev_region failed (%d)\n", ret);
		return ret;
	}

	cdev_init(&ps.cdev, &ps_fops);
	ps.cdev.owner = THIS_MODULE;
	ret = cdev_add(&ps.cdev, ps.devt, 1);
	if (ret) {
		pr_err("parksensor: cdev_add failed (%d)\n", ret);
		goto err_region;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	ps.class = class_create(PS_CLASS_NAME);
#else
	ps.class = class_create(THIS_MODULE, PS_CLASS_NAME);
#endif
	if (IS_ERR(ps.class)) {
		ret = PTR_ERR(ps.class);
		pr_err("parksensor: class_create failed (%d)\n", ret);
		goto err_cdev;
	}
	ps.class->devnode = ps_devnode;

	ps.device = device_create(ps.class, NULL, ps.devt, NULL, PS_DEVICE_NAME);
	if (IS_ERR(ps.device)) {
		ret = PTR_ERR(ps.device);
		pr_err("parksensor: device_create failed (%d)\n", ret);
		goto err_class;
	}

	ps.proc = proc_create_single(PS_PROC_NAME, 0444, NULL, ps_proc_show);
	if (!ps.proc) {
		ret = -ENOMEM;
		pr_err("parksensor: proc entry creation failed\n");
		goto err_device;
	}

	ps.stopping = false;
	timer_setup(&ps.timer, ps_timer_fn, 0);
	mod_timer(&ps.timer, jiffies + msecs_to_jiffies(PS_UPDATE_INTERVAL_MS));

	pr_info("parksensor: loaded, %s (major %d), start distance %d cm\n",
		PS_DEVICE_PATH, MAJOR(ps.devt), start_distance);
	return 0;

err_device:
	device_destroy(ps.class, ps.devt);
err_class:
	class_destroy(ps.class);
err_cdev:
	cdev_del(&ps.cdev);
err_region:
	unregister_chrdev_region(ps.devt, 1);
	return ret;
}

static void __exit ps_exit(void)
{
	spin_lock_bh(&ps.lock);
	ps.stopping = true;
	spin_unlock_bh(&ps.lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
	timer_delete_sync(&ps.timer);
#else
	del_timer_sync(&ps.timer);
#endif

	proc_remove(ps.proc);
	device_destroy(ps.class, ps.devt);
	class_destroy(ps.class);
	cdev_del(&ps.cdev);
	unregister_chrdev_region(ps.devt, 1);
	pr_info("parksensor: unloaded\n");
}

module_init(ps_init);
module_exit(ps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deeptesh Mohapatra");
MODULE_DESCRIPTION("Virtual Parking Sensor - simulated ultrasonic distance sensor driver");
MODULE_VERSION("1.0");
