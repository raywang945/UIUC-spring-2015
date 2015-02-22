#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("16");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG          1
#define FILENAME       "status"
#define DIRECTORY      "mp1"
#define TIMEINTERVAL   5000

typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

static struct proc_dir_entry *proc_dir, *proc_entry;
LIST_HEAD(mp1_proc_list);
static struct timer_list mp1_timer;
static struct workqueue_struct *mp1_workqueue;
static spinlock_t mp1_lock;
static struct work_struct *mp1_work;

static ssize_t mp1_read(struct file *file, char __user *buffer, size_t count, loff_t *data)
{
    unsigned long copied = 0;
    char *buf;
    proc_list *tmp;
    unsigned long flags;

    buf = (char *)kmalloc(count, GFP_KERNEL);

    // enter critical section
    spin_lock_irqsave(&mp1_lock, flags);
    list_for_each_entry(tmp, &mp1_proc_list, list) {
        copied += sprintf(buf + copied, "%u: %u\n", tmp->pid, jiffies_to_msecs(cputime_to_jiffies(tmp->cpu_time)));
    }
    spin_unlock_irqrestore(&mp1_lock, flags);

    buf[copied] = '\0';

    copy_to_user(buffer, buf, copied);

    kfree(buf);

    return copied;
}

static ssize_t mp1_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
    proc_list *tmp;
    unsigned long flags;
    char *buf;

    // initialize tmp->list
    tmp = (proc_list *)kmalloc(sizeof(proc_list), GFP_KERNEL);
    INIT_LIST_HEAD(&(tmp->list));

    // set tmp->pid
    buf = (char *)kmalloc(count + 1, GFP_KERNEL);
    copy_from_user(buf, buffer, count);
    buf[count] = '\0';
    sscanf(buf, "%u", &tmp->pid);

    // initial tmp->cpu_time
    tmp->cpu_time = 0;

    // add tmp to mp1_proc_list
    spin_lock_irqsave(&mp1_lock, flags);
    list_add(&(tmp->list), &mp1_proc_list);
    spin_unlock_irqrestore(&mp1_lock, flags);

    kfree(buf);

    return count;
}

static const struct file_operations mp1_fops = {
    .owner   = THIS_MODULE,
    .read    = mp1_read,
    .write   = mp1_write
};

void mp1_timer_callback(unsigned long data)
{
    queue_work(mp1_workqueue, mp1_work);
}

static void mp1_work_function(struct work_struct *work)
{
    unsigned long flags;
    proc_list *pos, *n;

    // enter critical section
    spin_lock_irqsave(&mp1_lock, flags);
    list_for_each_entry_safe(pos, n, &mp1_proc_list, list) {
        // unregister the process from mp1_proc_list if the process does not exist
        if (get_cpu_use(pos->pid, &pos->cpu_time) == -1) {
            list_del(&pos->list);
            kfree(pos);
        }
    }
    spin_unlock_irqrestore(&mp1_lock, flags);

    mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEINTERVAL));
}

// mp1_init - Called when module is loaded
static int __init mp1_init(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP1 MODULE LOADING\n");
    #endif
    // Insert your code here ...

    // create /proc/mp1
    if ((proc_dir = proc_mkdir(DIRECTORY, NULL)) == NULL) {
        printk(KERN_INFO "proc_mkdir ERROR\n");
        return -ENOMEM;
    }
    // create /proc/mp1/status
    if ((proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_fops)) == NULL) {
        remove_proc_entry(DIRECTORY, NULL);
        printk(KERN_INFO "proc_create ERROR\n");
        return -ENOMEM;
    }

    // initialize and start timer
    setup_timer(&mp1_timer, mp1_timer_callback, 0);
    mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEINTERVAL));

    // initialize workqueue
    if ((mp1_workqueue = create_workqueue("mp1_workqueue")) == NULL) {
        printk(KERN_INFO "create_workqueue ERROR\n");
        return -ENOMEM;
    }

    // initialize work
    mp1_work = (struct work_struct *)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    INIT_WORK(mp1_work, mp1_work_function);

    // initialize lock
    spin_lock_init(&mp1_lock);

    printk(KERN_ALERT "MP1 MODULE LOADED\n");
    return 0;
}

// mp1_exit - Called when module is unloaded
static void __exit mp1_exit(void)
{
    proc_list *pos, *n;

    #ifdef DEBUG
    printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
    #endif
    // Insert your code here ...

    // remove /proc/mp1/status
    remove_proc_entry(FILENAME, proc_dir);
    // remove /proc/mp1
    remove_proc_entry(DIRECTORY, NULL);

    // free timer
    del_timer_sync(&mp1_timer);

    // free mp1_proc_list
    list_for_each_entry_safe(pos, n, &mp1_proc_list, list) {
        list_del(&pos->list);
        kfree(pos);
    }

    // free workqueue
    flush_workqueue(mp1_workqueue);
    destroy_workqueue(mp1_workqueue);

    // free work
    kfree(mp1_work);

    printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
