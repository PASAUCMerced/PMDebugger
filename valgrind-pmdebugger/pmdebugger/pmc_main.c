#include <sys/param.h>
#include "pub_tool_libcfile.h"
#include <fcntl.h>
#include "pub_tool_oset.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_machine.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_debuginfo.h"

#include "pmdebugger.h"
#include "pmc_include.h"

#include "pub_tool_libcproc.h"
#ifdef GETTIME

ULong time_fence;
ULong time_flush;
ULong time_memory;
#endif
static void
db_add_and_merge_store(struct pmem_st *region);

#define MAX_REDUNDANT_FENCE 10000UL
#define MAX_MULT_OVERWRITES 10000UL
#define MAX_ARRAY_NUM 100000000UL

#define MAX_FLUSH_ERROR_EVENTS 10000UL

#define triop(_op, _arg1, _arg2, _arg3) \
    IRExpr_Triop((_op), (_arg1), (_arg2), (_arg3))
#define binop(_op, _arg1, _arg2) IRExpr_Binop((_op), (_arg1), (_arg2))
#define unop(_op, _arg) IRExpr_Unop((_op), (_arg))
#define mkU1(_n) IRExpr_Const(IRConst_U1(_n))
#define mkU8(_n) IRExpr_Const(IRConst_U8(_n))
#define mkU16(_n) IRExpr_Const(IRConst_U16(_n))
#define mkU32(_n) IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n) IRExpr_Const(IRConst_U64(_n))
#define mkexpr(_tmp) IRExpr_RdTmp((_tmp))

/** Max store size */
#define MAX_DSIZE 256

/** Max allowable path length */
#define MAX_PATH_SIZE 4096
struct pmem_stores_array
{
    UWord snippet_info_index;

    struct pmem_st *pmem_stores;
    struct arr_snippet *snippet_info;
};

Word cmp_pmem_own_st(const void *key, const void *elem)
{
    const struct pmem_st *lhs = (const struct pmem_st *)(key);
    const struct pmem_st *rhs = (const struct pmem_st *)(elem);

    if (lhs->index < rhs->index)
        return -1;
    else if (lhs->index > rhs->index)
        return 1;
    else
        return 0;
}

/** Holds parameters and runtime data */
static struct pmem_ops
{
    /** Set of stores to persistent memory. */

    OSet *pmem_stores;
    Addr min_add_pmem_store;
    Addr max_add_pmem_store;
    UWord not_pmem_num;
    struct pmem_stores_array array;

    OSet *debug_memory_merge;

    /** Set of registered persistent memory regions. */
    OSet *pmem_mappings;
    Addr min_add_pmem_mappings;
    Addr max_add_pmem_mappings;

    /** Holds possible multiple overwrite error events. */
    struct pmem_st **multiple_stores;

    /** Holds the number of registered multiple overwrites. */
    UWord multiple_stores_reg;

    /** Holds possible redundant flush events. */
    struct pmem_st **redundant_flushes;

    /** Holds the number of registered redundant flush events. */
    UWord redundant_flushes_reg;

    /** Holds superfluous flush error events. */
    struct pmem_st **superfluous_flushes;

    /** Holds the number of superfluous flush events. */
    UWord superfluous_flushes_reg;

    /** Within this many SBlocks a consecutive write is not considered
    * a poss_leak. */
    UWord store_sb_indiff;

    /** Turns on multiple overwrite error tracking. */
    Bool track_multiple_stores;

    /** Turns on logging persistent memory events. */
    Bool log_stores;

    /** Toggles summary printing. */
    Bool print_summary;

    /** Toggles checking multiple and superfluous flushes */
    Bool check_flush;

    /** The size of the cache line */
    Long flush_align_size;

    /** Force flush alignment to native cache line size */
    Bool force_flush_align;

    /** Toggles transaction tracking. */
    Bool transactions_only;

    /** Toggles store stacktrace logging. */
    Bool store_traces;

    /** Depth of the printed store stacktrace. */
    UInt store_traces_depth;

    /** Toggles automatic ISA recognition. */
    Bool automatic_isa_rec;

    /** Toggles error summary message */
    Bool error_summary;

    /** Simulate 2-phase flushing. */
    Bool weak_clflush;

    /**epoch detection*/
    Bool epoch;
    UWord epoch_fence;
    UWord redundant_fence_reg;
    ExeContext **redundant_fence;
    Bool tree_reorganization;
    Bool redundant_logging;
    Bool epoch_durability_fence;
    UWord epoch_durability_reg;
    Bool print_bug_detail;
    Bool order_guarantee;
    OSet *pmem_time_records;
    UWord global_time_stamp;
    OSet *pmem_order_records;
    UWord order_guarantee_reg;
    UWord global_time_records_index;
    Bool lack_ordering_trand;
    UWord lack_ordering_trand_reg;
#ifdef MemDistance
    UWord memDistance;
    UWord maxDistance;
#endif
#ifdef MemInstInGroup
    UWord memInstNum;
    UWord allflushgroupNum;
#endif
#ifdef ALLFLUSH
    UWord partflushNum;
    UWord allflushNum;
#endif
#ifdef InstrDisbu
    UWord flushNum;
    UWord fenceNum;
    UWord memoryNum;
#endif

} pmem;

struct strand_pmem
{
    struct pmem_ops *p_pmem;
    struct strand_pmem *pp;
    struct strand_pmem *pn;
    UWord strand_id;
};
static struct strand_pmem *curr_strand_pmem = NULL;

void pmem_st_copy(struct pmem_st *copy, struct pmem_st *orig)
{

    UWord index = copy->index;
    *copy = *orig;
    copy->index = index;
}
void init_snippet_info(void)
{
    pmem.array.snippet_info_index++;
    pmem.array.snippet_info[pmem.array.snippet_info_index].min_add = pmem.array.snippet_info[pmem.array.snippet_info_index].max_add = -1;
    pmem.array.snippet_info[pmem.array.snippet_info_index].flushed_num = NO_FLUSHED;
    pmem.array.snippet_info[pmem.array.snippet_info_index].index = pmem.array.snippet_info[pmem.array.snippet_info_index].start_index = pmem.array.snippet_info[pmem.array.snippet_info_index - 1].index;
}

/** A specific kind of expression. */
typedef IRExpr IRAtom;

/** Types of discernable events. */
typedef enum
{
    Event_Ir,
    Event_Dr,
    Event_Dw,
    Event_Dm
} EventKind;

/** The event structure. */
typedef struct
{
    EventKind ekind;
    IRAtom *addr;
    SizeT size;
    IRAtom *guard;
    IRAtom *value;
} Event;

/** Number of sblock run. */
static ULong sblocks = 0;

/**
* \brief Check if a given store overlaps with registered persistent memory
*        regions.
* \param[in] addr The base address of the store.
* \param[in] size The size of the store.
* \return True if store overlaps with any registered region, false otherwise.
*/

static Bool
is_pmem_access(Addr addr, SizeT size)
{
    struct pmem_st tmp = {0};
    tmp.size = size;
    tmp.addr = addr;
    if ((tmp.addr > pmem.max_add_pmem_mappings) || ((tmp.addr + tmp.size) < pmem.min_add_pmem_mappings))
        return False;
    else
        return VG_(OSetGen_Contains)(pmem.pmem_mappings, &tmp);
}

/**
* \brief State to string change for information purposes.
*/
static const char *
store_state_to_string(enum store_state state)
{
    switch (state)
    {
    case STST_CLEAN:
        return "CLEAN";
    case STST_DIRTY:
        return "DIRTY";
    case STST_FLUSHED:
        return "FLUSHED";
    default:
        return NULL;
    }
}

/**
 * \brief Prints registered redundant flushes.
 *
 * \details Flushing regions of memory which have already been flushed, but not
 * committed to memory, is a possible performance issue. This is not a data
 * consistency related problem.
 */
static void
print_redundant_fences(void)
{
    if (pmem.redundant_fence_reg == 0)
        return;
    VG_(umsg)
    ("Number of redundantly fence: %lu\n",
     pmem.redundant_fence_reg);
    ExeContext *tmp;
    Int i;
    for (i = 0; i < pmem.redundant_fence_reg; ++i)
    {
        tmp = pmem.redundant_fence[i];
        VG_(umsg)
        ("[%d] ", i);
        if (pmem.tree_reorganization || pmem.print_bug_detail)
        {
            VG_(pp_ExeContext)
            (tmp);
        }
    }
}
static void
print_redundant_flushes(void)
{
    VG_(umsg)
    ("\nNumber of redundantly flushed stores: %lu\n",
     pmem.redundant_flushes_reg);
    VG_(umsg)
    ("Stores flushed multiple times:\n");
    struct pmem_st *tmp;
    Int i;
    for (i = 0; i < pmem.redundant_flushes_reg; ++i)
    {
        tmp = pmem.redundant_flushes[i];
        VG_(umsg)
        ("[%d] ", i);
        if (pmem.tree_reorganization || pmem.print_bug_detail)
        {
            VG_(pp_ExeContext)
            (tmp->context);
        }

        VG_(umsg)
        ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
         tmp->addr, tmp->size, store_state_to_string(tmp->state));
    }
}

/**
 * \brief Prints registered superfluous flushes.
 *
 * \details Flushing clean (with no pending stores to flush) regions of memory
 * is most certainly an error in the algorithm. This is not a data consistency
 * related problem, but a performance issue.
 */
static void
print_superfluous_flushes(void)
{
    VG_(umsg)
    ("\nNumber of unnecessary flushes: %lu\n",
     pmem.superfluous_flushes_reg);
    struct pmem_st *tmp;
    Int i;
    for (i = 0; i < pmem.superfluous_flushes_reg; ++i)
    {
        tmp = pmem.superfluous_flushes[i];
        VG_(umsg)
        ("[%d] ", i);
        if (pmem.tree_reorganization || pmem.print_bug_detail)
        {
            VG_(pp_ExeContext)
            (tmp->context);
        }

        VG_(umsg)
        ("\tAddress: 0x%lx\tsize: %llu\n", tmp->addr, tmp->size);
    }
}

/**
 * \brief Prints registered multiple stores.
 *
 * \details Overwriting stores before they are made persistent suggests
 * an error in the algorithm. This could be both a data consistency and
 * performance issue.
 */
static void
print_multiple_stores(void)
{

    VG_(umsg)
    ("\nNumber of overwritten stores: %lu\n",
     pmem.multiple_stores_reg);
    VG_(umsg)
    ("Overwritten stores before they were made persistent:\n");
    struct pmem_st *tmp;
    Int i;
    for (i = 0; i < pmem.multiple_stores_reg; ++i)
    {
        tmp = pmem.multiple_stores[i];
        VG_(umsg)
        ("[%d] ", i);
        if (pmem.tree_reorganization || pmem.print_bug_detail)
        {
            VG_(pp_ExeContext)
            (tmp->context);
        }

        VG_(umsg)
        ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
         tmp->addr, tmp->size, store_state_to_string(tmp->state));
    }
}

/**
 * \brief Prints registered store statistics.
 *
 * \details Print outstanding stores which were not made persistent during the
 * whole run of the application.
 */
static void
print_store_stats(void)
{
    UWord total = 0;
    UWord k = 0;
    struct pmem_st *tmp;
    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
        {
            {
                tmp = pmem.array.pmem_stores + i;
                if (pmem.array.snippet_info[s_index].flushed_num == ALL_FLUSHED)
                    tmp->state = STST_FLUSHED;
                VG_(umsg)
                (" [%lu] ", k);
                if (pmem.tree_reorganization || pmem.print_bug_detail)
                {
                    VG_(pp_ExeContext)
                    (tmp->context);
                }
                VG_(umsg)
                ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
                 tmp->addr, tmp->size, store_state_to_string(tmp->state));
                total += tmp->size;
                ++k;
            }
        }
    }
    if (VG_(OSetGen_Size)(pmem.pmem_stores) != 0)
    {

        VG_(OSetGen_ResetIter)
        (pmem.pmem_stores);

        while ((tmp = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
        {

            if (pmem.tree_reorganization || pmem.print_bug_detail)
            {
                VG_(pp_ExeContext)
                (tmp->context);
            }
            VG_(umsg)
            ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
             tmp->addr, tmp->size, store_state_to_string(tmp->state));
            total += tmp->size;
            ++k;
        }
    }

    pmem.not_pmem_num = k;
}

