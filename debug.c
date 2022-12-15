// SPDX-License-Identifier: GPL-2.0

/*
 * This file is part of the QM35 UCI stack for linux.
 *
 * Copyright (c) 2022 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo.
 * Please contact Qorvo to inquire about licensing terms.
 *
 * QM35 LOG layer HSSPI Protocol
 */

#include <linux/debugfs.h>
#include <linux/poll.h>
#include <linux/fsnotify.h>

#include <qmrom.h>
#include <qmrom_spi.h>

#include "qm35.h"
#include "debug.h"
#include "hsspi_test.h"

static const struct file_operations debug_enable_fops;
static const struct file_operations debug_log_level_fops;
static const struct file_operations debug_test_hsspi_sleep_fops;

static void *priv_from_file(const struct file *filp)
{
	return filp->f_path.dentry->d_inode->i_private;
}

static ssize_t debug_enable_write(struct file *filp, const char __user *buff,
				  size_t count, loff_t *off)
{
	struct debug *debug;
	u8 enabled;

	debug = priv_from_file(filp);

	if (kstrtou8_from_user(buff, count, 10, &enabled))
		return -EFAULT;

	if (debug->trace_ops)
		debug->trace_ops->enable_set(debug, enabled == 1 ? 1 : 0);
	else
		return -ENOSYS;

	return count;
}

static ssize_t debug_enable_read(struct file *filp, char __user *buff,
				 size_t count, loff_t *off)
{
	char enabled[2];
	struct debug *debug;

	debug = priv_from_file(filp);

	if (debug->trace_ops)
		enabled[0] = debug->trace_ops->enable_get(debug) + '0';
	else
		return -ENOSYS;

	enabled[1] = '\n';

	return simple_read_from_buffer(buff, count, off, enabled,
				       sizeof(enabled));
}

static ssize_t debug_log_level_write(struct file *filp, const char __user *buff,
				     size_t count, loff_t *off)
{
	u8 log_level = 0;
	struct log_module *log_module;

	log_module = priv_from_file(filp);
	if (kstrtou8_from_user(buff, count, 10, &log_level))
		return -EFAULT;

	if (log_module->debug->trace_ops)
		log_module->debug->trace_ops->level_set(log_module->debug,
							log_module, log_level);
	else
		return -ENOSYS;

	return count;
}

static ssize_t debug_log_level_read(struct file *filp, char __user *buff,
				    size_t count, loff_t *off)
{
	char log_level[2];
	struct log_module *log_module;

	log_module = priv_from_file(filp);

	if (log_module->debug->trace_ops)
		log_level[0] = log_module->debug->trace_ops->level_get(
				       log_module->debug, log_module) +
			       '0';
	else
		return -ENOSYS;

	log_level[1] = '\n';

	return simple_read_from_buffer(buff, count, off, log_level,
				       sizeof(log_level));
}

static ssize_t debug_test_hsspi_sleep_write(struct file *filp, const char __user *buff,
				     size_t count, loff_t *off)
{
	int sleep_inter_frame_ms;

	if (kstrtoint_from_user(buff, count, 10, &sleep_inter_frame_ms))
		return -EFAULT;

	hsspi_test_set_inter_frame_ms(sleep_inter_frame_ms);
	return count;
}

static ssize_t debug_traces_read(struct file *filp, char __user *buff,
				 size_t count, loff_t *off)
{
	char *entry;
	rb_entry_size_t entry_size;
	struct qm35_ctx *qm35_hdl;
	uint16_t ret;
	struct debug *debug;

	debug = priv_from_file(filp);
	qm35_hdl = container_of(debug, struct qm35_ctx, debug);

	if (!debug->trace_ops)
		return -ENOSYS;

	entry_size = debug->trace_ops->trace_get_next_size(debug);
	if (!entry_size) {
		if (filp->f_flags & O_NONBLOCK)
			return 0;

		ret = wait_event_interruptible(
			debug->wq,
			(entry_size = debug->trace_ops->trace_get_next_size(debug)));
		if (ret)
			return ret;
	}

	if (entry_size > count)
		return -EMSGSIZE;

	entry = debug->trace_ops->trace_get_next(debug, &entry_size);
	if (!entry)
		return 0;

	ret = copy_to_user(buff, entry, entry_size);

	kfree(entry);

	return ret ? -EFAULT : entry_size;
}

static __poll_t debug_traces_poll(struct file *filp,
				  struct poll_table_struct *wait)
{
	struct debug *debug;
	__poll_t mask = 0;

