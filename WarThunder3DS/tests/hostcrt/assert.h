#pragma once
#include <stdio.h>
__declspec(dllimport) void ExitProcess(unsigned);
#define assert(c) ((c)?(void)0:(printf("ASSERT FAILED: %s, line %d\n",#c,__LINE__),ExitProcess(1)))
