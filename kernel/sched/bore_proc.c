/*
 *  Burst-Oriented Response Enhancer (BORE) CPU Scheduler
 *  Copyright (C) 2025 Rudi Setiyawan <diphons@gmail.com>
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "sched.h"

static int sched_min_base_slice_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sysctl_sched_min_base_slice);
	return 0;
}

static ssize_t sched_min_base_slice_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char buf[16];

	if (count > 15)
		count = 15;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtoint(buf, 10, &sysctl_sched_min_base_slice))
		return -EINVAL;

	return count;
}

static int sched_min_base_slice_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sched_min_base_slice_show, NULL);
}

static const struct proc_ops sched_min_base_slice_proc_fops = {
    .proc_open = sched_min_base_slice_proc_open,
    .proc_read = seq_read,
    .proc_write = sched_min_base_slice_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int sched_base_slice_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sysctl_sched_base_slice);
	return 0;
}

static ssize_t sched_base_slice_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char buf[16];

	if (count > 15)
		count = 15;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtoint(buf, 10, &sysctl_sched_base_slice))
		return -EINVAL;

	return count;
}

static int sched_base_slice_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sched_base_slice_show, NULL);
}

static const struct proc_ops sched_base_slice_proc_fops = {
    .proc_open = sched_base_slice_proc_open,
    .proc_read = seq_read,
    .proc_write = sched_base_slice_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init bore_proc_init(void)
{
    proc_create("min_base_slice_ns", 0644, NULL, &sched_min_base_slice_proc_fops);
    proc_create("base_slice_ns", 0444, NULL, &sched_base_slice_proc_fops);
    return 0;
}

static void __exit bore_proc_exit(void)
{
    remove_proc_entry("min_base_slice_ns", NULL);
    remove_proc_entry("base_slice_ns", NULL);
}

module_init(bore_proc_init);
module_exit(bore_proc_exit);
MODULE_LICENSE("GPL");
