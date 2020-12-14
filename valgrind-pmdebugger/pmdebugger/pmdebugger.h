#ifndef __PMEMCHECK_H
#define __PMEMCHECK_H

#include "valgrind.h"

typedef enum
{
    VG_USERREQ__PMC_REGISTER_PMEM_MAPPING = VG_USERREQ_TOOL_BASE('P', 'C'),
    VG_USERREQ__PMC_REGISTER_PMEM_FILE,
    VG_USERREQ__PMC_REMOVE_PMEM_MAPPING,
    VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING,
    VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS,
    VG_USERREQ__PMC_DO_FLUSH,
    VG_USERREQ__PMC_DO_FENCE,
    VG_USERREQ__PMC_RESERVED1,
    VG_USERREQ__PMC_WRITE_STATS,
    VG_USERREQ__PMC_RESERVED2,
    VG_USERREQ__PMC_RESERVED3,
    VG_USERREQ__PMC_RESERVED4,
    VG_USERREQ__PMC_RESERVED5,
    VG_USERREQ__PMC_RESERVED7,
    VG_USERREQ__PMC_RESERVED8,
    VG_USERREQ__PMC_RESERVED9,
    VG_USERREQ__PMC_RESERVED10,
    VG_USERREQ__PMC_SET_CLEAN,

    VG_USERREQ__PMC_START_TX,
    VG_USERREQ__PMC_START_TX_N,
    VG_USERREQ__PMC_END_TX,
    VG_USERREQ__PMC_END_TX_N,
    VG_USERREQ__PMC_ADD_TO_TX,
    VG_USERREQ__PMC_ADD_TO_TX_N,
    VG_USERREQ__PMC_REMOVE_FROM_TX,
    VG_USERREQ__PMC_REMOVE_FROM_TX_N,
    VG_USERREQ__PMC_ADD_THREAD_TO_TX_N,
    VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N,
    VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE,
    VG_USERREQ__PMC_RESERVED6,
    VG_USERREQ__PMC_EMIT_LOG,

    VG_USERREQ__PMC_EPOCH_BEGIN,
    VG_USERREQ__PMC_EPOCH_END,
    VG_USERREQ__PMC_FENCE_MARK,
    VG_USERREQ__PMC_NEW_STRAND,
    VG_USERREQ__PMC_ORDER_GUARANTEE,
} Vg_PMemCheckClientRequest;

/** Register a persistent memory mapping region */
#define VALGRIND_PMC_REGISTER_PMEM_MAPPING(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                     \
                                    VG_USERREQ__PMC_REGISTER_PMEM_MAPPING, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register a persistent memory file */
#define VALGRIND_PMC_REGISTER_PMEM_FILE(_qzz_desc, _qzz_addr_base,              \
                                        _qzz_size, _qzz_offset)                 \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                          \
                                    VG_USERREQ__PMC_REGISTER_PMEM_FILE,         \
                                    (_qzz_desc), (_qzz_addr_base), (_qzz_size), \
                                    (_qzz_offset), 0)

/** Remove a persistent memory mapping region */
#define VALGRIND_PMC_REMOVE_PMEM_MAPPING(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                   \
                                    VG_USERREQ__PMC_REMOVE_PMEM_MAPPING, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Check if the given range is a registered persistent memory mapping */
#define VALGRIND_PMC_CHECK_IS_PMEM_MAPPING(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                     \
                                    VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register an SFENCE */
#define VALGRIND_PMC_PRINT_PMEM_MAPPINGS                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS, \
                                    0, 0, 0, 0, 0)

/** Register a CLFLUSH-like operation */
#define VALGRIND_PMC_DO_FLUSH(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                        \
                                    VG_USERREQ__PMC_DO_FLUSH, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register an SFENCE */
#define VALGRIND_PMC_DO_FENCE                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_DO_FENCE, \
                                    0, 0, 0, 0, 0)

/** Write tool stats */
#define VALGRIND_PMC_WRITE_STATS                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_WRITE_STATS, \
                                    0, 0, 0, 0, 0)

/** Emit user log */
#define VALGRIND_PMC_EMIT_LOG(_qzz_emit_log)                  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                        \
                                    VG_USERREQ__PMC_EMIT_LOG, \
                                    (_qzz_emit_log), 0, 0, 0, 0)

/** Set a region of persistent memory as clean */
#define VALGRIND_PMC_SET_CLEAN(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                         \
                                    VG_USERREQ__PMC_SET_CLEAN, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Support for transactions */

/** Start an implicit persistent memory transaction */
#define VALGRIND_PMC_START_TX                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_START_TX, \
                                    0, 0, 0, 0, 0)

/** Start an explicit persistent memory transaction */
#define VALGRIND_PMC_START_TX_N(_qzz_txn)                       \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                          \
                                    VG_USERREQ__PMC_START_TX_N, \
                                    (_qzz_txn), 0, 0, 0, 0)

/** End an implicit persistent memory transaction */
#define VALGRIND_PMC_END_TX                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_END_TX, \
                                    0, 0, 0, 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_END_TX_N(_qzz_txn)                       \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                        \
                                    VG_USERREQ__PMC_END_TX_N, \
                                    (_qzz_txn), 0, 0, 0, 0)

/** Add a persistent memory region to the implicit transaction */
#define VALGRIND_PMC_ADD_TO_TX(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                         \
                                    VG_USERREQ__PMC_ADD_TO_TX, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Add a persistent memory region to an explicit transaction */
#define VALGRIND_PMC_ADD_TO_TX_N(_qzz_txn, _qzz_addr, _qzz_len)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                           \
                                    VG_USERREQ__PMC_ADD_TO_TX_N, \
                                    (_qzz_txn), (_qzz_addr), (_qzz_len), 0, 0)

/** Remove a persistent memory region from the implicit transaction */
#define VALGRIND_PMC_REMOVE_FROM_TX(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                              \
                                    VG_USERREQ__PMC_REMOVE_FROM_TX, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Remove a persistent memory region from an explicit transaction */
#define VALGRIND_PMC_REMOVE_FROM_TX_N(_qzz_txn, _qzz_addr, _qzz_len)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                \
                                    VG_USERREQ__PMC_REMOVE_FROM_TX_N, \
                                    (_qzz_txn), (_qzz_addr), (_qzz_len), 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_ADD_THREAD_TX_N(_qzz_txn)                          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                  \
                                    VG_USERREQ__PMC_ADD_THREAD_TO_TX_N, \
                                    (_qzz_txn), 0, 0, 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_REMOVE_THREAD_FROM_TX_N(_qzz_txn)                       \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                                       \
                                    VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N, \
                                    (_qzz_txn), 0, 0, 0, 0)

/** Remove a persistent memory region from the implicit transaction */
#define VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(_qzz_addr, _qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE, \
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

#define VALGRIND_PMC_EPOCH_BEGIN                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_EPOCH_BEGIN, \
                                    0, 0, 0, 0, 0)
#define VALGRIND_PMC_EPOCH_END                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_EPOCH_END, \
                                    0, 0, 0, 0, 0)
#define VALGRIND_PMC_FENCE_MARK                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_FENCE_MARK, \
                                    0, 0, 0, 0, 0)
#define VALGRIND_PMC_NEW_STRAND                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_NEW_STRAND, \
                                    0, 0, 0, 0, 0)
#define VALGRIND_PMC_ORDER_GUARANTEE(_qzz_addr, _qzz_len, _qzz_addr1, _qzz_len1) \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_ORDER_GUARANTEE,             \
                                    (_qzz_addr), (_qzz_len), (_qzz_addr1), (_qzz_len1), 0)
#endif
