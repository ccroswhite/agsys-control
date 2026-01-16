/**
 * @file syscalls.c
 * @brief Newlib syscall stubs for bare-metal/FreeRTOS environment
 * 
 * These stubs satisfy newlib-nano's requirements for POSIX-style I/O.
 * Required for clean builds without linker warnings.
 * 
 * Note: AgSys OTA uses agsys_flash_read/write directly, not file descriptors.
 * These stubs exist only to eliminate linker warnings.
 */

#include <sys/stat.h>
#include <errno.h>

/* Prevent multiple definition if SDK provides these */
#ifndef __SYSCALLS_IMPL__
#define __SYSCALLS_IMPL__

/**
 * @brief Close a file descriptor
 * @param fd File descriptor (unused in bare-metal)
 * @return -1 with errno set to EBADF
 */
int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

/**
 * @brief Seek within a file
 * @param fd File descriptor (unused)
 * @param offset Seek offset (unused)
 * @param whence Seek origin (unused)
 * @return -1 with errno set to EBADF
 */
int _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = EBADF;
    return -1;
}

/**
 * @brief Read from a file descriptor
 * @param fd File descriptor (unused)
 * @param buf Buffer to read into (unused)
 * @param count Bytes to read (unused)
 * @return -1 with errno set to EBADF
 */
int _read(int fd, char *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    errno = EBADF;
    return -1;
}

/**
 * @brief Write to a file descriptor
 * @param fd File descriptor (unused)
 * @param buf Buffer to write (unused)
 * @param count Bytes to write (unused)
 * @return -1 with errno set to EBADF
 * 
 * Note: Could be extended to redirect stdout/stderr to SEGGER RTT
 */
int _write(int fd, const char *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    errno = EBADF;
    return -1;
}

/**
 * @brief Get file status
 * @param fd File descriptor (unused)
 * @param st Stat buffer (unused)
 * @return -1 with errno set to EBADF
 */
int _fstat(int fd, struct stat *st)
{
    (void)fd;
    (void)st;
    errno = EBADF;
    return -1;
}

/**
 * @brief Check if fd is a terminal
 * @param fd File descriptor (unused)
 * @return 0 (not a terminal)
 */
int _isatty(int fd)
{
    (void)fd;
    return 0;
}

/**
 * @brief Get process ID
 * @return 1 (single process in bare-metal)
 */
int _getpid(void)
{
    return 1;
}

/**
 * @brief Send signal to process
 * @param pid Process ID (unused)
 * @param sig Signal number (unused)
 * @return -1 with errno set to EINVAL
 */
int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

#endif /* __SYSCALLS_IMPL__ */
