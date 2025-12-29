// POSIX
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/unistd.h.html

#ifndef _UNISTD_H
#define _UNISTD_H 1

#include "stddef.h"

#define _POSIX_VERSION  202405L
// #define _POSIX2_VERSION 202405L
#define _POSIX2_VERSION -1 // Shell and Utilities option not supported

#ifdef __IMPLEMENT_EXT_XSI
#define _XOPEN_VERSION 800
#endif /* __IMPLEMENT_EXT_XSI */

/**
 * Constants for Options and Option Groups. Map between optional options and their shorthand in POSIX spec:
 *   ADV = _POSIX_ADVISORY_INFO
 *   CPT = _POSIX_CPUTIME
 *   DC  = _POSIX_DEVICE_CONTROL
 *   FSC = _POSIX_FSYNC
 *   IP6 = _POSIX_IPV6
 *   ML  = _POSIX_MEMLOCK
 *   MLR = _POSIX_MEMLOCK_RANGE
 *   MSG = _POSIX_MESSAGE_PASSING
 *   PIO = _POSIX_PRIORITIZED_IO
 *   PS  = _POSIX_PRIORITY_SCHEDULING
 *   RS  = _POSIX_RAW_SOCKETS
 *   SHM = _POSIX_SHARED_MEMORY_OBJECTS
 *   SPN = _POSIX_SPAWN
 *   SS  = _POSIX_SPORADIC_SERVER
 *   SIO = _POSIX_SYNCHRONIZED_IO
 *   TSA = _POSIX_THREAD_ATTR_STACKADDR
 *   TSS = _POSIX_THREAD_ATTR_STACKSIZE
 *   TCT = _POSIX_THREAD_CPUTIME
 *   TPI = _POSIX_THREAD_PRIO_INHERIT
 *   TPP = _POSIX_THREAD_PRIO_PROTECT
 *   TPS = _POSIX_THREAD_PRIORITY_SCHEDULING
 *   TSH = _POSIX_THREAD_PROCESS_SHARED
 *   RPI = _POSIX_THREAD_ROBUST_PRIO_INHERIT
 *   RPP = _POSIX_THREAD_ROBUST_PRIO_PROTECT
 *   TSP = _POSIX_THREAD_SPORADIC_SERVER
 *   TYM = _POSIX_TYPED_MEMORY_OBJECTS
 *   OB  = obsolete (something about _POSIX_V7_LP64_OFF64, etc. (but not V8))
 *   CD  = _POSIX2_C_DEV
 *   FR  = _POSIX2_FORT_RUN
 *   SD  = _POSIX2_SW_DEV
 *   UP  = _POSIX2_UPE
 *   XSI = X/Open
 *   UU  = _XOPEN_UUCP
 *
 * TODO: figure out which ones we want to support
 */
