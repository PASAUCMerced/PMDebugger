
#include "../common.h"
#include "order_guarantee1_config.h"

#define FILE_SIZE (16 * 1024 * 1024)

int main ( void )
{
    void *base = make_map_tmpfile(FILE_SIZE);

    int64_t *a = base;
    int64_t *b = base+128;
    ABEFOREB1;

    *a = 4;
     *b=10;
    VALGRIND_PMC_DO_FLUSH(b, 64);
    VALGRIND_PMC_DO_FENCE;
   
    VALGRIND_PMC_DO_FLUSH(a, 64);
    VALGRIND_PMC_DO_FENCE;
    return 0;
}
