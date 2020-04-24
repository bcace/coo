#ifndef coo_type_h
#define coo_type_h

#define COO_MAX_NAME    256
#define COO_MAX_VARS    64
#define COO_MAX_DIFFS   64
#define COO_MAX_CASTS   16


typedef void (*COO_CAST_FUNC)(void *src, void *dst);

typedef struct CooCast {
    struct CooType *to_type;
    COO_CAST_FUNC func;
} CooCast;

typedef enum {
    CDT_COPY, /* old variable, same type */
    CDT_CAST, /* old variable, different type */
    CDT_NULL, /* new variable or new array elements of old variable */
} CooDiffType;

typedef enum {
    IT_VAL, /* no indirection, value */
    IT_PTR, /* managed pointer */
    IT_FPTR, /* fixed pointer */
} CooIndirection;

typedef struct CooDiff {
    CooDiffType diff_type;
    struct CooType *to_type;
    CooCast *cast;
    int src_offset, dst_offset;
    int src_stride, dst_stride;
    CooIndirection indirection;
    int count;
} CooDiff;

typedef struct CooVar {
    char name[COO_MAX_NAME];
    struct CooType *type;
    int count; /* int var[count]; */
    CooIndirection indirection;
    int offset; /* derived, in bytes */
    int old_index;
} CooVar;

typedef struct CooType {
    char name[COO_MAX_NAME];
    CooVar vars[COO_MAX_VARS];
    int vars_count;
    CooVar new_vars[COO_MAX_VARS];
    int new_vars_count;
    CooCast casts[COO_MAX_CASTS];
    int casts_count;
    CooDiff diffs[COO_MAX_DIFFS];
    int diffs_count;
    int size, old_size;
    int alignment;
    int update_id;
    int is_fixed;
} CooType;

void _init_type(CooType *t, const char *name, int size);
void _update_type_layout(CooType *t, int update_id);

typedef struct CooTag {
    struct CooTag *prev, *next;
    union {
        int count; /* elements in the allocated batch */
        struct CooTag *redirect; /* only used between update begin and end */
    };
} CooTag;

typedef struct CooAlloc {
    CooType *type;
    CooIndirection indirection;
    CooTag *first;
    CooTag *old_first; /* only used between update begin and end */
} CooAlloc;

void _init_alloc(CooAlloc *a, CooType *type, CooIndirection indirection);
void _clear_alloc(CooAlloc *a);
void _update_alloc_data_layout(CooAlloc *a);
void _update_alloc_pointers(CooAlloc *a);
void _free_old_versions_of_data(CooAlloc *a);

#endif