	debug = priv_from_file(filp);

	poll_wait(filp, &debug->wq, wait);

	if (debug->trace_ops && debug->trace_ops->trace_next_avail(debug))
		mask |= POLLIN;

	return mask;
}

static int debug_traces_open(struct inode *inodep, struct file *filep)
{
	struct debug *debug;

	debug = priv_from_file(filep);

	mutex_lock(&debug->pv_filp_lock);
	if (debug->pv_filp) {
		mutex_unlock(&debug->pv_filp_lock);
		return -EBUSY;
	}

	debug->pv_filp = filep;

	if (debug->trace_ops)
		debug->trace_ops->trace_reset(debug);

	mutex_unlock(&debug->pv_filp_lock);

	return 0;
}

static int debug_traces_release(struct inode *inodep, struct file *filep)
{
	struct debug *debug;

	debug = priv_from_file(filep);

	mutex_lock(&debug->pv_filp_lock);
	debug->pv_filp = NULL;
	mutex_unlock(&debug->pv_filp_lock);

	return 0;
}

static ssize_t debug_coredump_read(struct file *filep, char __user *buff,
				   size_t count, loff_t *off)
{
	struct qm35_ctx *qm35_hdl;
	struct debug *debug;
	char *cd;
	size_t cd_len = 0;

	debug = priv_from_file(filep);
	qm35_hdl = container_of(debug, struct qm35_ctx, debug);

	if (!debug->coredump_ops)
		return -ENOSYS;

	cd = debug->coredump_ops->coredump_get(debug, &cd_len);

	return simple_read_from_buffer(buff, count, off, cd, cd_len);
}

static ssize_t debug_coredump_write(struct file *filp, const char __user *buff,
				  size_t count, loff_t *off)
{
	struct debug *debug;
	u8 force;

	debug = priv_from_file(filp);

	if (kstrtou8_from_user(buff, count, 10, &force))
		return -EFAULT;

	if (debug->coredump_ops && force != 0)
		debug->coredump_ops->coredump_force(debug);
	else if (force == 0)
		pr_warn("qm35: write non null value to force coredump\n");
	else
		return -ENOSYS;

	return count;
}

static int debug_debug_certificate_open(struct inode *inodep, struct file *filep)
{
	struct debug *debug = priv_from_file(filep);

	if (debug->certificate != NULL)
		return -EBUSY;

	debug->certificate = kmalloc(sizeof(*debug->certificate) + DEBUG_CERTIFICATE_SIZE,
				     GFP_KERNEL);
	if (debug->certificate == NULL)
		return -ENOMEM;

	return 0;
}

static int debug_debug_certificate_close(struct inode *inodep, struct file *filep)
{
	struct debug *debug = priv_from_file(filep);
	struct qm35_ctx *qm35_hdl;
	int ret;
	const char *operation = filep->f_pos ? "flashing" : "erasing";

	qm35_hdl = container_of(debug, struct qm35_ctx, debug);

	qm35_hsspi_stop(qm35_hdl);

	dev_dbg(&qm35_hdl->spi->dev, "%s debug certificate (%lld bytes)\n", operation,
		filep->f_pos);

	if (filep->f_pos) {

		debug->certificate->size = filep->f_pos;

	} else {

		/* WA: qmrom_erase_dbg_cert is not working, waiting to find the root
		 * cause, workaround is to write a zeroed certificate
		 * ret = qmrom_erase_dbg_cert(qm35_hdl->spi, qmrom_spi_reset_device, qm35_hdl);
		 */
		debug->certificate->size = DEBUG_CERTIFICATE_SIZE;
		memset(debug->certificate->data, 0, DEBUG_CERTIFICATE_SIZE);
	}

	ret = qmrom_flash_dbg_cert(qm35_hdl->spi, debug->certificate,
							   qmrom_spi_reset_device, qm35_hdl);

	if (ret)
		dev_err(&qm35_hdl->spi->dev, "%s debug certificate fails: %d\n", operation, ret);
	else
		dev_info(&qm35_hdl->spi->dev, "%s debug certificate success\n", operation);

	msleep(QM_BEFORE_RESET_MS);
	qm35_reset(qm35_hdl, QM_RESET_LOW_MS);
	msleep(QM_BOOT_MS);

	qm35_hsspi_start(qm35_hdl);

	kfree(debug->certificate);
	debug->certificate = NULL;

	return 0;
}

static ssize_t debug_debug_certificate_write(struct file *filp,
					const char __user *buff, size_t count, loff_t *off)
{
	struct debug *debug = priv_from_file(filp);

	return simple_write_to_buffer(debug->certificate->data,
				      DEBUG_CERTIFICATE_SIZE, off, buff, count);
}

