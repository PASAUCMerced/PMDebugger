#ifndef PMC_INCLUDE_H
#define PMC_INCLUDE_H
/** Single store to memory. */
struct arr_snippet
{
    UWord index;
    Addr min_add;
    Addr max_add;
    UWord start_index;
    enum flushed_state
    {
        NO_FLUSHED,
        PART_FLUSHED,
        ALL_FLUSHED,
    } flushed_num;
};
struct pmem_st
{

    UWord index;
    Bool is_delete;
    Addr addr;
    ULong size;
    ULong block_num;
    UWord value;
    ExeContext *context;
    enum store_state
    {
        STST_CLEAN,
        STST_DIRTY,
        STST_FLUSHED,
    } state;
#ifdef MemDistance
    UWord pmemDistance;
#endif
    Bool inEpoch;
};

struct pmem_st_order
{
    UWord index;
    UWord timestamp_begin;
    UWord timestamp_end;
    ExeContext *context;
    Addr addr;
    UWord size;
};

struct order_record
{
    UWord index;
    struct pmem_st_order *a;
    struct pmem_st_order *b;
};

UWord db_pmem_store_index;

Word cmp_pmem_node(const void *key, const void *elem);
Word cmp_pmem_own_st(const void *key, const void *elem);
Word cmp_pmem_st_order(const void *key, const void *elem);
void pmem_st_copy(struct pmem_st *copy, struct pmem_st *orig);
void init_snippet_info(void);

UWord is_in_mapping_set(const struct pmem_st *region, OSet *region_set);

void add_region(const struct pmem_st *region, OSet *region_set);

void remove_region(const struct pmem_st *region, OSet *region_set);
void remove_region_mapping(const struct pmem_st *region, OSet *region_set);

Word cmp_pmem_st(const void *key, const void *elem);

void add_warning_event(struct pmem_st **event_register, UWord *nevents,
                       struct pmem_st *event, UWord limit, void (*err_msg)(UWord));

UWord check_overlap(const struct pmem_st *lhs, const struct pmem_st *rhs);

void init_transactions(Bool transactions_only, Bool tree_reorganization, Bool redundant_logging, Bool print_bug_detail);

void register_new_tx(UWord tx_id);

Bool is_nest_trans(UWord tx_id);
void check_pmem_order(void);
void epoch_begin(void);
void epoch_end(UWord txid);
void mark_epoch_fence(void);

UInt remove_tx(UWord tx_id);

UInt add_obj_to_tx(UWord tx_id, UWord base, UWord size);

UInt remove_obj_from_tx(UWord tx_id, UWord base, UWord size);

UInt add_thread_to_tx(UWord tx_id);

UInt remove_thread_from_tx(UWord tx_id);

void add_to_global_excludes(const struct pmem_st *region);

void handle_tx_store(const struct pmem_st *store);

void print_tx_summary(void);

UWord get_tx_all_err(void);

#endif
