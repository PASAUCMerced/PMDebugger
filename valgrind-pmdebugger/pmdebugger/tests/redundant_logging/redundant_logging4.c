
#include "../common.h"
#include <stdint.h>

#define FILE_SIZE (16 * 1024 * 1024)

int main ( void )
{
    void *base = make_map_tmpfile(FILE_SIZE);

    int8_t *i8p = base;
    int16_t *i16p = (int16_t *)((uintptr_t)base + 8);
    int32_t *i32p = (int32_t *)((uintptr_t)base + 16);
    int64_t *i64p = (int64_t *)((uintptr_t)base + 24);

    *i8p = 1;

    VALGRIND_PMC_START_TX_N(1234);

    VALGRIND_PMC_ADD_TO_TX_N(1234, i32p, sizeof (*i32p));
    VALGRIND_PMC_ADD_TO_TX_N(1234, i32p, sizeof (*i32p));

    *i8p = 1;
    *i16p = 2;
    *i32p = 3;
    *i64p = 4;

    VALGRIND_PMC_END_TX_N(1234);

    *i64p = 5;

    return 0;
}
