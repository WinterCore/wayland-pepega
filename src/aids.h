#ifndef AIDS
#define AIDS

#include <math.h>

#define DEV true

#define DEBUG(...) \
    if (DEV) { \
        printf("[DEBUG]: "); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        fflush(stdout); \
    }

#define ERR(...) \
    fprintf(stderr, "[ERROR]: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    fflush(stderr)

#define internal static
#define local_persist static
#define global_variable static
#define CLAMP(low, high, x) \
    x < low ? low : (x > high) ? high : x

#define LERP(min, max, t) \
    (t * (max - min) + min)

#define INVLERP(min, max, x) \
    CLAMP(0, 1, (float) (x - min) / (float) (max - min))

#define INVLERPANG(min, max, x) \
    fmod((float) (x - min) / (float) (max - min), 1)

#define REMAP(iMin, iMax, oMin, oMax, x) \
    LERP(oMin, oMax, INVLERP(iMin, iMax, x))


#endif