static void
print_store_stats_detail(void)
{
    pmem.not_pmem_num = 0;
    if (pmem.debug_memory_merge == NULL)
        pmem.debug_memory_merge = VG_(OSetGen_Create)(/*keyOff*/ 0, cmp_pmem_st,
                                                      VG_(malloc), "pmc.main.cpci.8", VG_(free));
    else
    {
        VG_(OSetGen_Destroy)
        (pmem.debug_memory_merge);
        pmem.debug_memory_merge = VG_(OSetGen_Create)(/*keyOff*/ 0, cmp_pmem_st,
                                                      VG_(malloc), "pmc.main.cpci.8", VG_(free));
    }

    struct pmem_st *tmp;
    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
        {
            tmp = pmem.array.pmem_stores + i;
            if (tmp->is_delete == True)
                continue;
            if (pmem.array.snippet_info[s_index].flushed_num == ALL_FLUSHED)
                tmp->state = STST_FLUSHED;
            db_add_and_merge_store(tmp);
        }
    }
    if (VG_(OSetGen_Size)(pmem.pmem_stores) != 0)
    {
        VG_(OSetGen_ResetIter)
        (pmem.pmem_stores);

        while ((tmp = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
        {

            db_add_and_merge_store(tmp);
        }
    }

    VG_(umsg)
    ("Number of stores not made persistent: %u\n", VG_(OSetGen_Size)(pmem.debug_memory_merge));

    if (VG_(OSetGen_Size)(pmem.debug_memory_merge) != 0)
    {
        VG_(OSetGen_ResetIter)
        (pmem.debug_memory_merge);

        UWord total = 0;
        Int i = 0;
        VG_(umsg)
        ("Stores not made persistent properly:\n");
        while ((tmp = VG_(OSetGen_Next)(pmem.debug_memory_merge)) != NULL)
        {
            VG_(umsg)
            ("[%d] ", i);
            if (pmem.tree_reorganization || pmem.print_bug_detail)
            {
                VG_(pp_ExeContext)
                (tmp->context);
            }

            VG_(umsg)
            ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
             tmp->addr, tmp->size, store_state_to_string(tmp->state));
            total += tmp->size;
            ++i;
        }
        pmem.not_pmem_num = i;

        VG_(umsg)
        ("Total memory not made persistent: %lu\n", total);
    }
}
/**
* \brief Prints the error message for exceeding the maximum allowable
*        overwrites.
* \param[in] limit The limit to print.
*/
static void
print_max_poss_overwrites_error(UWord limit)
{
    VG_(umsg)
    ("The number of overwritten stores exceeded %lu\n\n",
     limit);

    VG_(umsg)
    ("This either means there is something fundamentally wrong with"
     " your program, or you are using your persistent memory as "
     "volatile memory.\n");
    VG_(message_flush)
    ();

    print_multiple_stores();
}

/**
* \brief Prints the error message for exceeding the maximum allowable
*        number of superfluous flushes.
* \param[in] limit The limit to print.
*/
static void
print_superfluous_flush_error(UWord limit)
{
    VG_(umsg)
    ("The number of superfluous flushes exceeded %lu\n\n",
     limit);

    VG_(umsg)
    ("This means your program is constantly flushing regions of"
     " memory, where no stores were made. This is a performance"
     " issue.\n");
    VG_(message_flush)
    ();

    print_superfluous_flushes();
}

/**
* \brief Prints the error message for exceeding the maximum allowable
*        number of redundant flushes.
* \param[in] limit The limit to print.
*/
static void
print_redundant_flush_error(UWord limit)
{
    VG_(umsg)
    ("The number of redundant flushes exceeded %lu\n\n",
     limit);

    VG_(umsg)
    ("This means your program is constantly flushing regions of"
     " memory, which have already been flushed. This is a performance"
     " issue.\n");
    VG_(message_flush)
    ();

    print_redundant_flushes();
}

/**
 * \brief Prints registered store context.
 *
 * \details Print store context.
 */
static void
print_store_ip_desc(UInt n, DiEpoch ep, Addr ip, void *uu_opaque)
{
    InlIPCursor *iipc = VG_(new_IIPC)(ep, ip);

    VG_(emit)
    (";");

    do
    {
        const HChar *buf = VG_(describe_IP)(ep, ip, iipc);

        if (VG_(clo_xml))
            VG_(printf_xml)
        ("%s\n", buf);
        else VG_(emit)("%s", buf);

        n++;
    } while (VG_(next_IIPC)(iipc));

    VG_(delete_IIPC)
    (iipc);
}

/**
 * \brief Prints stack trace.
 *
 * \details Print stack trace.
 */
static void
pp_store_trace(const struct pmem_st *store, UInt n_ips)
{
    n_ips = n_ips == 0 ? VG_(get_ExeContext_n_ips)(store->context) : n_ips;

    tl_assert(n_ips > 0);

    if (VG_(clo_xml))
        VG_(printf_xml)
    ("    <stack>\n");

    DiEpoch ep = VG_(current_DiEpoch)();
    VG_(apply_StackTrace)
    (print_store_ip_desc, NULL, ep,
     VG_(get_ExeContext_StackTrace(store->context)), n_ips);

    if (VG_(clo_xml))
        VG_(printf_xml)
    ("    </stack>\n");
}

/**
 * \brief Check if a memcpy/memset is at the given instruction address.
 *
 * \param[in] ip The instruction address to check.
 * \return True if the function name has memcpy/memset in its name,
 *         False otherwise.
 */
static Bool
is_ip_memset_memcpy(Addr ip)
{
    DiEpoch ep = VG_(current_DiEpoch)();
    InlIPCursor *iipc = VG_(new_IIPC)(ep, ip);
    const HChar *buf = VG_(describe_IP)(ep, ip, iipc);
    Bool present = (VG_(strstr)(buf, "memcpy") != NULL);
    present |= (VG_(strstr)(buf, "memset") != NULL);
    VG_(delete_IIPC)
    (iipc);
    return present;
}

/**
 * \brief Compare two ExeContexts.
 * Checks if two ExeContext are equal not counting the possible first
 * memset/memcpy function in the callstack.
 *
 * \param[in] lhs The first ExeContext to compare.
 * \param[in] rhs The second ExeContext to compare.
 *
 * Return True if the ExeContexts are equal, not counting the first
 * memcpy/memset function, False otherwise.
 */
static Bool
cmp_exe_context(const ExeContext *lhs, const ExeContext *rhs)
{
    if (lhs == NULL || rhs == NULL)
        return False;

    if (lhs == rhs)
        return True;

    UInt n_ips1;
    UInt n_ips2;
    const Addr *ips1 = VG_(make_StackTrace_from_ExeContext)(lhs, &n_ips1);
    const Addr *ips2 = VG_(make_StackTrace_from_ExeContext)(rhs, &n_ips2);

    tl_assert(n_ips1 >= 1 && n_ips2 >= 1);

    if (n_ips1 != n_ips2)
        return False;

    Int i = 0;
    if ((ips1[0] == ips2[0]) || (is_ip_memset_memcpy(ips1[0]) && is_ip_memset_memcpy(ips2[0])))
        ++i;

    for (; i < n_ips1; i++)
        if (ips1[i] != ips2[i])
            return False;

    return True;
}

/**
 * \brief Checks if two stores are merge'able.
 * Does not check the adjacency of the stores. Checks only the context and state
 * of the store.
 *
 * \param[in] lhs The first store to check.
 * \param[in] rhs The second store to check.
 *
 * \return True if stores are merge'able, False otherwise.
 */
static Bool
is_store_mergeable(const struct pmem_st *lhs,
                   const struct pmem_st *rhs)
{
    Bool state_eq = lhs->state == rhs->state;
    return state_eq && cmp_exe_context(lhs->context, rhs->context);
}

/**
 * \brief Merge two stores together.
 * Does not check whether the two stores can in fact be merged. The two stores
 * should be adjacent or overlapping for the merging to make sense.
 *
 * \param[in,out] to_merge the store with which the merge will happen.
 * \param[in] to_be_merged the store that will be merged.
 */
static inline void
merge_stores(struct pmem_st *to_merge,
             const struct pmem_st *to_be_merged)
{
    ULong max_addr = MAX(to_merge->addr + to_merge->size,
                         to_be_merged->addr + to_be_merged->size);
    to_merge->addr = MIN(to_merge->addr, to_be_merged->addr);
    to_merge->size = max_addr - to_merge->addr;
}

typedef void (*split_clb)(struct pmem_st *store, OSet *set, Bool preallocated);

/**
 * \brief Free store if it was preallocated.
 *
 * \param[in,out] store The store to be freed.
 * \param[in,out] set The set the store belongs to.
 * \param[in] preallocated True if the store is in the heap and not the stack.
 */
static void
free_clb(struct pmem_st *store, OSet *set, Bool preallocated)
{
    if (preallocated)
        VG_(OSetGen_FreeNode)
    (set, store);
}

/**
 * \brief Issues a warning event with the given store as the offender.
 *
 * \param[in,out] store The store to be registered as a warning.
 * \param[in,out] set The set the store belongs to.
 * \param[in] preallocated True if the store is in the heap and not the stack.
 */
static void
add_mult_overwrite_warn(struct pmem_st *store, OSet *set, Bool preallocated)
{

    struct pmem_st *new = VG_(OSetGen_AllocNode)(set,
                                                 (SizeT)sizeof(struct pmem_st));
    new->index = db_pmem_store_index;
    db_pmem_store_index++;
    pmem_st_copy(new, store);

    add_warning_event(pmem.multiple_stores, &pmem.multiple_stores_reg,
                      new, MAX_MULT_OVERWRITES, print_max_poss_overwrites_error);
}

/**
 * \brief Splits-adjusts the two given stores so that they do not overlap.
 *
 * The stores need to be from the same set and have to overlap.
 *
 * \param[in,out] old The old store that will be modified.
 * \param[in] new The new store that will not be modified.
 * \param[in,out] set The set both of the stores belong to.
 * \param[in,out] clb The callback to be called for the overlapping part of the
 *  old store.
 */
static void
split_stores(struct pmem_st *old, const struct pmem_st *new, OSet *set,
             split_clb clb, Bool is_arr)
{
    Addr new_max = new->addr + new->size;
    Addr old_max = old->addr + old->size;
    struct pmem_st tmp;

    if (old->addr >= new->addr && old_max <= new_max)
    {
        if (is_arr)
        {
            old->is_delete = True;
        }
        else
        {
            tmp = *old;
            VG_(OSetGen_Remove)
            (set, old);
            VG_(OSetGen_FreeNode)
            (set, old);
            VG_(OSetGen_ResetIterAt)
            (set, &tmp);
        }
        clb(old, set, False);
        return;
    }

    if (old->addr < new->addr)
    {

        if (old_max > new_max)
        {

            struct pmem_st *after;

            after = VG_(OSetGen_AllocNode)(set,
                                           (SizeT)sizeof(struct pmem_st));
            after->index = db_pmem_store_index;
            db_pmem_store_index++;
            pmem_st_copy(after, old);

            after->addr = new_max;
            after->size = old_max - new_max;
            after->value &= (1 << (after->size * 8 + 1)) - 1;

            old->value >>= old_max - new->addr;
            old->size = new->addr - old->addr;

            VG_(OSetGen_Insert)
            (set, after);
            VG_(OSetGen_ResetIterAt)
            (set, old);

            tmp = *new;
            tmp.context = old->context;
            clb(&tmp, set, False);
        }
        else
        {

            tmp = *old;
            tmp.addr = new->addr;
            tmp.size = old_max - new->addr;

            clb(&tmp, set, False);
            old->value >>= old_max - new->addr;
            old->size = new->addr - old->addr;
        }
        return;
    }

    if (old_max > new_max)
    {

        tmp = *old;
        tmp.size -= old_max - new_max;
        clb(&tmp, set, False);

        old->addr = new_max;
        old->size = old_max - new_max;
        old->value &= (1 << (old->size * 8 + 1)) - 1;
        return;
    }

    tl_assert(False);
}

/**
 * \brief Add and merges adjacent stores if possible.
 * Should not be used if track_multiple_stores is enabled.
 *
 * param[in,out] region the store to be added and merged with adjacent stores.
 */

static void
add_and_merge_store(struct pmem_st *region)
{
    struct pmem_st *old_entry;

    while ((old_entry = VG_(OSetGen_Lookup)(pmem.pmem_stores, region)) != NULL)
        split_stores(old_entry, region, pmem.pmem_stores, free_clb, False);

    struct pmem_st search_entry = *region;
    search_entry.addr -= 1;
    int i = 0;
    for (i = 0; i < 2; ++i, search_entry.addr += 2)
    {
        old_entry = VG_(OSetGen_Lookup)(pmem.pmem_stores, &search_entry);

        if (old_entry == NULL)
            continue;

        if (!is_store_mergeable(region, old_entry))
            continue;

        merge_stores(region, old_entry);
        old_entry = VG_(OSetGen_Remove)(pmem.pmem_stores, &search_entry);
        VG_(OSetGen_FreeNode)
        (pmem.pmem_stores, old_entry);
    }
    VG_(OSetGen_Insert)
    (pmem.pmem_stores, region);
}

static void
db_add_and_merge_store(struct pmem_st *region)
{
    struct pmem_st *old_entry;

    while ((old_entry = VG_(OSetGen_Lookup)(pmem.debug_memory_merge, region)) != NULL)
        split_stores(old_entry, region, pmem.debug_memory_merge, free_clb, False);

    struct pmem_st search_entry = *region;
    search_entry.addr -= 1;
    int i = 0;
    for (i = 0; i < 2; ++i, search_entry.addr += 2)
    {
        old_entry = VG_(OSetGen_Lookup)(pmem.debug_memory_merge, &search_entry);

        if (old_entry == NULL)
            continue;

        if (!is_store_mergeable(region, old_entry))
            continue;

        merge_stores(region, old_entry);
        old_entry = VG_(OSetGen_Remove)(pmem.debug_memory_merge, &search_entry);
        VG_(OSetGen_FreeNode)
        (pmem.debug_memory_merge, old_entry);
    }
    struct pmem_st *final_entry = VG_(OSetGen_AllocNode)(pmem.debug_memory_merge, (SizeT)sizeof(struct pmem_st));
    final_entry->index = db_pmem_store_index;
    db_pmem_store_index++;
    pmem_st_copy(final_entry, region);

    VG_(OSetGen_Insert)
    (pmem.debug_memory_merge, final_entry);
}

