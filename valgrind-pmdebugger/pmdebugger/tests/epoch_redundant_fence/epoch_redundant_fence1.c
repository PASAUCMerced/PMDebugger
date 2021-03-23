
#include "../common.h"
#include <stdint.h>

#define FILE_SIZE (16 * 1024 * 1024)

int main ( void )
{
    void *base = make_map_tmpfile(FILE_SIZE);

    int8_t *i8p = base;
    int16_t *i16p = (int16_t *)((uintptr_t)base + 8);
    int32_t *i32p = (int32_t *)((uintptr_t)base + 16);

    VALGRIND_PMC_START_TX;
    VALGRIND_PMC_ADD_TO_TX(i8p, sizeof (*i8p));
    VALGRIND_PMC_ADD_TO_TX(i16p, sizeof (*i16p));

    *i8p = 1;
    VALGRIND_PMC_DO_FENCE;
    VALGRIND_PMC_FENCE_MARK;
    *i32p = 3;
    VALGRIND_PMC_DO_FENCE;
    VALGRIND_PMC_FENCE_MARK;
    VALGRIND_PMC_END_TX;
    *i16p = 2;

    return 0;
}
