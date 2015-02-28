#define LINUX

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/kthread.h>

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
#define READY            2
#define RUNNING          3

struct mp2_task_struct {
    struct task_struct *linux_task;
    struct timer_list wakeup_timer;
    struct list_head list;

    // milliseconds
    unsigned int pid;
    unsigned int period;
    unsigned int computation;
    unsigned int state;
    // jiffies
    unsigned long deadline;
};

static struct proc_dir_entry *proc_dir, *proc_entry;
LIST_HEAD(mp2_task_struct_list);
DEFINE_MUTEX(mp2_list_mutex);
DEFINE_MUTEX(mp2_current_running_task_mutex);
static spinlock_t mp2_lock;
static struct task_struct *mp2_dispatch_thread;
static struct mp2_task_struct *current_running_task = NULL;
static struct kmem_cache *mp2_task_struct_cache;

struct mp2_task_struct *__get_task_by_pid(unsigned int pid)
{
    struct mp2_task_struct *tmp;
    list_for_each_entry(tmp, &mp2_task_struct_list, list) {
        if (tmp->pid == pid) {
            return tmp;
        }
    }
    // if no task with such pid, then return NULL
    return NULL;
}

void timer_callback(unsigned long pid)
{
    unsigned long flags;

    spin_lock_irqsave(&mp2_lock, flags);
    __get_task_by_pid(pid)->state = READY;
    spin_unlock_irqrestore(&mp2_lock, flags);

    // wake up dispatching thread
    wake_up_process(mp2_dispatch_thread);
}

struct mp2_task_struct *get_highest_priority_ready_task(void)
{
    struct mp2_task_struct *tmp, *result = NULL;

    mutex_lock_interruptible(&mp2_list_mutex);
    list_for_each_entry(tmp, &mp2_task_struct_list, list) {
        // make sure the task's state is READY
        if (tmp->state != READY) {
            continue;
        }
        if (result == NULL) {
            // find out first READY task in the list
            result = tmp;
        } else {
            if (result->period > tmp->period) {
                result = tmp;
            }
        }
    }
    mutex_unlock(&mp2_list_mutex);

    return result;
}

int admission_control(unsigned int computation, unsigned int period)
{
    unsigned int sum_ratio = 0;
    unsigned int util_bound = 6930;
    unsigned int multiplier = 10000;
    struct mp2_task_struct *tmp;

    // accumulate all tasks' ratio already in the list
    mutex_lock_interruptible(&mp2_list_mutex);
    list_for_each_entry(tmp, &mp2_task_struct_list, list) {
        sum_ratio += (tmp->computation * multiplier) / tmp->period;
    }
    mutex_unlock(&mp2_list_mutex);

    // add the ratio of the new task
    sum_ratio += (computation * multiplier) / period;

    if (sum_ratio <= util_bound) {
        return 1;
    } else {
        return 0;
    }
}

void __set_priority(struct mp2_task_struct *task, int policy, int priority)
{
    struct sched_param sparam;
    sparam.sched_priority = priority;
    sched_setscheduler(task->linux_task, policy, &sparam);
}

static int mp2_dispatch_thread_fn(void *data)
{
    struct mp2_task_struct *tmp;

    while (1) {
        // put dispatching thread to sleep
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();

        if (kthread_should_stop()) {
            return 0;
        }

        mutex_lock_interruptible(&mp2_current_running_task_mutex);
        if ((tmp = get_highest_priority_ready_task()) == NULL) {
            // no READY task
            if (current_running_task != NULL) {
                // lower the priority of the current_running_task
                __set_priority(current_running_task, SCHED_NORMAL, 0);
            }
        } else {
            if (current_running_task != NULL && tmp->period < current_running_task->period) {
                // preemption, set the current_running_task to READY
                current_running_task->state = READY;
                __set_priority(current_running_task, SCHED_NORMAL, 0);
            }
            // wake up tmp (highest priority READY task)
            tmp->state = RUNNING;
            wake_up_process(tmp->linux_task);
            __set_priority(tmp, SCHED_FIFO, 99);
            current_running_task = tmp;
        }
        mutex_unlock(&mp2_current_running_task_mutex);
    }
}

static ssize_t mp2_read(struct file *file, char __user *buffer, size_t count, loff_t *data)
{
    unsigned long copied = 0;
    char *buf;
    struct mp2_task_struct *tmp;

    buf = (char *)kmalloc(count, GFP_KERNEL);

    // loop through the tasks in the list
    mutex_lock_interruptible(&mp2_list_mutex);
    list_for_each_entry(tmp, &mp2_task_struct_list, list) {
        copied += sprintf(buf + copied, "%u %u %u %u\n", tmp->pid, tmp->period, tmp->computation, tmp->state);
    }
    mutex_unlock(&mp2_list_mutex);

    buf[copied] = '\0';

    copy_to_user(buffer, buf, copied);

    kfree(buf);

    return copied;
}

