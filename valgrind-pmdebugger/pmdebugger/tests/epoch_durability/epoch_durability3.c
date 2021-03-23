
#include "../common.h"
#include <stdint.h>

#define FILE_SIZE (16 * 1024 * 1024)

int main ( void )
{
    void *base = make_map_tmpfile(FILE_SIZE);

    int8_t *i8p = base;
    int16_t *i16p = (int16_t *)((uintptr_t)base + 8);

    *i8p = 1;

    VALGRIND_PMC_START_TX_N(1234);

    VALGRIND_PMC_ADD_TO_TX_N(1234, i16p, sizeof (*i16p));


    *i8p = 1;
    *i16p = 2;

    *i16p = 2;

    VALGRIND_PMC_END_TX_N(1234);


    return 0;
}
