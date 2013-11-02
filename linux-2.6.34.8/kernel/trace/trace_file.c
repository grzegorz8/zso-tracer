#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/slab.h>

#include <trace/trace_file.h>

#include "trace.h"

#define MAX_VALUES_IN_LINE  16
/*
 * Buffer size:
 * +11: "READ_DATA" / "WRITE_DATA"
 * +11: pid + space
 * +(MAX_VALUES_IN_LINE * 3): data byte + space
 * +5: \0 + "safety buffer"
 */
#define BUFFER_SIZE     (10 + 11 + (MAX_VALUES_IN_LINE) * 3 + 5)

static struct trace_array      *ctx_trace;
static int __read_mostly       file_trace_enabled;

static inline int minimum(int a, int b) { return (a < b ? a : b); }

/**
 * Prints a line of data.
 * @buf - buffer containing data to print.
 * @count - number of bytes to print.
 * @is_read - indicates whether data was read or written.
 */
static void print_data_line(char *buf, int count, int is_read)
{
    char *name = is_read ? "READ_DATA" : "WRITE_DATA";
    /* Buffer for entire line. */
    char *buffer = (char *) kmalloc(BUFFER_SIZE * sizeof(char), GFP_KERNEL);
    int len = 0;
    int i;
    
    len += sprintf(buffer, "%d %s", current->pid, name);
    for (i = 0; i < count; ++i) {
        len += sprintf(buffer + len, " %02x", buf[i]);
    }
    trace_printk("%s\n", buffer);
    kfree(buffer);
}

/**
 * Prints all lines of data. May also print error message if an error occurs
 * when reading data from user space.
 * @buf - buffer containing data to print.
 * @arg - number of bytes to print.
 * @is_read - indicates whether data was read or written.
 */
static void print_data(const char __user *buf, int arg, int is_read)
{
    char *error_name = is_read ? "READ_DATA_FAULT" : "WRITE_DATA_FAULT";
    int offset = 0;
    char *buffer = (char *)
            kmalloc(MAX_VALUES_IN_LINE * sizeof(char), GFP_KERNEL);
    if (buffer == NULL) {
        trace_printk("%s\n", error_name);
        return;
    }
    
    while (offset < arg) {
        int count = minimum(arg - offset, MAX_VALUES_IN_LINE);
        if (copy_from_user((void *) buffer, buf + offset, count)) {
            trace_printk("%s\n", error_name);
            goto free_buffer;
        }
        print_data_line(buffer, count, is_read);
        offset += count;
    }
free_buffer:
    kfree(buffer);
}

static void probe_file_open(char *filename, int flags, int mode, int arg)
{
    if (!file_trace_enabled)
        return;

    if (arg >= 0) {
        trace_printk("%d OPEN %s %#x %#o SUCCESS %d\n",
            current->pid, filename, flags, mode, arg);
    } else {
        trace_printk("%d OPEN %s %#x %#o ERR %d\n",
            current->pid, filename, flags, mode, -arg);
    }
}

static void probe_file_close(int fd, int retval)
{
    if (!file_trace_enabled)
        return;

    if (retval == 0) {
        trace_printk("%d CLOSE %d SUCCESS\n",
            current->pid, fd);
    } else {
        trace_printk("%d CLOSE %d ERR %d\n",
            current->pid, fd, retval);
    }
}

static void probe_file_lseek(int fd, int offset, int mode, int arg)
{
    if (!file_trace_enabled)
        return;

    if (arg > 0) {
        trace_printk("%d LSEEK %d %d %d SUCCESS %d\n",
            current->pid, fd, offset, mode, arg);
    } else {
        trace_printk("%d LSEEK %d %d %d ERR %d\n",
            current->pid, fd, offset, mode, -arg);
    }
}