static void update_pmem_stores_head_minandmax(struct pmem_st *store)
{
    Addr store_max = store->addr + store->size;
    if (pmem.array.snippet_info[pmem.array.snippet_info_index].max_add == -1 && pmem.array.snippet_info[pmem.array.snippet_info_index].min_add == -1)
    {
        pmem.array.snippet_info[pmem.array.snippet_info_index].min_add = store->addr;
        pmem.array.snippet_info[pmem.array.snippet_info_index].max_add = store_max;
    }
    if (pmem.array.snippet_info[pmem.array.snippet_info_index].min_add > store->addr)
        pmem.array.snippet_info[pmem.array.snippet_info_index].min_add = store->addr;
    if (pmem.array.snippet_info[pmem.array.snippet_info_index].max_add < store_max)
        pmem.array.snippet_info[pmem.array.snippet_info_index].max_add = store_max;

    return;
}
static void update_Oset_pmem_stores_minandmax(struct pmem_st *store)
{
    Addr store_max = store->addr + store->size;
    if (pmem.min_add_pmem_store == -1 && pmem.max_add_pmem_store == -1)
    {
        pmem.min_add_pmem_store = store->addr;
        pmem.max_add_pmem_store = store_max;
    }
    if (pmem.min_add_pmem_store > store->addr)
        pmem.min_add_pmem_store = store->addr;
    if (pmem.max_add_pmem_store < store_max)
        pmem.max_add_pmem_store = store_max;

    return;
}
static int cmp_with_arr_minandmax(struct pmem_st *p_st, int index)
{
    Addr p_st_max = p_st->addr + p_st->size;
    if ((p_st->addr > pmem.array.snippet_info[index].max_add) || (p_st_max < pmem.array.snippet_info[index].min_add))
        return -1;
    else if (pmem.array.snippet_info[index].min_add <= p_st->addr && p_st_max <= pmem.array.snippet_info[index].max_add)
        return 1;
    else
        return 0;
}
/**
 * \brief Handle a new store checking for multiple overwrites.
 * This should be called when track_multiple_stores is enabled.
 *
 * \param[in,out] store the store to be handled.
 */
static void
handle_with_mult_stores(struct pmem_st *store)
{
    struct pmem_st *existing;

    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        if (cmp_with_arr_minandmax(store, s_index) != -1)
        {
            for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
            {
                if (LIKELY(pmem.array.pmem_stores[i].is_delete != True))
                    existing = pmem.array.pmem_stores + i;
                else
                    continue;
                if (cmp_pmem_st(existing, store) == 0)
                {

                    if ((store->block_num - existing->block_num) < pmem.store_sb_indiff && existing->addr == store->addr && existing->size == store->size && existing->value == store->value)
                    {
                        existing->is_delete = True;
                        continue;
                    }
                    split_stores(existing, store, pmem.pmem_stores,
                                 add_mult_overwrite_warn, True);
                }
            }
        }
    }
    if ((store->addr > pmem.max_add_pmem_store) || ((store->addr + store->size) < pmem.min_add_pmem_store))
        ;
    else
    {
        VG_(OSetGen_ResetIter)
        (pmem.pmem_stores);
        struct pmem_st iter_at;
        while ((existing = VG_(OSetGen_Next)(pmem.pmem_stores)) !=
               NULL)
        {

            if (cmp_pmem_st(existing, store) != 0)
                continue;

            if ((store->block_num - existing->block_num) < pmem.store_sb_indiff && existing->addr == store->addr && existing->size == store->size && existing->value == store->value)
            {
                iter_at = *store;
                VG_(OSetGen_Remove)
                (pmem.pmem_stores, store);
                VG_(OSetGen_FreeNode)
                (pmem.pmem_stores, existing);
                VG_(OSetGen_ResetIterAt)
                (pmem.pmem_stores, &iter_at);
                continue;
            }
            split_stores(existing, store, pmem.pmem_stores,
                         add_mult_overwrite_warn, False);
        }
    }
}

/**
* \brief Trace the given store if it was to any of the registered persistent
*        memory regions.
* \param[in] addr The base address of the store.
* \param[in] size The size of the store.
* \param[in] value The value of the store.
*/
static VG_REGPARM(3) void trace_pmem_store(Addr addr, SizeT size, UWord value)
{

    if (LIKELY(!is_pmem_access(addr, size)))
        return;

#ifdef InstrDisbu
    VG_(umsg)
    ("store\n");
    pmem.memoryNum++;
#endif
#ifdef GETTIME
    struct vki_timeval tv_begin;
    struct vki_timeval tv_end;
    VG_(gettimeofday)
    (&tv_begin, NULL);
#endif

    struct pmem_st *store;
    UWord old_index = pmem.array.snippet_info[pmem.array.snippet_info_index].index;
    if (LIKELY(pmem.array.snippet_info[pmem.array.snippet_info_index].index < MAX_ARRAY_NUM))
    {
        store = pmem.array.pmem_stores + pmem.array.snippet_info[pmem.array.snippet_info_index].index;
    }
    else
    {

        store = VG_(OSetGen_AllocNode)(pmem.pmem_stores, sizeof(struct pmem_st));
        store->index = db_pmem_store_index;
        db_pmem_store_index++;
    }
    if (pmem.epoch)
        store->inEpoch = True;
    else
        store->is_delete = False;

    store->addr = addr;
    store->size = size;
    store->state = STST_DIRTY;
    store->block_num = sblocks;
    store->value = value;
    store->is_delete = False;

#ifdef MemDistance
    store->pmemDistance = pmem.memDistance;
#endif

    if (pmem.tree_reorganization || pmem.print_bug_detail)
        store->context = VG_(record_ExeContext)(VG_(get_running_tid)(), 0);

    if (pmem.log_stores)
    {
        VG_(emit)
        ("|STORE;0x%lx;0x%lx;0x%lx", addr, value, size);
        if (pmem.store_traces)
            pp_store_trace(store, pmem.store_traces_depth);
    }
    if (pmem.track_multiple_stores)
        handle_with_mult_stores(store);

    if (pmem.order_guarantee)
    {
        struct pmem_st_order search_pmem;
        struct pmem_st_order *old_pmem;
        search_pmem.addr = addr;
        search_pmem.size = size;
        VG_(OSetGen_ResetIter)
        (pmem.pmem_time_records);
        while ((old_pmem = VG_(OSetGen_Next)(pmem.pmem_time_records)) != NULL)
        {
            if (cmp_pmem_st_order(&search_pmem, old_pmem) == 0)
            {
                old_pmem->timestamp_begin = pmem.global_time_stamp;
                old_pmem->timestamp_end = -1;
                if (pmem.tree_reorganization || pmem.print_bug_detail)
                {
                    old_pmem->context = store->context;
                }
            }
        }
    }

    if (UNLIKELY(old_index >= MAX_ARRAY_NUM))
    {
        if (pmem.tree_reorganization)
            add_and_merge_store(store);
        else
            VG_(OSetGen_Insert)
            (pmem.pmem_stores, store);
        update_Oset_pmem_stores_minandmax(store);
    }
    else
    {

        pmem.array.snippet_info[pmem.array.snippet_info_index].index++;
        update_pmem_stores_head_minandmax(store);
    }

    handle_tx_store(store);

#ifdef GETTIME
    VG_(gettimeofday)
    (&tv_end, NULL);
    time_memory += (tv_end.tv_sec * 1000000ULL + tv_end.tv_usec) - (tv_begin.tv_sec * 1000000ULL + tv_begin.tv_usec);
#endif
}

/**
* \brief Register the entry of a new SB.
*
* Useful when handling implementation independent multiple writes under
* the same address.
*/
static void
add_one_SB_entered(void)
{
    ++sblocks;
}

/**
* \brief Make a new atomic expression from e.
*
* A very handy function to have for creating binops, triops and widens.
* \param[in,out] sb The IR superblock to which the new expression will be added.
* \param[in] ty The IRType of the expression.
* \param[in] e The new expression to make.
* \return The Rd_tmp of the new expression.
*/
static IRAtom *
make_expr(IRSB *sb, IRType ty, IRExpr *e)
{
    IRTemp t;
    IRType tyE = typeOfIRExpr(sb->tyenv, e);

    tl_assert(tyE == ty);

    t = newIRTemp(sb->tyenv, tyE);
    addStmtToIRSB(sb, IRStmt_WrTmp(t, e));

    return mkexpr(t);
}

/**
* \brief Check if the expression needs to be widened.
* \param[in] sb The IR superblock to which the expression belongs.
* \param[in] e The checked expression.
* \return True if needs to be widened, false otherwise.
*/
static Bool
tmp_needs_widen(IRType type)
{
    switch (type)
    {
    case Ity_I1:
    case Ity_I8:
    case Ity_I16:
    case Ity_I32:
        return True;

    default:
        return False;
    }
}

/**
* \brief Check if the const expression needs to be widened.
* \param[in] e The checked expression.
* \return True if needs to be widened, false otherwise.
*/
static Bool
const_needs_widen(IRAtom *e)
{

    tl_assert(e->tag == Iex_Const);

    switch (e->Iex.Const.con->tag)
    {
    case Ico_U1:
    case Ico_U8:
    case Ico_U16:
    case Ico_U32:
    case Ico_U64:
        return True;

    default:
        return False;
    }
}

/**
* \brief Widen a given const expression to a word sized expression.
* \param[in] e The expression being widened.
* \return The widened const expression.
*/
static IRAtom *
widen_const(IRAtom *e)
{

    tl_assert(e->tag == Iex_Const);

    switch (e->Iex.Const.con->tag)
    {
    case Ico_U1:
        return mkIRExpr_HWord((UInt)e->Iex.Const.con->Ico.U1);

    case Ico_U8:
        return mkIRExpr_HWord((UInt)e->Iex.Const.con->Ico.U8);

    case Ico_U16:
        return mkIRExpr_HWord((UInt)e->Iex.Const.con->Ico.U16);

    case Ico_U32:
        return mkIRExpr_HWord((UInt)e->Iex.Const.con->Ico.U32);

    case Ico_U64:
        return mkIRExpr_HWord((UInt)e->Iex.Const.con->Ico.U64);

    default:
        tl_assert(False);
    }
}

/**
* \brief A generic widening function.
* \param[in] sb The IR superblock to which the expression belongs.
* \param[in] e The expression being widened.
* \return The widening operation.
*/
static IROp
widen_operation(IRSB *sb, IRAtom *e)
{
    switch (typeOfIRExpr(sb->tyenv, e))
    {
    case Ity_I1:
        return Iop_1Uto64;

    case Ity_I8:
        return Iop_8Uto64;

    case Ity_I16:
        return Iop_16Uto64;

    case Ity_I32:
        return Iop_32Uto64;

    default:
        tl_assert(False);
    }
}

/**
* \brief Handle wide sse operations.
* \param[in,out] sb The IR superblock to which add expressions.
* \param[in] end The endianess.
* \param[in] addr The expression with the address of the operation.
* \param[in] data The expression with the value of the operation.
* \param[in] guard The guard expression.
* \param[in] size The size of the operation.
*/
static void
handle_wide_expr(IRSB *sb, IREndness end, IRAtom *addr, IRAtom *data,
                 IRAtom *guard, SizeT size)
{
    IROp mkAdd;
    IRType ty, tyAddr;
    void *helper = trace_pmem_store;
    const HChar *hname = "trace_pmem_store";

    ty = typeOfIRExpr(sb->tyenv, data);

    tyAddr = typeOfIRExpr(sb->tyenv, addr);
    mkAdd = tyAddr == Ity_I32 ? Iop_Add32 : Iop_Add64;
    tl_assert(tyAddr == Ity_I32 || tyAddr == Ity_I64);
    tl_assert(end == Iend_LE || end == Iend_BE);

    Int i;
    Int parts = 0;

    UInt offs[4];

    IROp ops[4];
    IRDirty *dis[4];
    IRAtom *addrs[4];
    IRAtom *datas[4];
    IRAtom *eBiass[4];

    if (ty == Ity_V256)
    {

        ops[0] = Iop_V256to64_0;
        ops[1] = Iop_V256to64_1;
        ops[2] = Iop_V256to64_2;
        ops[3] = Iop_V256to64_3;

        if (end == Iend_LE)
        {
            offs[0] = 0;
            offs[1] = 8;
            offs[2] = 16;
            offs[3] = 24;
        }
        else
        {
            offs[3] = 0;
            offs[2] = 8;
            offs[1] = 16;
            offs[0] = 24;
        }

        parts = 4;
    }
    else if (ty == Ity_V128)
    {

        ops[0] = Iop_V128to64;
        ops[1] = Iop_V128HIto64;

        if (end == Iend_LE)
        {
            offs[0] = 0;
            offs[1] = 8;
        }
        else
        {
            offs[0] = 8;
            offs[1] = 0;
        }

        parts = 2;
    }

    for (i = 0; i < parts; ++i)
    {
        eBiass[i] = tyAddr == Ity_I32 ? mkU32(offs[i]) : mkU64(offs[i]);
        addrs[i] = make_expr(sb, tyAddr, binop(mkAdd, addr, eBiass[i]));
        datas[i] = make_expr(sb, Ity_I64, unop(ops[i], data));
        dis[i] = unsafeIRDirty_0_N(3, hname,
                                   VG_(fnptr_to_fnentry)(helper), mkIRExprVec_3(addrs[i], mkIRExpr_HWord(size / parts), datas[i]));
        if (guard)
            dis[i]->guard = guard;

        addStmtToIRSB(sb, IRStmt_Dirty(dis[i]));
    }
}

