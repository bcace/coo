#ifndef coo_state_h
#define coo_state_h

#define COO_MAX_ALLOCS 32
#define COO_MAX_TYPES  32


typedef struct CooState {
    struct CooAlloc *allocs[COO_MAX_ALLOCS];
    int allocs_count;
    struct CooType *types[COO_MAX_TYPES];
    int types_count;
    int update_id;
} CooState;

#endif
