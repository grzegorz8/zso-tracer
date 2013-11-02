#ifndef _TRACE_TRACE_FILE_H
#define _TRACE_TRACE_FILE_H

#include <linux/fs.h>
#include <linux/tracepoint.h>

DECLARE_TRACE(file_open,
        TP_PROTO(char *filename, int flags, int mode, int arg),
        TP_ARGS(filename, flags, mode, arg));
               
DECLARE_TRACE(file_close,
        TP_PROTO(int fd, int retval),
        TP_ARGS(fd, retval));
        
DECLARE_TRACE(file_lseek,
        TP_PROTO(int fd, int offset, int mode, int arg),
        TP_ARGS(fd, offset, mode, arg));
        
DECLARE_TRACE(file_read,
        TP_PROTO(int fd, int buf_size, int arg, char __user *buf),
        TP_ARGS(fd, buf_size, arg, buf));
        
DECLARE_TRACE(file_write,
        TP_PROTO(int fd, int buf_size, int arg, const char __user *buf),
        TP_ARGS(fd, buf_size, arg, buf));
    
#endif
