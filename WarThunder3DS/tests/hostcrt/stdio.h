#pragma once
#include <stdlib.h>
int printf(const char*,...);int snprintf(char*,size_t,const char*,...);int puts(const char*);
typedef void FILE;FILE *fopen(const char*,const char*);int fclose(FILE*);size_t fwrite(const void*,size_t,size_t,FILE*);
