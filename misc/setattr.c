#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/xattr.h>

#define XATTR_NAME      "user.file_trace"

int main() {
        ssize_t len;
        char *buf;
        char *filename = "/root/test";
        char *attr_val = "foo";

        len = setxattr(filename, XATTR_NAME, attr_val, sizeof(attr_val),
                XATTR_CREATE | XATTR_REPLACE);
        
        if (len == -1) {
                printf("setxattr failed (%d).\n", errno);
                return 1;
        }

        len = getxattr(filename, XATTR_NAME, NULL, 0);
        if (len == -1) {
                printf("Xattr not found (errno: %d).\n", errno);
                return 1;
        }
        buf = (char *) malloc(len);
        getxattr(filename, XATTR_NAME, buf, len);
        printf("Value: %s\n", buf);
        return 0;
}
