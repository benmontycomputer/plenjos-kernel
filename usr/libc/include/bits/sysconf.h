/**
 * Constants for sysconf()
 */
#define _SC_2_C_BIND                          1
#define _SC_2_C_DEV                           2
#define _SC_2_CHAR_TERM                       3
#define _SC_2_FORT_RUN                        4
#define _SC_2_LOCALEDEF                      5
#define _SC_2_SW_DEV                         6
#define _SC_2_UPE                            7
#define _SC_2_VERSION                        8

#define _SC_ADVISORY_INFO                    9
#define _SC_AIO_LISTIO_MAX                  10
#define _SC_AIO_MAX                         11
#define _SC_AIO_PRIO_DELTA_MAX              12
#define _SC_ARG_MAX                         13
#define _SC_ASYNCHRONOUS_IO                 14
#define _SC_ATEXIT_MAX                      15
#define _SC_BARRIERS                        16

#define _SC_BC_BASE_MAX                     17
#define _SC_BC_DIM_MAX                      18
#define _SC_BC_SCALE_MAX                    19
#define _SC_BC_STRING_MAX                   20

#define _SC_CHILD_MAX                       21
#define _SC_CLK_TCK                         22
#define _SC_CLOCK_SELECTION                 23
#define _SC_COLL_WEIGHTS_MAX                24
#define _SC_CPUTIME                         25

#define _SC_DELAYTIMER_MAX                  26
#define _SC_DEVICE_CONTROL                  27

#define _SC_EXPR_NEST_MAX                   28

#define _SC_FSYNC                           29

#define _SC_GETGR_R_SIZE_MAX                30
#define _SC_GETPW_R_SIZE_MAX                31

#define _SC_HOST_NAME_MAX                   32

#define _SC_IOV_MAX                         33
#define _SC_IPV6                            34
#define _SC_JOB_CONTROL                    35

#define _SC_LINE_MAX                        36
#define _SC_LOGIN_NAME_MAX                  37

#define _SC_MAPPED_FILES                    38
#define _SC_MEMLOCK                         39
#define _SC_MEMLOCK_RANGE                   40
#define _SC_MEMORY_PROTECTION               41
#define _SC_MESSAGE_PASSING                 42
#define _SC_MONOTONIC_CLOCK                 43
#define _SC_MQ_OPEN_MAX                     44
#define _SC_MQ_PRIO_MAX                     45

#define _SC_NGROUPS_MAX                     46
#define _SC_NPROCESSORS_CONF                47
#define _SC_NPROCESSORS_ONLN                48
#define _SC_NSIG                            49

#define _SC_OPEN_MAX                        50
#define _SC_PAGE_SIZE                       51
#define _SC_PAGESIZE                        _SC_PAGE_SIZE

#define _SC_PRIORITIZED_IO                  52
#define _SC_PRIORITY_SCHEDULING             53

#define _SC_RAW_SOCKETS                     54
#define _SC_RE_DUP_MAX                      55
#define _SC_READER_WRITER_LOCKS             56
#define _SC_REALTIME_SIGNALS                57
#define _SC_REGEXP                          58
#define _SC_RTSIG_MAX                       59

#define _SC_SAVED_IDS                       60
#define _SC_SEM_NSEMS_MAX                   61
#define _SC_SEM_VALUE_MAX                   62
#define _SC_SEMAPHORES                     63
#define _SC_SHARED_MEMORY_OBJECTS           64
#define _SC_SHELL                          65
#define _SC_SIGQUEUE_MAX                    66
#define _SC_SPAWN                          67
#define _SC_SPIN_LOCKS                     68
#define _SC_SPORADIC_SERVER                69
#define _SC_SS_REPL_MAX                    70
#define _SC_STREAM_MAX                     71
#define _SC_SYMLOOP_MAX                    72
#define _SC_SYNCHRONIZED_IO                73

#define _SC_THREAD_ATTR_STACKADDR           74
#define _SC_THREAD_ATTR_STACKSIZE           75
#define _SC_THREAD_CPUTIME                  76
#define _SC_THREAD_DESTRUCTOR_ITERATIONS    77
#define _SC_THREAD_KEYS_MAX                 78
#define _SC_THREAD_PRIO_INHERIT             79
#define _SC_THREAD_PRIO_PROTECT             80
#define _SC_THREAD_PRIORITY_SCHEDULING      81
#define _SC_THREAD_PROCESS_SHARED           82
#define _SC_THREAD_ROBUST_PRIO_INHERIT       83
#define _SC_THREAD_ROBUST_PRIO_PROTECT       84
#define _SC_THREAD_SAFE_FUNCTIONS           85
#define _SC_THREAD_SPORADIC_SERVER           86
#define _SC_THREAD_STACK_MIN                87
#define _SC_THREAD_THREADS_MAX              88
#define _SC_THREADS                        89

#define _SC_TIMEOUTS                       90
#define _SC_TIMER_MAX                      91
#define _SC_TIMERS                         92
#define _SC_TTY_NAME_MAX                   93
#define _SC_TYPED_MEMORY_OBJECTS            94
#define _SC_TZNAME_MAX                     95

/* POSIX Issue 8 width-restricted environments */
#define _SC_V8_ILP32_OFF32                  96
#define _SC_V8_ILP32_OFFBIG                 97
#define _SC_V8_LP64_OFF64                  98
#define _SC_V8_LPBIG_OFFBIG                99

// [OB]
/* POSIX Issue 7 (OB – optional, for compatibility) */
#define _SC_V7_ILP32_OFF32                 100
#define _SC_V7_ILP32_OFFBIG                101
#define _SC_V7_LP64_OFF64                  102
#define _SC_V7_LPBIG_OFFBIG                103
// End [OB]

#define _SC_VERSION                       104

#define _SC_XOPEN_CRYPT                   105
#define _SC_XOPEN_ENH_I18N                106
#define _SC_XOPEN_REALTIME                107
#define _SC_XOPEN_REALTIME_THREADS        108
#define _SC_XOPEN_SHM                     109
#define _SC_XOPEN_UNIX                    110
#define _SC_XOPEN_UUCP                    111
#define _SC_XOPEN_VERSION                 112