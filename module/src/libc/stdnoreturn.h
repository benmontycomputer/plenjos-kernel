#ifndef _STDNORETURN_H_
#define _STDNORETURN_H_

// Based on the C11 standard
// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf

#define _Noreturn __attribute__((noreturn))

#endif /* _STDNORETURN_H_ */