#define LINUX

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/mm.h>

#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("16");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG                 1
#define FILENAME              "status"
#define DIRECTORY             "mp3"
#define REGISTRATION          'R'
#define UNREGISTRATION        'U'
#define DELAY                 50
#define NPAGES                128
#define PROFILER_BUFFER_LEN   (NPAGES * PAGE_SIZE / sizeof(unsigned long))
#define CDEV_NAME             "mp3_character_device"

struct mp3_task_struct {
    struct task_struct *linux_task;
    struct list_head list;
    unsigned int pid;
};

static void mp3_work_function(struct work_struct *work);
static struct proc_dir_entry *proc_dir, *proc_entry;
LIST_HEAD(mp3_task_struct_list);
static spinlock_t mp3_lock;
static struct workqueue_struct *mp3_workqueue;
DECLARE_DELAYED_WORK(mp3_delayed_work, mp3_work_function);
static dev_t mp3_dev;
static struct cdev mp3_cdev;
unsigned long delay;
unsigned long *profiler_buffer;
int profiler_ptr = 0;

struct mp3_task_struct *__get_task_by_pid(unsigned int pid)
{
    struct mp3_task_struct *tmp;
    list_for_each_entry(tmp, &mp3_task_struct_list, list) {
        if (tmp->pid == pid) {
            return tmp;
        }
    }
    // if no task with such pid, then return NULL
    return NULL;
}

static void mp3_work_function(struct work_struct *work)
{
    unsigned long flags;
    unsigned long stime, min_flt, maj_flt, utime;
    struct mp3_task_struct *tmp;
    unsigned long total_min_flt = 0, total_maj_flt = 0, total_cpu_time = 0;
    int tag = 0;

    // calculate min_flt, maj_flt, and cpu time
    spin_lock_irqsave(&mp3_lock, flags);
    list_for_each_entry(tmp, &mp3_task_struct_list, list) {
        if (get_cpu_use(tmp->pid, &min_flt, &maj_flt, &utime, &stime) == -1) {
            continue;
        }
        total_min_flt += min_flt;
        total_maj_flt += maj_flt;
        total_cpu_time += utime + stime;
        tag ++;
    }
    spin_unlock_irqrestore(&mp3_lock, flags);

    // write to profiler buffer
    profiler_buffer[profiler_ptr ++] = jiffies;
    profiler_buffer[profiler_ptr ++] = total_min_flt;
    profiler_buffer[profiler_ptr ++] = total_maj_flt;
    profiler_buffer[profiler_ptr ++] = jiffies_to_msecs(cputime_to_jiffies(total_cpu_time));

    if (profiler_ptr >= PROFILER_BUFFER_LEN) {
        profiler_ptr = 0;
    }

    queue_delayed_work(mp3_workqueue, &mp3_delayed_work, delay);
}

static ssize_t mp3_read(struct file *file, char __user *buffer, size_t count, loff_t *data)
{
    ssize_t copied = 0;
    static int finished = 0;
    unsigned long flags;
    struct mp3_task_struct *tmp;
    char *buf = (char*)kmalloc(count, GFP_KERNEL);

    if (finished) {
        finished = 0;
        return 0;
    } else {
        finished = 1;
    }

    // loop through the tasks in the list
    spin_lock_irqsave(&mp3_lock, flags);
    list_for_each_entry(tmp, &mp3_task_struct_list, list) {
        copied += sprintf(buf + copied, "%u\n", tmp->pid);
    }
    spin_unlock_irqrestore(&mp3_lock, flags);

    copy_to_user(buffer, buf, copied);
    kfree(buf);

    return copied;
}

void mp3_register_processs(char *buf)
{
    unsigned long flags;

    // allocate memory by cache for tmp
    struct mp3_task_struct *tmp = (struct mp3_task_struct*)kmalloc(sizeof(struct mp3_task_struct), GFP_KERNEL);

    // initialize list
    INIT_LIST_HEAD(&(tmp->list));
    // set pid
    sscanf(strsep(&buf, ","), "%u", &tmp->pid);
    // set linux_task
    tmp->linux_task = find_task_by_pid(tmp->pid);

    spin_lock_irqsave(&mp3_lock, flags);
    // queue the work if this process is the first process
    if (list_empty(&mp3_task_struct_list)) {
        queue_delayed_work(mp3_workqueue, &mp3_delayed_work, delay);
    }

    // add task to mp3_task_struct_list
    list_add(&(tmp->list), &mp3_task_struct_list);
    spin_unlock_irqrestore(&mp3_lock, flags);
}

