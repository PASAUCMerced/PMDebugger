
#include <stdint.h>

#include "../../pmdebugger.h"

int main ( void )
{

    int8_t i8;
    int16_t i16;

    VALGRIND_PMC_REGISTER_PMEM_MAPPING(&i8, sizeof (i8));

    VALGRIND_PMC_START_TX;

    VALGRIND_PMC_ADD_TO_TX(&i16, sizeof (i16));
    VALGRIND_PMC_ADD_TO_TX(&i16, sizeof (i16));
    i16 = 2;

    VALGRIND_PMC_ADD_TO_TX(&i8, sizeof (i8));
    i8 = 1;

    VALGRIND_PMC_WRITE_STATS;

    VALGRIND_PMC_REMOVE_FROM_TX(&i16, sizeof (i16));
    VALGRIND_PMC_REMOVE_FROM_TX(&i8, sizeof (i8));

    i16 = 2;

    VALGRIND_PMC_END_TX;

    VALGRIND_PMC_REMOVE_PMEM_MAPPING(&i8, sizeof (i8));

    return 0;
}
