#include <sys/io.h>

#include "internal/syscalls/syscalls.h"

fd_t open(const char* path, uint8_t flags)
{    
    fd_t result = SYSCALL(SYS_OPEN, 2, path, flags);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}