int mp3_cdev_mmap(struct file *fp, struct vm_area_struct *vma)
{
    int ret;
    unsigned long length = vma->vm_end - vma->vm_start;
    unsigned long start = vma->vm_start;
    struct page *page;
    char *profiler_buffer_ptr = (char*)profiler_buffer;

    if (length > NPAGES * PAGE_SIZE) {
        return -EIO;
    }
    // map physical memory to virtual memory
    while (length > 0) {
        page = vmalloc_to_page(profiler_buffer_ptr);
        if ((ret = vm_insert_page(vma, start, page)) < 0) {
            return ret;
        }
        start += PAGE_SIZE;
        profiler_buffer_ptr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }
    return 0;
}

void mp3_unregister_process(char *buf)
{
    unsigned int pid;
    struct mp3_task_struct *tmp;
    unsigned long flags;

    sscanf(buf, "%u", &pid);

    // find and delete the task from mp2_task_struct_list
    spin_lock_irqsave(&mp3_lock, flags);
    tmp = __get_task_by_pid(pid);
    list_del(&tmp->list);
    spin_unlock_irqrestore(&mp3_lock, flags);

    if (list_empty(&mp3_task_struct_list)) {
        cancel_delayed_work(&mp3_delayed_work);
        flush_workqueue(mp3_workqueue);
    }

    // free cache
    kfree(tmp);
}

static ssize_t mp3_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
    char *buf = (char *)kmalloc(count + 1, GFP_KERNEL);
    copy_from_user(buf, buffer, count);
    buf[count] = '\0';

    switch (buf[0]) {
        case REGISTRATION:
            mp3_register_processs(buf + 2);
            break;
        case UNREGISTRATION:
            mp3_unregister_process(buf + 2);
            break;
        default:
            printk(KERN_INFO "error in mp3_write\n");
    }

    kfree(buf);
    return count;
}

static const struct file_operations mp3_fops = {
    .owner = THIS_MODULE,
    .read  = mp3_read,
    .write = mp3_write,
};

static const struct file_operations mp3_cdev_fops = {
    .owner   = THIS_MODULE,
    .open    = NULL,
    .release = NULL,
    .mmap    = mp3_cdev_mmap
};

// mp3_init - Called when module is loaded
static int __init mp3_init(void)
{
    int ret = 0;
    #ifdef DEBUG
    printk(KERN_ALERT "MP3 MODULE LOADING\n");
    #endif

    // create /proc/mp3
    if ((proc_dir = proc_mkdir(DIRECTORY, NULL)) == NULL) {
        ret = -ENOMEM;
        goto out;
    }
    // create /proc/mp3/status
    if ((proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp3_fops)) == NULL) {
        ret = -ENOMEM;
        goto out_proc_dir;
    }

    // set delay
    delay = msecs_to_jiffies(DELAY);

    // init spinlock
    spin_lock_init(&mp3_lock);

    // initialize workqueue
    if ((mp3_workqueue = create_singlethread_workqueue("mp3_workqueue")) == NULL) {
        ret = -ENOMEM;
        goto out_proc_entry;
    }

    // allocate character device number range
    if ((ret = alloc_chrdev_region(&mp3_dev, 0, 1, CDEV_NAME)) < 0) {
        goto out_workqueue;
    }
    // init character device
    cdev_init(&mp3_cdev, &mp3_cdev_fops);
    if ((ret = cdev_add(&mp3_cdev, mp3_dev, 1))) {
        goto out_chrdev_region;
    }

    if ((profiler_buffer = (unsigned long*)vmalloc(NPAGES * PAGE_SIZE)) == NULL) {
        ret = -ENOMEM;
        goto out_cdev;
    }
    profiler_buffer = (unsigned long*)vmalloc(NPAGES * PAGE_SIZE);

    return ret;

out_cdev:
    cdev_del(&mp3_cdev);
out_chrdev_region:
    unregister_chrdev_region(mp3_dev, 1);
out_workqueue:
    destroy_workqueue(mp3_workqueue);
out_proc_entry:
    remove_proc_entry(FILENAME, proc_dir);
out_proc_dir:
    remove_proc_entry(DIRECTORY, NULL);
out:
    return ret;
}

// mp1_exit - Called when module is unloaded
static void __exit mp3_exit(void)
{
    struct mp3_task_struct *pos, *n;

    #ifdef DEBUG
    printk(KERN_ALERT "MP3 MODULE UNLOADING\n");
    #endif

    // remove /proc/mp3/status
    remove_proc_entry(FILENAME, proc_dir);
    // remove /proc/mp3
    remove_proc_entry(DIRECTORY, NULL);

    // free mp3_task_struct_list
    list_for_each_entry_safe(pos, n, &mp3_task_struct_list, list) {
        list_del(&pos->list);
        kfree(pos);
    }

    // destroy workqueue
    cancel_delayed_work(&mp3_delayed_work);
    flush_workqueue(mp3_workqueue);
    destroy_workqueue(mp3_workqueue);

    // destroy character device
    cdev_del(&mp3_cdev);
    unregister_chrdev_region(mp3_dev, 1);

    vfree(profiler_buffer);

    printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