/**
* \brief Add a guarded write event.
* \param[in,out] sb The IR superblock to which the expression belongs.
* \param[in] daddr The expression with the address of the operation.
* \param[in] dsize The size of the operation.
* \param[in] guard The guard expression.
* \param[in] value The expression with the value of the operation.
*/
static void
add_event_dw_guarded(IRSB *sb, IRAtom *daddr, Int dsize, IRAtom *guard,
                     IRAtom *value)
{
    tl_assert(isIRAtom(daddr));
    tl_assert(isIRAtom(value));
    tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

    const HChar *helperName = "trace_pmem_store";
    void *helperAddr = trace_pmem_store;
    IRExpr **argv;
    IRDirty *di;
    IRType type = typeOfIRExpr(sb->tyenv, value);

    if (value->tag == Iex_RdTmp && type == Ity_I64)
    {

        argv = mkIRExprVec_3(daddr, mkIRExpr_HWord(dsize),
                             value);
        di = unsafeIRDirty_0_N(/*regparms*/ 3, helperName,
                               VG_(fnptr_to_fnentry)(helperAddr), argv);
        if (guard)
        {
            di->guard = guard;
        }
        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
    else if (value->tag == Iex_RdTmp && type == Ity_F64)
    {
        argv = mkIRExprVec_3(daddr, mkIRExpr_HWord(dsize),
                             make_expr(sb, Ity_I64, unop(Iop_ReinterpF64asI64, value)));
        di = unsafeIRDirty_0_N(/*regparms*/ 3, helperName,
                               VG_(fnptr_to_fnentry)(helperAddr), argv);
        if (guard)
        {
            di->guard = guard;
        }
        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
    else if (value->tag == Iex_RdTmp && tmp_needs_widen(type))
    {

        argv = mkIRExprVec_3(daddr, mkIRExpr_HWord(dsize),
                             make_expr(sb, Ity_I64, unop(widen_operation(sb, value), value)));
        di = unsafeIRDirty_0_N(/*regparms*/ 3, helperName,
                               VG_(fnptr_to_fnentry)(helperAddr), argv);
        if (guard)
        {
            di->guard = guard;
        }
        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
    else if (value->tag == Iex_Const && const_needs_widen(value))
    {

        argv = mkIRExprVec_3(daddr, mkIRExpr_HWord(dsize),
                             widen_const(value));
        di = unsafeIRDirty_0_N(/*regparms*/ 3, helperName,
                               VG_(fnptr_to_fnentry)(helperAddr), argv);
        if (guard)
        {
            di->guard = guard;
        }
        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
    else if (type == Ity_V128 || type == Ity_V256)
    {
        handle_wide_expr(sb, Iend_LE, daddr, value, guard, dsize);
    }
    else
    {
        VG_(umsg)
        ("Unable to trace store - unsupported type of store 0x%x 0x%x\n",
         value->tag, type);
    }
}

/**
* \brief Add an ordinary write event.
* \param[in,out] sb The IR superblock to which the expression belongs.
* \param[in] daddr The expression with the address of the operation.
* \param[in] dsize The size of the operation.
* \param[in] value The expression with the value of the operation.
*/
static void
add_event_dw(IRSB *sb, IRAtom *daddr, Int dsize, IRAtom *value)
{
    add_event_dw_guarded(sb, daddr, dsize, NULL, value);
}
void check_pmem_order(void)
{
    VG_(OSetGen_ResetIter)
    (pmem.pmem_order_records);
    struct order_record *tmp = NULL;
    struct pmem_st_order *pmemA, *pmemB;
    while ((tmp = VG_(OSetGen_Next)(pmem.pmem_order_records)) != NULL)
    {
        pmemA = tmp->a;
        pmemB = tmp->b;
        if (!(pmemA->timestamp_end < pmemB->timestamp_begin))
        {

            pmem.order_guarantee_reg++;
            VG_(umsg)
            ("Not order guarantee: \n");
            if (pmem.tree_reorganization || pmem.print_bug_detail)
            {
                VG_(pp_ExeContext)
                (pmemA->context);
                VG_(pp_ExeContext)
                (pmemB->context);
            }
            VG_(umsg)
            ("\tFirst Address: 0x%lx\tFirst size: %lu\tSecond Address: 0x%lx\tSecond size: %lu\n", pmemA->addr, pmemA->size, pmemB->addr, pmemB->size);
        }
    }
}
void mark_epoch_fence(void)
{
    if (pmem.epoch)
    {
        pmem.epoch_fence++;
        if (pmem.epoch_fence > MAX_REDUNDANT_FENCE - 1)
        {
            VG_(umsg)
            ("exceed the capacity of MAX_REDUNDANT_FENCE");
            return;
        }
        if (pmem.epoch_fence > 1)
        {
            pmem.redundant_fence[pmem.redundant_fence_reg] = VG_(record_ExeContext)(VG_(get_running_tid)(), 0);
            pmem.redundant_fence_reg++;
        }
    }
}

void epoch_begin(void)
{
    pmem.epoch = True;
}

void epoch_end(UWord txid)
{
    if (pmem.epoch != True)
    {
        VG_(umsg)
        ("Cannot find EPOCH_BEGIN\n");
        return;
    }
    if (!is_nest_trans(txid))
    {
        pmem.epoch = False;
    }
    pmem.epoch_fence = 0;

    struct pmem_st *being_fenced = NULL;
    VG_(OSetGen_ResetIter)
    (pmem.pmem_stores);
    while ((being_fenced = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
    {
        if (being_fenced->inEpoch)
        {
            VG_(umsg)
            ("Not persistent memory even if the epoch ends: \n");
            if (pmem.tree_reorganization || pmem.print_bug_detail)
                VG_(pp_ExeContext)
            (being_fenced->context);
            VG_(umsg)
            ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
             being_fenced->addr, being_fenced->size, store_state_to_string(being_fenced->state));
            being_fenced->inEpoch = False;
            pmem.epoch_durability_reg++;
        }
    }

    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
        {
            being_fenced = pmem.array.pmem_stores + i;
            if (being_fenced->inEpoch && being_fenced->is_delete != True)
            {
                VG_(umsg)
                ("Not persistent memory even if the epoch ends: \n");
                if (pmem.tree_reorganization || pmem.print_bug_detail)
                    VG_(pp_ExeContext)
                (being_fenced->context);
                VG_(umsg)
                ("\tAddress: 0x%lx\tsize: %llu\tstate: %s\n",
                 being_fenced->addr, being_fenced->size, store_state_to_string(being_fenced->state));
                being_fenced->inEpoch = False;
                pmem.epoch_durability_reg++;
            }
        }
    }
}

/**
* \brief Register a fence.
*
* Marks flushed stores as persistent.
* The proper state transitions are DIRTY->FLUSHED->CLEAN.
* The CLEAN state is not registered, the store is removed from the set.
*/
static void
do_fence(void)
{
#ifdef InstrDisbu
    VG_(umsg)
    ("fence\n");
    pmem.fenceNum++;
#endif

#ifdef MemDistance
    pmem.memDistance++;
#endif
#ifdef GETTIME
    struct vki_timeval tv_begin;
    struct vki_timeval tv_end;
    VG_(gettimeofday)
    (&tv_begin, NULL);
#endif

    if (pmem.log_stores)
        VG_(emit)
    ("|FENCE");

    struct pmem_st *being_fenced = NULL;

    if (pmem.order_guarantee)
    {

        struct pmem_st_order *old_pmem;
        struct pmem_st *tmp_array;
        struct pmem_st tmp_tree;
        Addr max_order_addr = 0;
        Addr min_order_addr = 0;
        VG_(OSetGen_ResetIter)
        (pmem.pmem_time_records);
        while ((old_pmem = VG_(OSetGen_Next)(pmem.pmem_time_records)) != NULL)
        {
            min_order_addr = old_pmem->addr;
            max_order_addr = old_pmem->addr + old_pmem->size;
            for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
            {
                if (pmem.array.snippet_info[s_index].flushed_num == ALL_FLUSHED)
                {
                    if (!(min_order_addr > pmem.array.snippet_info[s_index].max_add || max_order_addr < pmem.array.snippet_info[s_index].min_add))
                    {
                        old_pmem->timestamp_end = pmem.global_time_stamp;
                        tmp_array = pmem.array.pmem_stores + pmem.array.snippet_info[s_index].start_index;
                    }
                }
                else if (pmem.array.snippet_info[s_index].flushed_num == PART_FLUSHED)
                {
                    if (!(min_order_addr > pmem.array.snippet_info[s_index].max_add || max_order_addr < pmem.array.snippet_info[s_index].min_add))
                    {
                        for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
                        {
                            tmp_array = pmem.array.pmem_stores + i;
                            if (tmp_array->state == STST_FLUSHED && !(min_order_addr > (tmp_array->addr + tmp_array->size) || max_order_addr < tmp_array->addr))
                            {
                                old_pmem->timestamp_end = pmem.global_time_stamp;
                            }
                        }
                    }
                }
            }
            tmp_tree.addr = old_pmem->addr;
            tmp_tree.size = old_pmem->size;
            VG_(OSetGen_ResetIter)
            (pmem.pmem_stores);
            while ((tmp_array = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
            {
                if (cmp_pmem_st(tmp_array, &tmp_tree) == 0 && tmp_array->state == STST_FLUSHED)
                {
                    old_pmem->timestamp_end = pmem.global_time_stamp;
                }
            }
        }
    }

    pmem.min_add_pmem_store = pmem.max_add_pmem_store = -1;
    VG_(OSetGen_ResetIter)
    (pmem.pmem_stores);
    while ((being_fenced = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
    {

        if (being_fenced->state == STST_FLUSHED)
        {
#ifdef MemDistance
            if ((pmem.memDistance - being_fenced->pmemDistance) > pmem.maxDistance)
                pmem.maxDistance = pmem.memDistance - being_fenced->pmemDistance;
            VG_(umsg)
            ("distance: %lu \n", pmem.memDistance - being_fenced->pmemDistance);
#endif

            struct pmem_st temp = *being_fenced;
            VG_(OSetGen_Remove)
            (pmem.pmem_stores, being_fenced);
            VG_(OSetGen_FreeNode)
            (pmem.pmem_stores, being_fenced);

            VG_(OSetGen_ResetIterAt)
            (pmem.pmem_stores, &temp);
        }
        else
        {
            update_Oset_pmem_stores_minandmax(being_fenced);
        }
    }
#ifdef GETTIME
    VG_(gettimeofday)
    (&tv_end, NULL);
    time_fence += (tv_end.tv_sec * 1000000ULL + tv_end.tv_usec) - (tv_begin.tv_sec * 1000000ULL + tv_begin.tv_usec);
#endif

    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        if (pmem.array.snippet_info[s_index].flushed_num == ALL_FLUSHED)
        {
#ifdef YCSBmemInstInGroup
            VG_(umsg)
            ("%lu\n", (pmem.array.snippet_info[s_index].index - pmem.array.snippet_info[s_index].start_index));
#endif
#ifdef MemInstInGroup
            pmem.allflushgroupNum++;
            pmem.memInstNum += (pmem.array.snippet_info[s_index].index - pmem.array.snippet_info[s_index].start_index);
#endif
#ifdef ALLFLUSH
            pmem.allflushNum++;
            VG_(umsg)
            ("allFlush\n");
#endif
#ifdef MemDistance

            for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
            {
                being_fenced = pmem.array.pmem_stores + i;
                if ((pmem.memDistance - being_fenced->pmemDistance) > pmem.maxDistance)
                    pmem.maxDistance = pmem.memDistance - being_fenced->pmemDistance;
                VG_(umsg)
                ("distance: %lu \n", pmem.memDistance - being_fenced->pmemDistance);
            }
#endif

            continue;
        }
        else
        {
#ifdef ALLFLUSH
            if (pmem.array.snippet_info[s_index].flushed_num == PART_FLUSHED)
            {
                VG_(umsg)
                ("partFlush\n");
                pmem.partflushNum++;
            }
#endif
            for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
            {
                being_fenced = pmem.array.pmem_stores + i;

                if (being_fenced->state == STST_FLUSHED || being_fenced->is_delete == True)
                {

#ifdef MemDistance
                    if ((pmem.memDistance - being_fenced->pmemDistance) > pmem.maxDistance)
                        pmem.maxDistance = pmem.memDistance - being_fenced->pmemDistance;
                    VG_(umsg)
                    ("distance: %lu \n", pmem.memDistance - being_fenced->pmemDistance);
#endif

                    continue;
                }
                else
                {
                    struct pmem_st *new = VG_(OSetGen_AllocNode)(pmem.pmem_stores, sizeof(struct pmem_st));
                    new->index = db_pmem_store_index;
                    db_pmem_store_index++;

                    pmem_st_copy(new, being_fenced);

                    if (pmem.tree_reorganization)
                        add_and_merge_store(new);
                    else
                        VG_(OSetGen_Insert)
                    (pmem.pmem_stores, new);
                    update_Oset_pmem_stores_minandmax(new);
                }
            }
        }
    }

    pmem.array.snippet_info[0].min_add = pmem.array.snippet_info[0].max_add = -1;
    pmem.array.snippet_info[0].flushed_num = NO_FLUSHED;
    pmem.array.snippet_info[0].index = pmem.array.snippet_info[0].start_index = 0;
    pmem.array.snippet_info_index = 0;

    if (pmem.order_guarantee)
    {
        pmem.global_time_stamp++;
    }
}

/**
* \brief Register a flush.
*
* Marks dirty stores as flushed. The proper state transitions are
* DIRTY->FLUSHED->FENCED->COMMITTED->CLEAN. The CLEAN state is not registered,
* the store is removed from the set.
*
* \param[in] base The base address of the flush.
* \param[in] size The size of the flush in bytes.
*/
static void
do_flush(UWord base, UWord size)
{

#ifdef GETTIME
    struct vki_timeval tv_begin;
    struct vki_timeval tv_end;
    VG_(gettimeofday)
    (&tv_begin, NULL);
#endif

    struct pmem_st flush_info = {0};

    if (LIKELY(pmem.force_flush_align == False))
    {
        flush_info.addr = base;
        flush_info.size = size;
    }
    else
    {

        flush_info.addr = base & ~(pmem.flush_align_size - 1);
        flush_info.size = roundup(size, pmem.flush_align_size);
    }

#ifdef InstrDisbu
    pmem.flushNum++;
    VG_(umsg)
    ("flush\n");
#endif
    if (pmem.log_stores)
        VG_(emit)
    ("|FLUSH;0x%lx;0x%llx", flush_info.addr, flush_info.size);

    Addr flush_max = flush_info.addr + flush_info.size;
    struct pmem_st *being_flushed;
    Bool valid_flush = False;

    if (pmem.lack_ordering_trand && curr_strand_pmem->strand_id > 0)
    {
        VG_(OSetGen_ResetIter)
        (pmem.pmem_order_records);
        struct order_record *tmp = NULL;
        struct pmem_st_order flush_order;
        while ((tmp = VG_(OSetGen_Next)(pmem.pmem_order_records)) != NULL)
        {
            flush_order.addr = flush_info.addr;
            flush_order.size = flush_info.size;
            if (cmp_pmem_st_order(&flush_order, tmp->b) == 0)
            {

                pmem.lack_ordering_trand_reg++;
                VG_(umsg)
                ("Violating order guarantee because of multi-strands: \n");
                if (pmem.tree_reorganization || pmem.print_bug_detail)
                {
                    VG_(pp_ExeContext)
                    (VG_(record_ExeContext)(VG_(get_running_tid)(), 0));
                }
                VG_(umsg)
                ("\t flush Address: 0x%lx\tflush size: %lu\n", flush_order.addr, flush_order.size);
            }
        }
    }

    for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
    {
        if (flush_max >= pmem.array.snippet_info[s_index].max_add && pmem.array.snippet_info[s_index].min_add >= flush_info.addr && pmem.array.snippet_info[s_index].flushed_num == NO_FLUSHED)
        {
            pmem.array.snippet_info[s_index].flushed_num = ALL_FLUSHED;
            valid_flush = True;
        }
        else if (flush_max < pmem.array.snippet_info[s_index].min_add || pmem.array.snippet_info[s_index].max_add < flush_info.addr)
        {
            continue;
        }
        else
        {

            for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
            {
                being_flushed = pmem.array.pmem_stores + i;
                if (being_flushed->is_delete == True)
                {
                    being_flushed->state = STST_FLUSHED;
                    continue;
                }

                if (cmp_pmem_st(&flush_info, being_flushed) != 0)
                    continue;

                valid_flush = True;

                if (pmem.array.snippet_info[s_index].flushed_num == ALL_FLUSHED || being_flushed->state != STST_DIRTY)
                {
                    if (pmem.check_flush)
                    {

                        struct pmem_st *wrong_flush = VG_(malloc)("pmc.main.cpci.3",
                                                                  sizeof(struct pmem_st));
                        *wrong_flush = *being_flushed;
                        wrong_flush->state = STST_FLUSHED;

                        add_warning_event(pmem.redundant_flushes,
                                          &pmem.redundant_flushes_reg,
                                          wrong_flush, MAX_FLUSH_ERROR_EVENTS,
                                          print_redundant_flush_error);
                    }
                    continue;
                }

                being_flushed->state = STST_FLUSHED;
                pmem.array.snippet_info[s_index].flushed_num = PART_FLUSHED;

                if (being_flushed->addr < flush_info.addr)
                {

                    struct pmem_st *split;
                    UWord old_index = pmem.array.snippet_info[pmem.array.snippet_info_index].index;
                    if (LIKELY(pmem.array.snippet_info[pmem.array.snippet_info_index].index < MAX_ARRAY_NUM))
                    {
                        split = pmem.array.pmem_stores + pmem.array.snippet_info[pmem.array.snippet_info_index].index;

                        pmem.array.snippet_info[pmem.array.snippet_info_index].index++;
                    }
                    else
                    {
                        split = VG_(OSetGen_AllocNode)(pmem.pmem_stores, sizeof(struct pmem_st));
                        split->index = db_pmem_store_index;
                        db_pmem_store_index++;
                    }

                    pmem_st_copy(split, being_flushed);

                    split->size = flush_info.addr - being_flushed->addr;
                    split->state = STST_DIRTY;

                    being_flushed->addr = flush_info.addr;
                    being_flushed->size -= split->size;
                    if (old_index >= MAX_ARRAY_NUM)
                    {
                        if (pmem.tree_reorganization)
                            add_and_merge_store(split);
                        else
                            VG_(OSetGen_Insert)
                        (pmem.pmem_stores, split);
                    }
                }

                if (being_flushed->addr + being_flushed->size > flush_max)
                {

                    UWord old_index = pmem.array.snippet_info[pmem.array.snippet_info_index].index;
                    struct pmem_st *split;
                    if (LIKELY(pmem.array.snippet_info[pmem.array.snippet_info_index].index < MAX_ARRAY_NUM))
                    {
                        split = pmem.array.pmem_stores + pmem.array.snippet_info[pmem.array.snippet_info_index].index;
                        pmem.array.snippet_info[pmem.array.snippet_info_index].index++;
                    }
                    else
                    {
                        split = VG_(OSetGen_AllocNode)(pmem.pmem_stores, sizeof(struct pmem_st));
                        split->index = db_pmem_store_index;
                        db_pmem_store_index++;
                    }
                    pmem_st_copy(split, being_flushed);

                    split->addr = flush_max;
                    split->size = being_flushed->addr + being_flushed->size - flush_max;
                    split->state = STST_DIRTY;

                    being_flushed->size -= split->size;
                    if (old_index >= MAX_ARRAY_NUM)
                    {
                        if (pmem.tree_reorganization)
                            add_and_merge_store(split);
                        else
                            VG_(OSetGen_Insert)
                        (pmem.pmem_stores, split);
                    }
                }
            }
        }
    }

    if ((flush_info.addr > pmem.max_add_pmem_store) || (flush_max < pmem.min_add_pmem_store))
        ;
    else
    {

        VG_(OSetGen_ResetIter)
        (pmem.pmem_stores);
        while ((being_flushed = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
        {

            if (cmp_pmem_st(&flush_info, being_flushed) != 0)
            {
                continue;
            }

            valid_flush = True;

            if (being_flushed->state != STST_DIRTY)
            {
                if (pmem.check_flush)
                {

                    struct pmem_st *wrong_flush = VG_(malloc)("pmc.main.cpci.3",
                                                              sizeof(struct pmem_st));
                    *wrong_flush = *being_flushed;
                    wrong_flush->state = STST_FLUSHED;

                    add_warning_event(pmem.redundant_flushes,
                                      &pmem.redundant_flushes_reg,
                                      wrong_flush, MAX_FLUSH_ERROR_EVENTS,
                                      print_redundant_flush_error);
                }
                continue;
            }

            being_flushed->state = STST_FLUSHED;

            if (being_flushed->addr < flush_info.addr)
            {

                struct pmem_st *split =
                    VG_(OSetGen_AllocNode)(pmem.pmem_stores, (SizeT)sizeof(struct pmem_st));

                split->index = db_pmem_store_index;
                db_pmem_store_index++;
                pmem_st_copy(split, being_flushed);

                split->size = flush_info.addr - being_flushed->addr;
                split->state = STST_DIRTY;

                being_flushed->addr = flush_info.addr;
                being_flushed->size -= split->size;
                VG_(OSetGen_Insert)
                (pmem.pmem_stores, split);

                VG_(OSetGen_ResetIterAt)
                (pmem.pmem_stores, being_flushed);
            }

            if (being_flushed->addr + being_flushed->size > flush_max)
            {

                struct pmem_st *split =
                    VG_(OSetGen_AllocNode)(pmem.pmem_stores, (SizeT)sizeof(struct pmem_st));

                split->index = db_pmem_store_index;
                db_pmem_store_index++;
                split->addr = flush_max;
                split->size = being_flushed->addr + being_flushed->size - flush_max;
                split->state = STST_DIRTY;

                being_flushed->size -= split->size;
                VG_(OSetGen_Insert)
                (pmem.pmem_stores, split);

                VG_(OSetGen_ResetIterAt)
                (pmem.pmem_stores, being_flushed);
            }
        }
    }

    if (!valid_flush && pmem.check_flush)
    {

        struct pmem_st *wrong_flush = VG_(malloc)("pmc.main.cpci.6",
                                                  sizeof(struct pmem_st));
        *wrong_flush = flush_info;
        wrong_flush->context = VG_(record_ExeContext)(VG_(get_running_tid)(),
                                                      0);
        add_warning_event(pmem.superfluous_flushes,
                          &pmem.superfluous_flushes_reg,
                          wrong_flush, MAX_FLUSH_ERROR_EVENTS,
                          print_superfluous_flush_error);
    }
#ifdef GETTIME
    VG_(gettimeofday)
    (&tv_end, NULL);
    time_flush += (tv_end.tv_sec * 1000000ULL + tv_end.tv_usec) - (tv_begin.tv_sec * 1000000ULL + tv_begin.tv_usec);
#endif
    if (pmem.array.snippet_info[pmem.array.snippet_info_index].flushed_num != NO_FLUSHED)
    {
        init_snippet_info();
    }
}

/**
 * \brief Register runtime flush.
 * \param addr[in] addr The expression with the address of the operation.
 */
static VG_REGPARM(1) void trace_pmem_flush(Addr addr)
{

    do_flush(addr, pmem.flush_align_size);
}

/**
* \brief Add an ordinary flush event.
* \param[in,out] sb The IR superblock to which the expression belongs.
* \param[in] daddr The expression with the address of the operation.
*/
static void
add_flush_event(IRSB *sb, IRAtom *daddr)
{
    tl_assert(isIRAtom(daddr));

    const HChar *helperName = "trace_pmem_flush";
    void *helperAddr = trace_pmem_flush;
    IRExpr **argv;
    IRDirty *di;

    argv = mkIRExprVec_1(daddr);
    di = unsafeIRDirty_0_N(/*regparms*/ 1, helperName,
                           VG_(fnptr_to_fnentry)(helperAddr), argv);

    addStmtToIRSB(sb, IRStmt_Dirty(di));
}

/**
* \brief Add an event without any parameters.
* \param[in,out] sb The IR superblock to which the expression belongs.
*/
static void
add_simple_event(IRSB *sb, void *helperAddr, const HChar *helperName)
{
    IRDirty *di;

    di = unsafeIRDirty_0_N(/*regparms*/ 0, helperName,
                           VG_(fnptr_to_fnentry)(helperAddr), mkIRExprVec_0());

    addStmtToIRSB(sb, IRStmt_Dirty(di));
}

/**
* \brief Read the cache line size - linux specific.
* \return The size of the cache line.
*/
static Int
read_cache_line_size(void)
{

    Int ret_val = 64;

    int fp;
    if ((fp = VG_(fd_open)("/proc/cpuinfo", O_RDONLY, 0)) < 0)
    {
        return ret_val;
    }

    int proc_read_size = 2048;
    char read_buffer[proc_read_size];

    while (VG_(read)(fp, read_buffer, proc_read_size - 1) > 0)
    {
        static const char clflush[] = "clflush size\t: ";
        read_buffer[proc_read_size] = 0;

        char *cache_str = NULL;
        if ((cache_str = VG_(strstr)(read_buffer, clflush)) != NULL)
        {

            cache_str += sizeof(clflush) - 1;
            ret_val = VG_(strtoll10)(cache_str, NULL) ?: 64;
            break;
        }
    }

    VG_(close)
    (fp);
    return ret_val;
}

/**
* \brief Try to register a file mapping.
* \param[in] fd The file descriptor to be registered.
* \param[in] addr The address at which this file will be mapped.
* \param[in] size The size of the registered file mapping.
* \param[in] offset Offset within the mapped file.
* \return Returns 1 on success, 0 otherwise.
*/
static UInt
register_new_file(Int fd, UWord base, UWord size, UWord offset)
{
    char fd_path[64];
    VG_(sprintf(fd_path, "/proc/self/fd/%d", fd));
    UInt retval = 0;

    char *file_name = VG_(malloc)("pmc.main.nfcc", MAX_PATH_SIZE);
    int read_length = VG_(readlink)(fd_path, file_name, MAX_PATH_SIZE - 1);
    if (read_length <= 0)
    {
        retval = 1;
        goto out;
    }

    file_name[read_length] = 0;

    if (pmem.log_stores)
        VG_(emit)
    ("|REGISTER_FILE;%s;0x%lx;0x%lx;0x%lx", file_name, base,
     size, offset);
out:
    VG_(free)
    (file_name);
    return retval;
}

/**
 * \brief Print the summary of whole analysis.
 */
static void
print_general_summary(void)
{
    UWord all_errors = pmem.redundant_flushes_reg +
                       pmem.superfluous_flushes_reg +
                       pmem.multiple_stores_reg +
                       pmem.not_pmem_num + pmem.redundant_fence_reg + pmem.epoch_durability_reg + pmem.order_guarantee_reg + pmem.lack_ordering_trand_reg +
                       get_tx_all_err();

    VG_(umsg)
    ("ERROR SUMMARY: %lu errors\n", all_errors);
}

/**
* \brief Print tool statistics.
*/
static void
print_pmem_stats(Bool append_blank_line)
{
    if (pmem.print_bug_detail)
        print_store_stats_detail();
    else
        print_store_stats();

    print_tx_summary();
    print_redundant_fences();

    if (pmem.redundant_flushes_reg)
        print_redundant_flushes();

    if (pmem.superfluous_flushes_reg)
        print_superfluous_flushes();

    if (pmem.track_multiple_stores && (pmem.multiple_stores_reg > 0))
        print_multiple_stores();

    if (pmem.error_summary)
    {
        print_general_summary();
    }
    if (append_blank_line)
        VG_(umsg)
    ("\n");
}

/**
* \brief Print the registered persistent memory mappings
*/
static void
print_persistent_mappings(void)
{
    VG_(OSetGen_ResetIter)
    (pmem.pmem_mappings);
    struct pmem_st *mapping;
    Int i = 0;
    while ((mapping = VG_(OSetGen_Next)(pmem.pmem_mappings)) != NULL)
    {
        VG_(umsg)
        ("[%d] Mapping base: 0x%lx\tsize: %llu\n", i++, mapping->addr,
         mapping->size);
    }
}

/**
* \brief Print gdb monitor commands.
*/
static void
print_monitor_help(void)
{
    VG_(gdb_printf)
    ("\n"
     "pmemcheck gdb monitor commands:\n"
     "  print_stats\n"
     "        prints the summary\n"
     "  print_pmem_regions \n"
     "        prints the registered persistent memory regions\n"
     "\n");
}

/**
* \brief Gdb monitor command handler.
* \param[in] tid Id of the calling thread.
* \param[in] req Command request string.
* \return True if command is recognized, true otherwise.
*/
static Bool handle_gdb_monitor_command(ThreadId tid, HChar *req)
{
    HChar *wcmd;
    HChar s[VG_(strlen(req)) + 1];
    HChar *ssaveptr;

    VG_(strcpy)
    (s, req);

    wcmd = VG_(strtok_r)(s, " ", &ssaveptr);
    switch (VG_(keyword_id)("help print_stats print_pmem_regions",
                            wcmd, kwd_report_duplicated_matches))
    {
    case -2:
        return True;

    case -1:
        return False;

    case 0:
        print_monitor_help();
        return True;

    case 1:
        print_pmem_stats(True);
        return True;

    case 2:
    {
        VG_(gdb_printf)
        ("Registered persistent memory regions:\n");
        struct pmem_st *tmp;
        for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
        {
            for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
            {
                tmp = pmem.array.pmem_stores + i;
                VG_(gdb_printf)
                ("\tAddress: 0x%lx \tsize: %llu\n",
                 tmp->addr, tmp->size);
            }
        }
        VG_(OSetGen_ResetIter)
        (pmem.pmem_stores);
        while ((tmp = VG_(OSetGen_Next)(pmem.pmem_stores)) != NULL)
        {
            VG_(gdb_printf)
            ("\tAddress: 0x%lx \tsize: %llu\n",
             tmp->addr, tmp->size);
        }

        return True;
    }

    default:
        tl_assert(0);
        return False;
    }
}

/**
* \brief The main instrumentation function - the heart of the tool.
*
* The translated client code is passed into this function, where appropriate
* instrumentation is made. All uninteresting operations are copied straight
* to the returned IRSB. The only interesting operations are stores, which are
* instrumented for further analysis.
* \param[in] closure Valgrind closure - unused.
* \param[in] bb The IR superblock provided by the core.
* \param[in] layout Vex quest layout - unused.
* \param[in] vge Vex quest extents - unused.
* \param[in] archinfo_host Vex architecture info - unused.
* \param[in] gWordTy Guest word type.
* \param[in] hWordTy Host word type.
* \return The modified IR superblock.
*/

static IRSB *
pmc_instrument(VgCallbackClosure *closure,
               IRSB *bb,
               const VexGuestLayout *layout,
               const VexGuestExtents *vge,
               const VexArchInfo *archinfo_host,
               IRType gWordTy, IRType hWordTy)
{
    Int i;
    IRSB *sbOut;
    IRTypeEnv *tyenv = bb->tyenv;

    if (gWordTy != hWordTy)
    {

        VG_(tool_panic)
        ("host/guest word size mismatch");
    }

    sbOut = deepCopyIRSBExceptStmts(bb);

    i = 0;
    while (i < bb->stmts_used && bb->stmts[i]->tag != Ist_IMark)
    {
        addStmtToIRSB(sbOut, bb->stmts[i]);
        ++i;
    }

    IRDirty *di = unsafeIRDirty_0_N(0, "add_one_SB_entered",
                                    VG_(fnptr_to_fnentry)(&add_one_SB_entered), mkIRExprVec_0());
    addStmtToIRSB(sbOut, IRStmt_Dirty(di));

    for (/*use current i*/; i < bb->stmts_used; i++)
    {
        IRStmt *st = bb->stmts[i];
        if (!st || st->tag == Ist_NoOp)
            continue;

        switch (st->tag)
        {
        case Ist_IMark:
        case Ist_AbiHint:
        case Ist_Put:
        case Ist_PutI:
        case Ist_LoadG:
        case Ist_WrTmp:
        case Ist_Exit:
        case Ist_Dirty:

            addStmtToIRSB(sbOut, st);
            break;

        case Ist_Flush:
        {

            addStmtToIRSB(sbOut, st);
            if (LIKELY(pmem.automatic_isa_rec))
            {
                IRExpr *addr = st->Ist.Flush.addr;
                IRType type = typeOfIRExpr(tyenv, addr);
                tl_assert(type != Ity_INVALID);
                add_flush_event(sbOut, st->Ist.Flush.addr);

                if (st->Ist.Flush.fk == Ifk_flush)
                    if (!pmem.weak_clflush)
                        add_simple_event(sbOut, do_fence, "do_fence");
            }

            break;
        }

        case Ist_MBE:
        {
            addStmtToIRSB(sbOut, st);
            if (LIKELY(pmem.automatic_isa_rec))
            {
                switch (st->Ist.MBE.event)
                {
                case Imbe_Fence:
                case Imbe_SFence:
                    add_simple_event(sbOut, do_fence, "do_fence");
                    break;
                default:
                    break;
                }
            }
            break;
        }

        case Ist_Store:
        {
            addStmtToIRSB(sbOut, st);
            IRExpr *data = st->Ist.Store.data;
            IRType type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            add_event_dw(sbOut, st->Ist.Store.addr, sizeofIRType(type),
                         data);
            break;
        }

        case Ist_StoreG:
        {
            addStmtToIRSB(sbOut, st);
            IRStoreG *sg = st->Ist.StoreG.details;
            IRExpr *data = sg->data;
            IRType type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            add_event_dw_guarded(sbOut, sg->addr, sizeofIRType(type),
                                 sg->guard, data);
            break;
        }

        case Ist_CAS:
        {
            Int dataSize;
            IRType dataTy;
            IRCAS *cas = st->Ist.CAS.details;
            tl_assert(cas->addr != NULL);
            tl_assert(cas->dataLo != NULL);
            dataTy = typeOfIRExpr(tyenv, cas->dataLo);
            dataSize = sizeofIRType(dataTy);

            addStmtToIRSB(sbOut, st);

            IROp opCasCmpEQ;
            IROp opOr;
            IROp opXor;
            IRAtom *zero = NULL;
            IRType loType = typeOfIRExpr(tyenv, cas->expdLo);
            switch (loType)
            {
            case Ity_I8:
                opCasCmpEQ = Iop_CasCmpEQ8;
                opOr = Iop_Or8;
                opXor = Iop_Xor8;
                break;
            case Ity_I16:
                opCasCmpEQ = Iop_CasCmpEQ16;
                opOr = Iop_Or16;
                opXor = Iop_Xor16;
                break;
            case Ity_I32:
                opCasCmpEQ = Iop_CasCmpEQ32;
                opOr = Iop_Or32;
                opXor = Iop_Xor32;
                break;
            case Ity_I64:
                opCasCmpEQ = Iop_CasCmpEQ64;
                opOr = Iop_Or64;
                opXor = Iop_Xor64;
                break;
            default:
                tl_assert(0);
            }

            if (cas->dataHi != NULL)
            {
                IRAtom *xHi = NULL;
                IRAtom *xLo = NULL;
                IRAtom *xHL = NULL;
                xHi = make_expr(sbOut, loType, binop(opXor, cas->expdHi, mkexpr(cas->oldHi)));
                xLo = make_expr(sbOut, loType, binop(opXor, cas->expdLo, mkexpr(cas->oldLo)));
                xHL = make_expr(sbOut, loType, binop(opOr, xHi, xLo));
                IRAtom *guard = make_expr(sbOut, Ity_I1,
                                          binop(opCasCmpEQ, xHL, zero));

                add_event_dw_guarded(sbOut, cas->addr, dataSize, guard,
                                     cas->dataLo);
                add_event_dw_guarded(sbOut, cas->addr + dataSize,
                                     dataSize, guard, cas->dataHi);
            }
            else
            {
                IRAtom *guard = make_expr(sbOut, Ity_I1, binop(opCasCmpEQ, cas->expdLo, mkexpr(cas->oldLo)));

                add_event_dw_guarded(sbOut, cas->addr, dataSize, guard,
                                     cas->dataLo);
            }
            break;
        }

        case Ist_LLSC:
        {
            addStmtToIRSB(sbOut, st);
            IRType dataTy;
            if (st->Ist.LLSC.storedata != NULL)
            {
                dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
                add_event_dw(sbOut, st->Ist.LLSC.addr, sizeofIRType(dataTy), st->Ist.LLSC.storedata);
            }
            break;
        }

        default:
            ppIRStmt(st);
            tl_assert(0);
        }
    }

    return sbOut;
}

/**
* \brief Client mechanism handler.
* \param[in] tid Id of the calling thread.
* \param[in] arg Arguments passed in the request, 0-th is the request name.
* \param[in,out] ret Return value passed to the client.
* \return True if the request has been handled, false otherwise.
*/
static Bool
pmc_handle_client_request(ThreadId tid, UWord *arg, UWord *ret)
{
    if (!VG_IS_TOOL_USERREQ('P', 'C', arg[0]) && VG_USERREQ__PMC_REGISTER_PMEM_MAPPING != arg[0] && VG_USERREQ__PMC_REGISTER_PMEM_FILE != arg[0] && VG_USERREQ__PMC_REMOVE_PMEM_MAPPING != arg[0] && VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING != arg[0] && VG_USERREQ__PMC_DO_FLUSH != arg[0] && VG_USERREQ__PMC_DO_FENCE != arg[0] && VG_USERREQ__PMC_WRITE_STATS != arg[0] && VG_USERREQ__GDB_MONITOR_COMMAND != arg[0] && VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS != arg[0] && VG_USERREQ__PMC_EMIT_LOG != arg[0] && VG_USERREQ__PMC_START_TX != arg[0] && VG_USERREQ__PMC_START_TX_N != arg[0] && VG_USERREQ__PMC_END_TX != arg[0] && VG_USERREQ__PMC_END_TX_N != arg[0] && VG_USERREQ__PMC_ADD_TO_TX != arg[0] && VG_USERREQ__PMC_ADD_TO_TX_N != arg[0] && VG_USERREQ__PMC_REMOVE_FROM_TX != arg[0] && VG_USERREQ__PMC_REMOVE_FROM_TX_N != arg[0] && VG_USERREQ__PMC_ADD_THREAD_TO_TX_N != arg[0] && VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N != arg[0] && VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE != arg[0] && VG_USERREQ__PMC_RESERVED1 != arg[0] && VG_USERREQ__PMC_RESERVED2 != arg[0] && VG_USERREQ__PMC_RESERVED3 != arg[0] && VG_USERREQ__PMC_RESERVED4 != arg[0] && VG_USERREQ__PMC_RESERVED5 != arg[0] && VG_USERREQ__PMC_RESERVED6 != arg[0] && VG_USERREQ__PMC_RESERVED7 != arg[0] && VG_USERREQ__PMC_RESERVED8 != arg[0] && VG_USERREQ__PMC_RESERVED9 != arg[0] && VG_USERREQ__PMC_RESERVED10 != arg[0] && VG_USERREQ__PMC_FENCE_MARK != arg[0] && VG_USERREQ__PMC_EPOCH_BEGIN != arg[0] && VG_USERREQ__PMC_EPOCH_END != arg[0] && VG_USERREQ__PMC_NEW_STRAND != arg[0] && VG_USERREQ__PMC_ORDER_GUARANTEE != arg[0])
        return False;

    switch (arg[0])
    {
    case VG_USERREQ__PMC_REGISTER_PMEM_MAPPING:
    {
        struct pmem_st temp_info = {0};
        temp_info.addr = arg[1];
        temp_info.size = arg[2];

        Addr temp_info_max = temp_info.addr + temp_info.size;
        if (pmem.min_add_pmem_mappings == -1 && pmem.max_add_pmem_mappings == -1)
        {
            pmem.min_add_pmem_mappings = temp_info.addr;
            pmem.max_add_pmem_mappings = temp_info_max;
        }
        if (pmem.min_add_pmem_mappings > temp_info.addr)
            pmem.min_add_pmem_mappings = temp_info.addr;
        if (pmem.max_add_pmem_mappings < temp_info_max)
        {
            pmem.max_add_pmem_mappings = temp_info_max;
        }

        add_region(&temp_info, pmem.pmem_mappings);
        break;
    }

    case VG_USERREQ__PMC_REMOVE_PMEM_MAPPING:
    {
        struct pmem_st temp_info = {0};
        temp_info.addr = arg[1];
        temp_info.size = arg[2];

        remove_region_mapping(&temp_info, pmem.pmem_mappings);
        struct pmem_st *pmem_tmp = NULL;
        struct pmem_st *before_pmem_tmp = NULL;
        if (temp_info.addr == pmem.min_add_pmem_mappings)
        {
            VG_(OSetGen_ResetIter)
            (pmem.pmem_mappings);
            pmem_tmp = VG_(OSetGen_Next)(pmem.pmem_mappings);
            if (pmem_tmp == NULL)
                pmem.max_add_pmem_mappings = pmem.min_add_pmem_mappings = -1;
            else
                pmem.min_add_pmem_mappings = pmem_tmp->addr;
        }
        if (temp_info.addr + temp_info.size == pmem.max_add_pmem_mappings)
        {
            VG_(OSetGen_ResetIter)
            (pmem.pmem_mappings);
            while ((pmem_tmp = VG_(OSetGen_Next)(pmem.pmem_mappings)) != NULL)
            {
                before_pmem_tmp = pmem_tmp;
            }
            if (before_pmem_tmp == NULL)
                pmem.max_add_pmem_mappings = pmem.min_add_pmem_mappings = -1;
            else
                pmem.max_add_pmem_mappings = before_pmem_tmp->addr + before_pmem_tmp->size;
        }

        break;
    }

    case VG_USERREQ__PMC_REGISTER_PMEM_FILE:
    {
        *ret = 1;
        Int fd = (Int)arg[1];
        if (fd >= 0)
            *ret = register_new_file(fd, arg[2], arg[3], arg[4]);
        break;
    }

    case VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING:
    {
        struct pmem_st temp_info = {0};
        temp_info.addr = arg[1];
        temp_info.size = arg[2];

        *ret = is_in_mapping_set(&temp_info, pmem.pmem_mappings);
        break;
    }

    case VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS:
    {
        print_persistent_mappings();
        break;
    }

    case VG_USERREQ__PMC_DO_FLUSH:
    {
        do_flush(arg[1], arg[2]);
        break;
    }

    case VG_USERREQ__PMC_DO_FENCE:
    {
        do_fence();
        break;
    }

    case VG_USERREQ__PMC_WRITE_STATS:
    {
        print_pmem_stats(True);
        break;
    }

    case VG_USERREQ__GDB_MONITOR_COMMAND:
    {
        Bool handled = handle_gdb_monitor_command(tid, (HChar *)arg[1]);
        if (handled)
            *ret = 0;
        else
            *ret = 1;
        return handled;
    }

    case VG_USERREQ__PMC_EMIT_LOG:
    {
        if (pmem.log_stores)
        {
            VG_(emit)
            ("|%s", (char *)arg[1]);
        }
        break;
    }

    case VG_USERREQ__PMC_SET_CLEAN:
    {

        struct pmem_st temp_info = {0};
        temp_info.addr = arg[1];
        temp_info.size = arg[2];

        struct pmem_st *modified_entry = NULL;
        struct pmem_st *region = &temp_info;
        UWord old_index;
        for (int s_index = 0; s_index <= pmem.array.snippet_info_index; s_index++)
        {
            if (cmp_with_arr_minandmax(&temp_info, s_index) != -1)
            {
                for (int i = pmem.array.snippet_info[s_index].start_index; i < pmem.array.snippet_info[s_index].index; i++)
                {

                    modified_entry = pmem.array.pmem_stores + i;
                    if (modified_entry->is_delete == True)
                        continue;
                    if (cmp_pmem_st(region, modified_entry) == 0)
                    {
                        SizeT region_max_addr = region->addr + region->size;

                        SizeT mod_entry_max_addr = modified_entry->addr + modified_entry->size;
                        if ((modified_entry->addr > region->addr) && (mod_entry_max_addr <
                                                                      region_max_addr))
                        {

                            modified_entry->is_delete = True;
                        }
                        else if ((modified_entry->addr < region->addr) &&
                                 (mod_entry_max_addr > region_max_addr))
                        {

                            modified_entry->size = region->addr - modified_entry->addr;

                            struct pmem_st *new_region;
                            old_index = pmem.array.snippet_info[pmem.array.snippet_info_index].index;
                            if (pmem.array.snippet_info[pmem.array.snippet_info_index].index >= MAX_ARRAY_NUM)
                            {
                                new_region = VG_(OSetGen_AllocNode)(pmem.pmem_stores,
                                                                    sizeof(struct pmem_st));
                                new_region->index = db_pmem_store_index;
                                db_pmem_store_index++;
                            }
                            else
                            {
                                new_region = pmem.array.pmem_stores + pmem.array.snippet_info[pmem.array.snippet_info_index].index;
                                pmem.array.snippet_info[pmem.array.snippet_info_index].index++;
                            }
                            new_region->addr = region_max_addr;
                            new_region->size = mod_entry_max_addr - new_region->addr;
                            if (old_index >= MAX_ARRAY_NUM)
                            {
                                if (pmem.tree_reorganization)
                                    add_and_merge_store(new_region);
                                else
                                    VG_(OSetGen_Insert)
                                (pmem.pmem_stores, new_region);
                            }
                        }
                        else if ((modified_entry->addr >= region->addr) &&
                                 (mod_entry_max_addr > region_max_addr))
                        {

                            modified_entry->size -= region_max_addr - modified_entry->addr;
                            modified_entry->addr = region_max_addr;
                        }
                        else if ((mod_entry_max_addr <= region_max_addr) &&
                                 (region->addr > modified_entry->addr))
                        {

                            modified_entry->size = region->addr - modified_entry->addr;
                        }
                        else
                        {

                            modified_entry->is_delete = True;
                        }
                    }
                }
            }
        }

        remove_region(&temp_info, pmem.pmem_stores);

        break;
    }

    case VG_USERREQ__PMC_START_TX:
    {

        register_new_tx(VG_(get_running_tid)());
        if (pmem.epoch_durability_fence && !is_nest_trans(VG_(get_running_tid)()))
            epoch_begin();
        break;
    }

    case VG_USERREQ__PMC_START_TX_N:
    {
        register_new_tx(arg[1]);
        if (pmem.epoch_durability_fence && !is_nest_trans(arg[1]))
            epoch_begin();
        break;
    }

    case VG_USERREQ__PMC_END_TX:
    {
        if (pmem.epoch_durability_fence && !is_nest_trans(VG_(get_running_tid)()))
        {
            *ret = remove_tx(VG_(get_running_tid)());
            mark_epoch_fence();
            epoch_end(VG_(get_running_tid)());
        }
        else
        {
            *ret = remove_tx(VG_(get_running_tid)());
        }
        break;
    }

    case VG_USERREQ__PMC_END_TX_N:
    {
        if (pmem.epoch_durability_fence && !is_nest_trans(arg[1]))
        {
            *ret = remove_tx(arg[1]);
            mark_epoch_fence();
            epoch_end(arg[1]);
        }
        else
        {
            *ret = remove_tx(arg[1]);
        }
        break;
    }

    case VG_USERREQ__PMC_ADD_TO_TX:
    {

        *ret = add_obj_to_tx(VG_(get_running_tid)(), arg[1], arg[2]);

        break;
    }

    case VG_USERREQ__PMC_ADD_TO_TX_N:
    {
        *ret = add_obj_to_tx(arg[1], arg[2], arg[3]);
        break;
    }

    case VG_USERREQ__PMC_REMOVE_FROM_TX:
    {

        *ret = remove_obj_from_tx(VG_(get_running_tid)(), arg[1], arg[2]);

        break;
    }

    case VG_USERREQ__PMC_REMOVE_FROM_TX_N:
    {
        *ret = remove_obj_from_tx(arg[1], arg[2], arg[3]);
        break;
    }

    case VG_USERREQ__PMC_ADD_THREAD_TO_TX_N:
    {
        *ret = remove_obj_from_tx(arg[1], arg[2], arg[3]);
        break;
    }

    case VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N:
    {
        *ret = remove_obj_from_tx(arg[1], arg[2], arg[3]);
        break;
    }

    case VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE:
    {

        struct pmem_st temp_info = {0};
        temp_info.addr = arg[1];
        temp_info.size = arg[2];

        add_to_global_excludes(&temp_info);

        break;
    }
    case VG_USERREQ__PMC_FENCE_MARK:
    {
        mark_epoch_fence();
        break;
    }

    case VG_USERREQ__PMC_EPOCH_BEGIN:
    {
        if (pmem.epoch_durability_fence)
            epoch_begin();
        break;
    }

    case VG_USERREQ__PMC_EPOCH_END:
    {
        if (pmem.epoch_durability_fence)
            epoch_end(VG_(get_running_tid)());
        break;
    }
    case VG_USERREQ__PMC_RESERVED1:
    {

        break;
    }
    case VG_USERREQ__PMC_NEW_STRAND:
    {

        *(curr_strand_pmem->p_pmem) = pmem;
        struct strand_pmem *new_node = (struct strand_pmem *)VG_(malloc)("pmc.strand.cpci.1", sizeof(struct strand_pmem));
        new_node->p_pmem = (struct pmem_ops *)VG_(malloc)("pmc.main.cpci.8", sizeof(struct pmem_ops));
        new_node->strand_id = ++curr_strand_pmem->strand_id;
        new_node->pp = curr_strand_pmem;
        new_node->pn = NULL;
        curr_strand_pmem->pn = new_node;

        *(new_node->p_pmem) = pmem;

#ifdef InstrDisbu
        new_node->p_pmem->memoryNum = new_node->p_pmem->flushNum = new_node->p_pmem->fenceNum = 0;
#endif
#ifdef ALLFLUSH
        new_node->p_pmem->partflushNum = new_node->p_pmem->allflushNum = 0;
#endif
#ifdef MemInstInGroup
        new_node->p_pmem->allflushgroupNum = new_node->p_pmem->memInstNum = 0
#endif
#ifdef MemDistance
                                                                            new_node->p_pmem->maxDistance = new_node->p_pmem->memDistance = 0;
#endif

        new_node->p_pmem->epoch = False;
        new_node->p_pmem->epoch_fence = 0;
        new_node->p_pmem->not_pmem_num = 0;
        if (pmem.print_bug_detail)
            new_node->p_pmem->debug_memory_merge = NULL;

        if (new_node->p_pmem->tree_reorganization)
        {
            new_node->p_pmem->pmem_stores = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_st,
                                                                          VG_(malloc), "pmc.main.cpci.1", VG_(free), sizeof(struct pmem_st) * 1000000, sizeof(struct pmem_st));
        }
        else
        {
            new_node->p_pmem->pmem_stores = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_own_st,
                                                                          VG_(malloc), "pmc.main.cpci.1", VG_(free), sizeof(struct pmem_st) * 1000000, sizeof(struct pmem_st));
        }

        new_node->p_pmem->array.pmem_stores = VG_(malloc)("pmc.main.cpci.7",
                                                          MAX_ARRAY_NUM * sizeof(struct pmem_st));
        new_node->p_pmem->array.snippet_info_index = 0;
        new_node->p_pmem->array.snippet_info = VG_(malloc)("pmc.main.cpci.8",
                                                           MAX_ARRAY_NUM * sizeof(struct arr_snippet));
        new_node->p_pmem->array.snippet_info[0].min_add = new_node->p_pmem->array.snippet_info[0].max_add = -1;
        new_node->p_pmem->array.snippet_info[0].flushed_num = NO_FLUSHED;
        new_node->p_pmem->array.snippet_info[0].index = new_node->p_pmem->array.snippet_info[0].start_index = 0;

        new_node->p_pmem->flush_align_size = read_cache_line_size();

        pmem = *(new_node->p_pmem);
        curr_strand_pmem = new_node;

        break;
    }
    case VG_USERREQ__PMC_ORDER_GUARANTEE:
    {

        struct pmem_st_order *newA = VG_(OSetGen_AllocNode)(pmem.pmem_time_records,
                                                            sizeof(struct pmem_st_order));
        newA->addr = arg[1];
        newA->size = arg[2];
        newA->index = pmem.global_time_records_index;
        pmem.global_time_records_index++;
        struct pmem_st_order *newB = VG_(OSetGen_AllocNode)(pmem.pmem_time_records,
                                                            sizeof(struct pmem_st_order));
        newB->addr = arg[3];
        newB->size = arg[4];
        newB->index = pmem.global_time_records_index;
        pmem.global_time_records_index++;

        VG_(OSetGen_Insert)
        (pmem.pmem_time_records, newA);
        VG_(OSetGen_Insert)
        (pmem.pmem_time_records, newB);
        struct order_record *newRecord = VG_(OSetGen_AllocNode)(pmem.pmem_order_records,
                                                                sizeof(struct order_record));
        newRecord->a = newA;
        newRecord->b = newB;
        newRecord->index = pmem.global_time_records_index;
        pmem.global_time_records_index++;
        VG_(OSetGen_Insert)
        (pmem.pmem_order_records, newRecord);

        break;
    }
    case VG_USERREQ__PMC_RESERVED2:
    case VG_USERREQ__PMC_RESERVED3:
    case VG_USERREQ__PMC_RESERVED4:
    case VG_USERREQ__PMC_RESERVED5:
    case VG_USERREQ__PMC_RESERVED6:
    case VG_USERREQ__PMC_RESERVED7:
    case VG_USERREQ__PMC_RESERVED8:
    case VG_USERREQ__PMC_RESERVED9:
    case VG_USERREQ__PMC_RESERVED10:
    {
        VG_(message)
        (
            Vg_UserMsg,
            "Warning: deprecated pmemcheck client request code 0x%llx\n",
            (ULong)arg[0]);
        return False;
    }

    default:
        VG_(message)
        (
            Vg_UserMsg,
            "Warning: unknown pmemcheck client request code 0x%llx\n",
            (ULong)arg[0]);
        return False;
    }

    return True;
}

/**
* \brief Handle tool command line arguments.
* \param[in] arg Tool command line arguments.
* \return True if the parameter is recognized, false otherwise.
*/
static Bool
pmc_process_cmd_line_option(const HChar *arg)
{
    if VG_BOOL_CLO (arg, "--mult-stores", pmem.track_multiple_stores)
    {
    }
    else if VG_BINT_CLO (arg, "--indiff", pmem.store_sb_indiff, 0, UINT_MAX)
    {
    }
    else if VG_BOOL_CLO (arg, "--log-stores", pmem.log_stores)
    {
    }
    else if VG_BOOL_CLO (arg, "--log-stores-stacktraces", pmem.store_traces)
    {
    }
    else if VG_BINT_CLO (arg, "--log-stores-stacktraces-depth",
                         pmem.store_traces_depth, 1, UINT_MAX)
    {
    }
    else if VG_BOOL_CLO (arg, "--print-summary", pmem.print_summary)
    {
    }
    else if VG_BOOL_CLO (arg, "--flush-check", pmem.check_flush)
    {
    }
    else if VG_BOOL_CLO (arg, "--flush-align", pmem.force_flush_align)
    {
    }
    else if VG_BOOL_CLO (arg, "--tx-only", pmem.transactions_only)
    {
    }
    else if VG_BOOL_CLO (arg, "--isa-rec", pmem.automatic_isa_rec)
    {
    }
    else if VG_BOOL_CLO (arg, "--error-summary", pmem.error_summary)
    {
    }
    else if VG_BOOL_CLO (arg, "--expect-fence-after-clflush",
                         pmem.weak_clflush)
    {
    }
    else if VG_BOOL_CLO (arg, "--tree-reorganization", pmem.tree_reorganization)
    {
    }
    else if VG_BOOL_CLO (arg, "--redundant-logging", pmem.redundant_logging)
    {
    }
    else if VG_BOOL_CLO (arg, "--epoch-durability-fence", pmem.epoch_durability_fence)
    {
    }

    else if VG_BOOL_CLO (arg, "--print-debug-detail", pmem.print_bug_detail)
    {
    }
    else if VG_BOOL_CLO (arg, "--order-guarantee", pmem.order_guarantee)
    {
    }
    else if VG_BOOL_CLO (arg, "--lack-ordering-in-strand", pmem.lack_ordering_trand)
    {
    }
    else
        return False;

    return True;
}

/**
* \brief Post command line options initialization.
*/
static void
pmc_post_clo_init(void)
{

#ifdef InstrDisbu
    pmem.memoryNum = pmem.flushNum = pmem.fenceNum = 0;
#endif
#ifdef ALLFLUSH
    pmem.partflushNum = pmem.allflushNum = 0;
#endif
#ifdef MemInstInGroup
    pmem.allflushgroupNum = pmem.memInstNum = 0;
#endif
#ifdef MemDistance
    pmem.maxDistance = pmem.memDistance = 0;
#endif

    pmem.epoch = False;
    pmem.epoch_fence = 0;

#ifdef GETTIME
    time_flush = time_fence = time_memory = 0;
#endif
    pmem.not_pmem_num = 0;
    if (pmem.print_bug_detail)
    {
        pmem.debug_memory_merge = NULL;
    }
    db_pmem_store_index = 0;
    if (pmem.tree_reorganization)
    {
        pmem.pmem_stores = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_st,
                                                         VG_(malloc), "pmc.main.cpci.1", VG_(free), sizeof(struct pmem_st) * 1000000, sizeof(struct pmem_st));
    }
    else
    {
        pmem.pmem_stores = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_own_st,
                                                         VG_(malloc), "pmc.main.cpci.1", VG_(free), sizeof(struct pmem_st) * 1000000, sizeof(struct pmem_st));
    }
    if (pmem.order_guarantee)
    {
        pmem.pmem_time_records = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_own_st,
                                                               VG_(malloc), "pmc.main.time_records", VG_(free), sizeof(struct pmem_st_order) * 1000, sizeof(struct pmem_st_order));
        pmem.global_time_stamp = 0;
        pmem.global_time_records_index = 0;
        pmem.pmem_order_records = VG_(OSetGen_Create_With_Pool)(/*keyOff*/ 0, cmp_pmem_own_st,
                                                                VG_(malloc), "pmc.main.pmem_order_records", VG_(free), sizeof(struct order_record) * 1000, sizeof(struct order_record));
    }
    pmem.lack_ordering_trand_reg = 0;

    pmem.min_add_pmem_store = pmem.max_add_pmem_store = -1;
    pmem.min_add_pmem_mappings = pmem.max_add_pmem_mappings = -1;

    pmem.array.pmem_stores = VG_(malloc)("pmc.main.cpci.7",
                                         MAX_ARRAY_NUM * sizeof(struct pmem_st));
    pmem.array.snippet_info_index = 0;
    pmem.array.snippet_info = VG_(malloc)("pmc.main.cpci.8",
                                          MAX_ARRAY_NUM * sizeof(struct arr_snippet));
    pmem.array.snippet_info[0].min_add = pmem.array.snippet_info[0].max_add = -1;
    pmem.array.snippet_info[0].flushed_num = NO_FLUSHED;
    pmem.array.snippet_info[0].index = pmem.array.snippet_info[0].start_index = 0;

    if (pmem.track_multiple_stores)
        pmem.multiple_stores = VG_(malloc)("pmc.main.cpci.2",
                                           MAX_MULT_OVERWRITES * sizeof(struct pmem_st *));

    pmem.redundant_flushes = VG_(malloc)("pmc.main.cpci.3",
                                         MAX_FLUSH_ERROR_EVENTS * sizeof(struct pmem_st *));

    pmem.pmem_mappings = VG_(OSetGen_Create)(/*keyOff*/ 0, cmp_pmem_st,
                                             VG_(malloc), "pmc.main.cpci.4", VG_(free));

    pmem.superfluous_flushes = VG_(malloc)("pmc.main.cpci.6",
                                           MAX_FLUSH_ERROR_EVENTS * sizeof(struct pmem_st *));

    pmem.redundant_fence = VG_(malloc)("pmc.main.cpci.7",
                                       MAX_REDUNDANT_FENCE * sizeof(ExeContext *));

    pmem.flush_align_size = read_cache_line_size();

    init_transactions(pmem.transactions_only, pmem.tree_reorganization, pmem.redundant_logging, pmem.print_bug_detail);
    struct strand_pmem *new_node = (struct strand_pmem *)VG_(malloc)("pmc.main.cpci.8", sizeof(struct strand_pmem));
    new_node->p_pmem = (struct pmem_ops *)VG_(malloc)("pmc.main.cpci.8", sizeof(struct pmem_ops));

    new_node->pp = NULL;
    new_node->pn = NULL;
    new_node->strand_id = 0;

    curr_strand_pmem = new_node;
    if (pmem.log_stores)
        VG_(emit)
    ("START");
}

/**
* \brief Print usage.
*/
static void
pmc_print_usage(void)
{
    VG_(printf)
    (
        "    --indiff=<uint>                        multiple store indifference\n"
        "                                           default [0 SBlocks]\n"
        "    --mult-stores=<yes|no>                 track multiple stores to the same\n"
        "                                           address default [no]\n"
        "    --print-summary=<yes|no>               print summary on program exit\n"
        "                                           default [yes]\n"
        "    --flush-check=<yes|no>                 register multiple flushes of stores\n"
        "                                           default [no]\n"
        "    --flush-align=<yes|no>                 force flush alignment to native cache\n"
        "                                           line size default [no]\n"
        "    --error-summary=<yes|no>               turn on error summary message\n"
        "                                           default [yes]\n"
        "    --tree-reorganization=<yes|no>         open tree reorganization\n"
        "                                           default [no]\n"
        "    --redundant-logging=<yes|no>           open detection of redundant logging\n"
        "                                           default [no]\n"
        "    --epoch-durability-fence=<yes|no>       open detection of redundant fence and lack\n"
        "                                           durability in epoch default [no]\n"
        "    --print-debug-detail=<yes|no>          print detail information for detected bugs\n"
        "                                           default [no]\n"
        "    --order-guarantee=<yes|no>             track persistent order\n"
        "                                           default [no]\n"
        "    --lack-ordering-in-strand=<yes|no>     detect violating order guarantee because of multi-strands (must open --order-guarantee=yes ) \n"
        "                                           default [no]\n"

    );
}

/**
* \brief Print debug usage.
*/
static void
pmc_print_debug_usage(void)
{
    VG_(printf)
    (
        "    (none)\n");
}

/**
 * \brief Function called on program exit.
 */
static void
pmc_fini(Int exitcode)
{
    if (pmem.order_guarantee)
    {
        check_pmem_order();
    }
#ifdef InstrDisbu
    VG_(umsg)
    ("memory instr = %lu flush instr = %lu fence intr = %lu\n", pmem.memoryNum, pmem.flushNum, pmem.fenceNum);
#endif
#ifdef ALLFLUSH
    VG_(umsg)
    ("allFlush %.2f, partflush %.2f, total number %lu\n", (float)pmem.allflushNum / (pmem.allflushNum + pmem.partflushNum), (float)pmem.partflushNum / (pmem.allflushNum + pmem.partflushNum), pmem.allflushNum + pmem.partflushNum);
#endif

#ifdef MemInstInGroup
    VG_(umsg)
    ("memory instruction number per all flush group %.2f\n", (float)pmem.memInstNum / pmem.allflushgroupNum);
#endif
#ifdef MemDistance
    VG_(umsg)
    ("max_distance %lu\n", pmem.maxDistance);
#endif

    if (pmem.array.snippet_info_index >= MAX_ARRAY_NUM)
        VG_(umsg)
        ("====================================pmem.array.snippet_info_index>=MAX_ARRAY_NUM============================================\n");

    if (pmem.log_stores)
        VG_(emit)
    ("|STOP\n");

    if (pmem.print_summary)
    {
        print_pmem_stats(False);
#ifdef GETTIME
        VG_(umsg)
        ("****************time flush=%llu ms***************\n", time_flush / 1000);
        VG_(umsg)
        ("****************time fence=%llu ms***************\n", time_fence / 1000);
        VG_(umsg)
        ("****************time memory=%llu ms***************\n", time_memory / 1000);
#endif
    }
}

/**
* \brief Pre command line options initialization.
*/
static void
pmc_pre_clo_init(void)
{
    VG_(details_name)
    ("PMDebugger");
    VG_(details_version)
    ("1.0");
    VG_(details_description)
    ("fast, flexible and comprehensive persistent store checker");
    VG_(details_copyright_author)
    ("");
    VG_(details_bug_reports_to)
    ("");

    VG_(details_avg_translation_sizeB)
    (275);

    VG_(basic_tool_funcs)
    (pmc_post_clo_init, pmc_instrument, pmc_fini);

    VG_(needs_command_line_options)
    (pmc_process_cmd_line_option,
     pmc_print_usage, pmc_print_debug_usage);

    VG_(needs_client_requests)
    (pmc_handle_client_request);

    tl_assert(VG_WORDSIZE == 8);
    tl_assert(sizeof(void *) == 8);
    tl_assert(sizeof(Addr) == 8);
    tl_assert(sizeof(UWord) == 8);
    tl_assert(sizeof(Word) == 8);

    pmem.print_summary = True;
    pmem.store_traces_depth = 1;
    pmem.automatic_isa_rec = True;
    pmem.error_summary = True;
}

VG_DETERMINE_INTERFACE_VERSION(pmc_pre_clo_init)
