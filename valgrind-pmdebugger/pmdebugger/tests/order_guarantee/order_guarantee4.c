
#include "order_guarantee4_config.h"
#include "../common.h"
#include <stdint.h>

#define FILE_SIZE (16 * 1024 * 1024)

int main ( void )
{
    void *base = make_map_tmpfile(FILE_SIZE);

    int8_t *i8p = base;
    int16_t *i16p = (int16_t *)((uintptr_t)base + 8);
    ABEFOREB1; 
    *i8p = 1;
    VALGRIND_PMC_DO_FLUSH(i8p, 1);
    VALGRIND_PMC_DO_FENCE;
    *i16p = 2;
    VALGRIND_PMC_DO_FLUSH(i16p, 2);
    VALGRIND_PMC_DO_FENCE;
   
    return 0;
}
