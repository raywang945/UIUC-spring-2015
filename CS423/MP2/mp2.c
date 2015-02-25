#define LINUX

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("16");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG            1
#define FILENAME         "status"
#define DIRECTORY        "mp2"
#define REGISTRATION     'R'
#define YIELD            'Y'
#define DEREGISTRATION   'D'
#define SLEEPING         1

typedef struct {
    struct task_struct *linux_task;
    /*struct timer_list wakeup_timer;*/
    struct list_head list;

    /*uint64_t next_period;*/
    unsigned int pid;
    unsigned int period;
    unsigned int computation;
    unsigned int state;
    /*unsigned long relative_period;*/
    /*unsigned long slice;*/
} proc_list;

static struct proc_dir_entry *proc_dir, *proc_entry;
LIST_HEAD(mp2_proc_list);
DEFINE_MUTEX(mp2_mutex);

static ssize_t mp2_read(struct file *file, char __user *buffer, size_t count, loff_t *data)
{
    unsigned long copied = 0;
    char *buf;
    proc_list *tmp;

    buf = (char *)kmalloc(count, GFP_KERNEL);

    // enter critical section
    mutex_lock_interruptible(&mp2_mutex);
    list_for_each_entry(tmp, &mp2_proc_list, list) {
        copied += sprintf(buf + copied, "%u %u %u %u\n", tmp->pid, tmp->period, tmp->computation, tmp->state);
    }
    mutex_unlock(&mp2_mutex);

    buf[copied] = '\0';

    copy_to_user(buffer, buf, copied);

    kfree(buf);

    return copied;
}

void mp2_register_processs(char *buf)
{
    proc_list *tmp;

    // initialize tmp->list
    tmp = (proc_list *)kmalloc(sizeof(proc_list), GFP_KERNEL);
    INIT_LIST_HEAD(&(tmp->list));

    // set tmp->pid
    sscanf(strsep(&buf, ","), "%u", &tmp->pid);
    // set tmp->period
    sscanf(strsep(&buf, ","), "%u", &tmp->period);
    // set tmp->computation
    sscanf(strsep(&buf, "\n"), "%u", &tmp->computation);
    // set tmp->linux_task
    tmp->linux_task = find_task_by_pid(tmp->pid);
    // set tmp->state
    tmp->state = SLEEPING;

    // add tmp to mp2_proc_list
    mutex_lock_interruptible(&mp2_mutex);
    list_add(&(tmp->list), &mp2_proc_list);
    mutex_unlock(&mp2_mutex);
}

void mp2_unregister_process(char *buf)
{
    unsigned int pid;
    proc_list *pos, *n;

    sscanf(buf, "%u", &pid);

    mutex_lock_interruptible(&mp2_mutex);
    list_for_each_entry_safe(pos, n, &mp2_proc_list, list) {
        if (pos->pid == pid) {
            list_del(&pos->list);
            kfree(pos);
        }
    }
    mutex_unlock(&mp2_mutex);
}

static ssize_t mp2_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
    char *buf;

    buf = (char *)kmalloc(count + 1, GFP_KERNEL);
    copy_from_user(buf, buffer, count);
    buf[count] = '\0';

    switch (buf[0]) {
        case REGISTRATION:
            mp2_register_processs(buf + 3);
            break;
        case YIELD:
            printk("YIELD\n");
            break;
        case DEREGISTRATION:
            mp2_unregister_process(buf + 3);
            break;
        default:
            printk(KERN_INFO "error in my_mapping(opt)\n");
    }

    kfree(buf);

    return count;
}

static const struct file_operations mp2_fops = {
    .owner   = THIS_MODULE,
    .read    = mp2_read,
    .write   = mp2_write
};

// mp2_init - Called when module is loaded
static int __init mp2_init(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP2 MODULE LOADING\n");
    #endif
    // Insert your code here ...

    // create /proc/mp2
    if ((proc_dir = proc_mkdir(DIRECTORY, NULL)) == NULL) {
        printk(KERN_INFO "proc_mkdir ERROR\n");
        return -ENOMEM;
    }
    // create /proc/mp2/status
    if ((proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp2_fops)) == NULL) {
        remove_proc_entry(DIRECTORY, NULL);
        printk(KERN_INFO "proc_create ERROR\n");
        return -ENOMEM;
    }

    printk(KERN_ALERT "MP2 MODULE LOADED\n");
    return 0;
}

// mp1_exit - Called when module is unloaded
static void __exit mp2_exit(void)
{
    proc_list *pos, *n;

    #ifdef DEBUG
    printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
    #endif
    // Insert your code here ...

    // remove /proc/mp1/status
    remove_proc_entry(FILENAME, proc_dir);
    // remove /proc/mp1
    remove_proc_entry(DIRECTORY, NULL);

    mutex_destroy(&mp2_mutex);

    // free mp1_proc_list
    list_for_each_entry_safe(pos, n, &mp2_proc_list, list) {
        list_del(&pos->list);
        kfree(pos);
    }

    printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
