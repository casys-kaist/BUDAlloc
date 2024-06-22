#ifndef __STDARG_H
#define __STDARG_H

#define va_list __builtin_va_list

#define va_start(va, p) __builtin_va_start((va), (p))
#define va_arg(va, t) __builtin_va_arg((va), t)
#define va_end(va) __builtin_va_end((va))

#define va_copy(d, s) __builtin_va_copy((d), (s))

#endif