void mp2_register_processs(char *buf)
{
    struct mp2_task_struct *tmp;

    // allocate memory by cache for tmp
    tmp = (struct mp2_task_struct *)kmem_cache_alloc(mp2_task_struct_cache, GFP_KERNEL);

    // initialize list
    INIT_LIST_HEAD(&(tmp->list));
    // set pid
    sscanf(strsep(&buf, ","), "%u", &tmp->pid);
    // set period
    sscanf(strsep(&buf, ","), "%u", &tmp->period);
    // set computation
    sscanf(strsep(&buf, "\n"), "%u", &tmp->computation);
    // set deadline
    tmp->deadline = 0;
    // set linux_task
    tmp->linux_task = find_task_by_pid(tmp->pid);
    // set wakeup_timer
    setup_timer(&(tmp->wakeup_timer), timer_callback, tmp->pid);

    // check for admission_control
    if (!admission_control(tmp->computation, tmp->period)) {
        return;
    }

    // add task to mp2_task_struct_list
    mutex_lock_interruptible(&mp2_list_mutex);
    list_add(&(tmp->list), &mp2_task_struct_list);
    mutex_unlock(&mp2_list_mutex);
}

void mp2_yield_process(char *buf)
{
    unsigned int pid;
    struct mp2_task_struct *tmp;
    int should_skip;

    // set task to tmp
    sscanf(buf, "%u", &pid);
    mutex_lock_interruptible(&mp2_list_mutex);
    tmp = __get_task_by_pid(pid);
    mutex_unlock(&mp2_list_mutex);

    if (tmp->deadline == 0) {
        // initial deadline when the task first time yield
        should_skip = 0;
        tmp->deadline = jiffies + msecs_to_jiffies(tmp->period);
    } else {
        // update deadline
        tmp->deadline += msecs_to_jiffies(tmp->period);
        // check if the task should skip sleeping
        should_skip = jiffies > tmp->deadline ? 1 : 0;
    }

    if (should_skip) {
        return;
    }

    // update timer
    mod_timer(&(tmp->wakeup_timer), tmp->deadline);
    // set state to SLEEPING
    tmp->state = SLEEPING;
    // reset current_running_task to NULL
    mutex_lock_interruptible(&mp2_current_running_task_mutex);
    current_running_task = NULL;
    mutex_unlock(&mp2_current_running_task_mutex);
    // wake up scheduler
    wake_up_process(mp2_dispatch_thread);
    // sleep
    set_task_state(tmp->linux_task, TASK_UNINTERRUPTIBLE);
    schedule();
}

void mp2_unregister_process(char *buf)
{
    unsigned int pid;
    struct mp2_task_struct *tmp;

    sscanf(buf, "%u", &pid);

    // find and delete the task from mp2_task_struct_list
    mutex_lock_interruptible(&mp2_list_mutex);
    tmp = __get_task_by_pid(pid);
    list_del(&tmp->list);
    mutex_unlock(&mp2_list_mutex);

    // if the current_running_task is the deleting task, set current_running_task to NULL
    mutex_lock_interruptible(&mp2_current_running_task_mutex);
    if (current_running_task == tmp) {
        current_running_task = NULL;
        wake_up_process(mp2_dispatch_thread);
    }
    mutex_unlock(&mp2_current_running_task_mutex);

    // free cache
    kmem_cache_free(mp2_task_struct_cache, tmp);
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
            mp2_yield_process(buf + 3);
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

    // init spinlock
    spin_lock_init(&mp2_lock);

    // create cache
    mp2_task_struct_cache = KMEM_CACHE(mp2_task_struct, SLAB_PANIC);

    // init and run the dispatching thread
    mp2_dispatch_thread = kthread_run(mp2_dispatch_thread_fn, NULL, "mp2_dispatch_thread");

    printk(KERN_ALERT "MP2 MODULE LOADED\n");
    return 0;
}

// mp1_exit - Called when module is unloaded
static void __exit mp2_exit(void)
{
    struct mp2_task_struct *pos, *n;

    #ifdef DEBUG
    printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
    #endif

    // remove /proc/mp1/status
    remove_proc_entry(FILENAME, proc_dir);
    // remove /proc/mp1
    remove_proc_entry(DIRECTORY, NULL);

    // destroy all mutexes
    mutex_destroy(&mp2_list_mutex);
    mutex_destroy(&mp2_current_running_task_mutex);

    // stop the dispatching thread
    kthread_stop(mp2_dispatch_thread);

    // free mp1_proc_list
    list_for_each_entry_safe(pos, n, &mp2_task_struct_list, list) {
        list_del(&pos->list);
        kmem_cache_free(mp2_task_struct_cache, pos);
    }

    // destroy the cache
    kmem_cache_destroy(mp2_task_struct_cache);

    printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