static void probe_file_read(int fd, int buf_size, int arg, char __user *buf)
{
    if (!file_trace_enabled)
        return;

    if (arg > 0) {
        trace_printk("%d READ %d %d SUCCESS %d\n",
            current->pid, fd, buf_size, arg);
        print_data(buf, arg, 1);
    } else if (arg == 0) {
        trace_printk("%d READ %d %d EOF\n",
            current->pid, fd, buf_size);
    }
    else {
        trace_printk("%d READ %d %d ERR %d\n",
            current->pid, fd, buf_size, -arg);
    }
}

static void probe_file_write(int fd, int buf_size, int arg,
    const char __user *buf)
{
    if (!file_trace_enabled)
        return;

    if (arg >= 0) {
        trace_printk("%d WRITE %d %d SUCCESS %d\n",
            current->pid, fd, buf_size, arg);
        if (arg > 0)
            print_data(buf, buf_size, 0);
    }
    else {
        trace_printk("%d WRITE %d %d ERR %d\n",
            current->pid, fd, buf_size, -arg);
        if (buf_size > 0)
            print_data(buf, buf_size, 0);
        else if (buf_size < 0)
            trace_printk("WRITE_DATA_FAULT\n");
            
    }
}

static void reset_file_trace(struct trace_array *tr)
{
    tr->time_start = ftrace_now(tr->cpu);
    tracing_reset_online_cpus(tr);
}

static int file_trace_register(void)
{
    int ret;

    ret = register_trace_file_open(probe_file_open);
    if (ret)
        goto fail_open;
       
    ret = register_trace_file_close(probe_file_close);
    if (ret)
        goto fail_close;
    
    ret = register_trace_file_lseek(probe_file_lseek);
    if (ret)
        goto fail_lseek;
    
    ret = register_trace_file_read(probe_file_read);
    if (ret)
        goto fail_read;
    
    ret = register_trace_file_write(probe_file_write);
    if (ret)
        goto fail_write;

    return ret;
    
fail_write:
    unregister_trace_file_read(probe_file_read);
fail_read:
    unregister_trace_file_lseek(probe_file_lseek);
fail_lseek:
    unregister_trace_file_close(probe_file_close);
fail_close:
    unregister_trace_file_open(probe_file_open);
fail_open:
    return ret;
}

static void file_trace_unregister(void)
{
    unregister_trace_file_open(probe_file_open);
    unregister_trace_file_close(probe_file_close);
    unregister_trace_file_lseek(probe_file_lseek);
    unregister_trace_file_read(probe_file_read);
    unregister_trace_file_write(probe_file_write);
}

static void file_trace_start(void)
{
    file_trace_register();
}

static void file_trace_stop(void)
{
    file_trace_unregister();
}

void file_trace_stop_cmdline_record(void)
{
    file_trace_stop();
}

static void file_start_trace(struct trace_array *tr)
{
    reset_file_trace(tr);
    file_trace_start();
    file_trace_enabled = 1;
}

static void file_stop_trace(struct trace_array *tr)
{
    file_trace_enabled = 0;
    file_trace_stop();
}

static int file_trace_init(struct trace_array *tr)
{
    ctx_trace = tr;

    file_start_trace(tr);
    return 0;
}

static void file_trace_reset(struct trace_array *tr)
{
    file_stop_trace(tr);
}

/**
 * struct tracer - a specific tracer and its callbacks to interact with debugfs
 * @name: the name chosen to select it on the available_tracers file
 * @init: called when one switches to this tracer (echo name > current_tracer)
 * @reset: called when one switches to another tracer
 * ... There is no need to handle other callbacks.
 */
static struct tracer file_trace __read_mostly =
{
    .name       = "file_trace",
    .init       = file_trace_init,
    .reset      = file_trace_reset,
};

__init static int init_file_trace(void)
{
    int ret = 0;
    ret = file_trace_register();
    if (ret) {
        return ret;
    }
    return register_tracer(&file_trace);
}

device_initcall(init_file_trace);
