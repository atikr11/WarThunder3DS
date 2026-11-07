#pragma once
float sinf(float);float cosf(float);float expf(float);float sqrtf(float);static inline float hostfabs(float a){return a<0?-a:a;}
#define fabsf hostfabs
float fmaxf(float,float);float fminf(float,float);float atan2f(float,float);float powf(float,float);float copysignf(float,float);float tanf(float);float fmodf(float,float);float ceilf(float);int _finite(double);
#define isfinite(x) _finite(x)

