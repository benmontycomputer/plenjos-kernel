#ifndef _PLENJOS_LIMITS_H
#define _PLENJOS_LIMITS_H 1

#define PROCESS_NAME_MAX 64

#define NAME_MAX 255
#define PATH_MAX 4096

#define SSIZE_MAX INT64_MAX

#ifndef SIZE_MAX
#define SIZE_MAX  UINT64_MAX
#endif

#endif /* _PLENJOS_LIMITS_H */