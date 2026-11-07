#include <stdio.h>
typedef char *va_list;
void __va_start(va_list*,...);
#pragma intrinsic(__va_start)
__declspec(dllimport) void *__acrt_iob_func(unsigned);
__declspec(dllimport) int __stdio_common_vfprintf(unsigned long long,void*,const char*,void*,va_list);
__declspec(dllimport) int __stdio_common_vsprintf(unsigned long long,char*,size_t,const char*,void*,va_list);
int printf(const char *fmt,...){va_list ap;__va_start(&ap,fmt);return __stdio_common_vfprintf(0,__acrt_iob_func(1),fmt,NULL,ap);}
int snprintf(char *out,size_t n,const char *fmt,...){va_list ap;__va_start(&ap,fmt);return __stdio_common_vsprintf(2,out,n,fmt,NULL,ap);}
int _fltused=0;