static const struct file_operations debug_enable_fops = {
	.owner = THIS_MODULE,
	.write = debug_enable_write,
	.read = debug_enable_read,
};

static const struct file_operations debug_log_level_fops = {
	.owner = THIS_MODULE,
	.write = debug_log_level_write,
	.read = debug_log_level_read
};

static const struct file_operations debug_test_hsspi_sleep_fops = {
	.owner = THIS_MODULE,
	.write = debug_test_hsspi_sleep_write
};

static const struct file_operations debug_traces_fops = {
	.owner = THIS_MODULE,
	.open = debug_traces_open,
	.release = debug_traces_release,
	.read = debug_traces_read,
	.poll = debug_traces_poll,
	.llseek = no_llseek,
};

static const struct file_operations debug_coredump_fops = {
	.owner = THIS_MODULE,
	.read = debug_coredump_read,
	.write = debug_coredump_write,
};

static const struct file_operations debug_debug_certificate_fops = {
	.owner = THIS_MODULE,
	.open = debug_debug_certificate_open,
	.write = debug_debug_certificate_write,
	.release = debug_debug_certificate_close,
};

int debug_create_module_entry(struct debug *debug,
			      struct log_module *log_module)
{
	struct dentry *dir;
	struct dentry *file;

	dir = debugfs_create_dir(log_module->name, debug->fw_dir);
	if (!dir) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/%s\n",
		       log_module->name);
		return -1;
	}

	file = debugfs_create_file("log_level", 0644, dir, log_module,
				   &debug_log_level_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/%s/log_level\n",
		       log_module->name);
		return -1;
	}

	pr_info("qm35 debug: created /sys/kernel/debug/uwb0/%s/log_level\n",
		log_module->name);

	return 0;
}

void debug_new_trace_available(struct debug *debug)
{
	if (debug->pv_filp)
		fsnotify_modify(debug->pv_filp);

	wake_up_interruptible(&debug->wq);
}

static int debug_devid_show(struct seq_file *s, void *unused)
{
	struct debug *debug = (struct debug *)s->private;
	uint8_t soc_id[ROM_SOC_ID_LEN];
	int rc;

	if (debug->trace_ops && debug->trace_ops->get_soc_id) {
		rc = debug->trace_ops->get_soc_id(debug, soc_id);
		if (rc < 0)
			return -EIO;
		seq_printf(s, "%*phN\n", ROM_SOC_ID_LEN, soc_id);
	}
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(debug_devid);

int debug_init(struct debug *debug, struct dentry *root)
{
	struct dentry *file;

	init_waitqueue_head(&debug->wq);
	mutex_init(&debug->pv_filp_lock);
	debug->pv_filp = NULL;

	debug->root_dir = debugfs_create_dir("uwb0", root);
	if (!debug->root_dir) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0\n");
		goto unregister;
	}

	debug->fw_dir = debugfs_create_dir("fw", debug->root_dir);
	if (!debug->fw_dir) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw\n");
		goto unregister;
	}

	file = debugfs_create_file("enable", 0644, debug->fw_dir, debug,
				   &debug_enable_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/enable\n");
		goto unregister;
	}

	file = debugfs_create_file("traces", 0444, debug->fw_dir, debug,
				   &debug_traces_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/traces\n");
		goto unregister;
	}

	file = debugfs_create_file("coredump", 0444, debug->fw_dir, debug,
				   &debug_coredump_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/coredump\n");
		goto unregister;
	}

	file = debugfs_create_file("devid", 0444, debug->fw_dir, debug,
				   &debug_devid_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/devid\n");
		goto unregister;
	}

	debug->certificate = NULL;
	file = debugfs_create_file("debug_certificate", 0200, debug->fw_dir, debug,
				&debug_debug_certificate_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/debug_certificate\n");
		goto unregister;
	}

	file = debugfs_create_file("test_sleep_hsspi_ms", 0200, debug->fw_dir, debug,
				   &debug_test_hsspi_sleep_fops);
	if (!file) {
		pr_err("qm35: failed to create /sys/kernel/debug/uwb0/fw/test_sleep_hsspi\n");
		goto unregister;
	}

	return 0;

unregister:
	debug_deinit(debug);
	return -1;
}

void debug_deinit(struct debug *debug)
{
	wake_up_interruptible(&debug->wq);
	debugfs_remove_recursive(debug->root_dir);
}
