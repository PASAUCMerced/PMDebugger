
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

    VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(i8p, sizeof (*i8p));

    VALGRIND_PMC_START_TX;

    VALGRIND_PMC_ADD_TO_TX(i8p, sizeof (*i8p));
    VALGRIND_PMC_ADD_TO_TX(i8p, sizeof (*i8p));

    VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(i16p, sizeof (*i16p));

    VALGRIND_PMC_ADD_TO_TX(i32p, sizeof (*i32p));
    VALGRIND_PMC_ADD_TO_TX(i32p, sizeof (*i32p));

    /* dirty stores */
    *i8p = 1;
    *i16p = 2;
    *i32p = 3;

    /* ignore out-of-transaction stores to this region */
    VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(i64p, sizeof (*i64p));
    *i64p = 4;

    VALGRIND_PMC_END_TX;

    return 0;
}