#define _POSIX_ADVISORY_INFO              -1      // Advisory information not supported
#define _POSIX_ASYNCHRONOUS_IO            202405L // Mandatory
#define _POSIX_BARRIERS                   202405L // Mandatory
#define _POSIX_CHOWN_RESTRICTED           1       // Mandatory
#define _POSIX_CLOCK_SELECTION            202405L // Mandatory
#define _POSIX_CPUTIME                    -1      // CPU-time clocks not supported
#define _POSIX_DEVICE_CONTROL             -1      // Device control not supported
#define _POSIX_FSYNC                      -1      // File synchronization option not supported
#define _POSIX_IPV6                       -1      // IPv6 option not supported
#define _POSIX_JOB_CONTROL                202405L // Mandatory value > 0
#define _POSIX_MAPPED_FILES               202405L // Mandatory
#define _POSIX_MEMLOCK                    -1      // Process memory locking not supported
#define _POSIX_MEMLOCK_RANGE              -1      // Range memory locking not supported
#define _POSIX_MEMORY_PROTECTION          202405L // Mandatory
#define _POSIX_MESSAGE_PASSING            -1      // Message passing not supported
#define _POSIX_MONOTONIC_CLOCK            202405L // Mandatory
#define _POSIX_NO_TRUNC                   0       // TODO: IMPORTANT: decide on this
#define _POSIX_PRIORITIZED_IO             -1      // Prioritized I/O not supported
#define _POSIX_PRIORITY_SCHEDULING        -1      // Priority scheduling not supported
#define _POSIX_RAW_SOCKETS                -1      // Raw sockets not supported
#define _POSIX_READER_WRITER_LOCKS        202405L // Mandatory
#define _POSIX_REALTIME_SIGNALS           202405L // Mandatory
#define _POSIX_REGEXP                     202405L // Mandatory > 0
#define _POSIX_SAVED_IDS                  202405L // Mandatory > 0
#define _POSIX_SEMAPHORES                 202405L // Mandatory
#define _POSIX_SHARED_MEMORY_OBJECTS      -1      // Shared memory objects not supported
#define _POSIX_SHELL                      202405L // Mandatory
#define _POSIX_SPAWN                      -1      // Spawn option not supported
#define _POSIX_SPIN_LOCKS                 202405L // Mandatory
#define _POSIX_SPORADIC_SERVER            -1      // Sporadic server not supported
#define _POSIX_SYNCHRONIZED_IO            -1      // Synchronized I/O not supported
#define _POSIX_THREAD_ATTR_STACKADDR      -1      // Thread stack address attribute not supported
#define _POSIX_THREAD_ATTR_STACKSIZE      -1      // Thread stack size attribute not supported
#define _POSIX_THREAD_CPUTIME             -1      // Thread CPU-time clocks not supported
#define _POSIX_THREAD_PRIO_INHERIT        -1      // Non-robust mutex priority inheritance not supported
#define _POSIX_THREAD_PRIO_PROTECT        -1      // Non-robust mutex priority protection not supported
#define _POSIX_THREAD_PRIORITY_SCHEDULING -1      // Threaded execution scheduling not supported
#define _POSIX_THREAD_PROCESS_SHARED      -1      // Thread process-shared synchronization not supported
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT -1      // Robust mutex priority inheritance not supported
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT -1      // Robust mutex priority protection not supported
#define _POSIX_THREAD_SAFE_FUNCTIONS      202405L // Mandatory
#define _POSIX_THREAD_SPORADIC_SERVER     -1      // Threaded sporadic server not supported
#define _POSIX_THREADS                    202405L // Mandatory
#define _POSIX_TIMEOUTS                   202405L // Mandatory
#define _POSIX_TIMERS                     202405L // Mandatory
#define _POSIX_TYPED_MEMORY_OBJECTS       -1      // Typed memory objects not supported
#define _POSIX_V8_LP64_OFF64              1       // 32-bit int; 64-bit long, off_t, and pointers
#define _POSIX2_C_BIND                    202405L // Mandatory
#define _POSIX2_C_DEV                     -1      // C development utilities not supported
#define _POSIX2_CHAR_TERM                 -1      // Character terminal not supported
#define _POSIX2_FORT_RUN                  -1      // Fortran runtime utilities not supported
#define _POSIX2_LOCALEDEF                 -1      // Localedef utility not supported
#define _POSIX2_SW_DEV                    -1      // Software development utilities not supported
#define _POSIX2_UPE                       -1      // User Portability Utilities not supported
#define _XOPEN_CRYPT                      -1      // X/Open encryption not supported
#define _XOPEN_ENH_I18N                   0 // TODO: figure this out (docs say it must always be defined and can't be -1)
#define _XOPEN_REALTIME                   -1 // X/Open Realtime option not supported
#define _XOPEN_REALTIME_THREADS           -1 // X/Open Realtime Threads option not supported
#define _XOPEN_SHM                        0 // TODO: figure this out (docs say it must always be defined and can't be -1)
#define _XOPEN_UNIX                       -1 // XSI option not supported
#define _XOPEN_UUCP                       -1 // X/Open UUCP Utilities option not supported

/**
 * Execution-time symbolic constants. See docs for details.
 */
#define _POSIX_ASYNC_IO             -1
#define _POSIX_FALLOC               -1
#define _POSIX_PRIO_IO              -1
#define _POSIX_SYNC_IO              -1
#define _POSIX_TIMESTAMP_RESOLUTION 1 // Nanosecond resolution supported
#define _POSIX2_SYMLINKS            1 // Symbolic links supported

/**
 * Constants for functions
 */

#include "fcntl.h" // For O_CLOEXEC and O_CLOFORK

/**
 * Constants for access()
 */
#define F_OK 0 // Test for existence of file
#define R_OK 4 // Test for read permission
#define W_OK 2 // Test for write permission
#define X_OK 1 // Test for execute (search) permission

/**
 * Constants for confstr()
 *
 * These values are for the first argument to confstr().
 */

/* Default utility search path */
#define _CS_PATH 1

/* Default POSIX Issue 8 execution environment */
#define _CS_V8_ENV 2

/* POSIX Issue 8 width-restricted environment: LP64/OFF64 */
#define _CS_POSIX_V8_LP64_OFF64_CFLAGS  3
#define _CS_POSIX_V8_LP64_OFF64_LDFLAGS 4
#define _CS_POSIX_V8_LP64_OFF64_LIBS    5

/* POSIX Issue 8 threads environment */
#define _CS_POSIX_V8_THREADS_CFLAGS  6
#define _CS_POSIX_V8_THREADS_LDFLAGS 7

/* Supported width-restricted environments */
#define _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS 8

#include "stdio.h" // For SEEK_SET, SEEK_CUR, SEEK_END

#define SEEK_HOLE 3 // Set position to file hole (if supported)
#define SEEK_DATA 4 // Set position to file data (if supported)

#ifdef __IMPLEMENT_EXT_XSI
/**
 * Constants for lockf()
 */
#define F_LOCK  1 // Lock a section of a file
#define F_TLOCK 2 // Test and lock a section of a file
#define F_ULOCK 3 // Unlock a section of a file
#define F_TEST  4 // Test a section of a file for locks

#endif /* __IMPLEMENT_EXT_XSI */

/**
 * Constants for pathconf()
 */
#include "bits/pathconf.h"

/**
 * Constants for sysconf()
 */
#include "bits/sysconf.h"

/**
 * Symbolic constants for file streams
 */
#define STDIN_FILENO  0 // Standard input file descriptor
#define STDOUT_FILENO 1 // Standard output file descriptor
#define STDERR_FILENO 2 // Standard error file descriptor

/**
 * Symbolic constants for terminal special character handling
 */
#define _POSIX_VDISABLE 0 // Disable special character handling

/**
 * Constants for posix_close()
 */
#define POSIX_CLOSE_RESTART 0x1 // Restartable close

/**
 * Type definitions
 */
#include "stdint.h"    // intptr_t
#include "sys/types.h" // size_t, ssize_t, uid_t, gid_t, off_t, and pid_t

// TODO: change representation of file descriptors to int throughout the OS?

#ifdef __IMPLEMENT_EXT_FSC
int fsync(int fd);
#endif /* __IMPLEMENT_EXT_FSC */

#ifdef __IMPLEMENT_EXT_SIO
int fdatasync(int fd);
#endif /* __IMPLEMENT_EXT_SIO */

#ifdef __IMPLEMENT_EXT_XSI

#ifdef __IMPLEMENT_EXT_OB
void encrypt(char block[64], int edflag);
#endif /* __IMPLEMENT_EXT_OB */

char *crypt(const char *key, const char *salt);
long gethostid(void);
int getresgid(gid_t *restrict rgid, gid_t *restrict egid, gid_t *restrict sgid);
int getresuid(uid_t *restrict ruid, uid_t *restrict euid, uid_t *restrict suid);
int lockf(int fd, int cmd, off_t len);
int nice(int inc);
int setregid(gid_t rgid, gid_t egid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setreuid(uid_t ruid, uid_t euid);
void swab(const void *restrict src, void *restrict dest, ssize_t nbytes);
void sync(void);
#endif /* __IMPLEMENT_EXT_XSI */

int access(const char *pathname, int mode);
unsigned alarm(unsigned seconds);
int chdir(const char *path);
int chown(const char *pathname, uid_t owner, gid_t group);
int close(int fd);
size_t confstr(int name, char *buf, size_t len);

int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
_Noreturn void _exit(int status);

int execl(const char *path, const char *arg, ...);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
int fchdir(int fd);
int fchown(int fd, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);

int fexecve(int fd, char *const argv[], char *const envp[]);
pid_t _Fork(void);
pid_t fork(void);
long fpathconf(int fd, int name);

int ftruncate(int fd, off_t length);
char *getcwd(char *buf, size_t size);
gid_t getegid(void);
int getentropy(void *buf, size_t buflen);
uid_t geteuid(void);
gid_t getgid(void);
int getgroups(int size, gid_t list[]);

int gethostname(char *name, size_t len);
char *getlogin(void);
int getlogin_r(char *name, size_t len);
int getopt(int argc, char *const argv[], const char *optstring);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);

pid_t getsid(pid_t pid);
uid_t getuid(void);
int isatty(int fd);
int lchown(const char *pathname, uid_t owner, gid_t group);
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);

off_t lseek(int fd, off_t offset, int whence);

long pathconf(const char *pathname, int name);
int pause(void);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
int posix_close(int fd, int flags);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
ssize_t read(int fd, void *buf, size_t count);
ssize_t readlink(const char *restrict path, char *restrict buf, size_t buflen);
ssize_t readlinkat(int dirfd, const char *restrict path, char *restrict buf, size_t buflen);
int rmdir(const char *pathname);
int setegid(gid_t gid);
int seteuid(uid_t uid);
int setgid(gid_t gid);
int setpgid(pid_t pid, pid_t pgid);

pid_t setsid(void);
int setuid(uid_t uid);
unsigned sleep(unsigned seconds);

int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);

long sysconf(int name);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int truncate(const char *path, off_t length);
char *ttyname(int fd);
int ttyname_r(int fd, char *buf, size_t buflen);
int unlink(const char *pathname);
int unlinkat(int dirfd, const char *pathname, int flags);
ssize_t write(int fd, const void *buf, size_t count);

/**
 * External variables
 */
extern char *optarg;
extern int opterr, optind, optopt;

#endif /* _UNISTD_H */