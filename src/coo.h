#ifndef coo_h
#define coo_h


typedef struct CooState CooState;
typedef struct CooType CooType;
typedef struct CooAlloc CooAlloc;

/* create and destroy coo state */
CooState *coo_create_state();
void coo_destroy_state(CooState *s);

/* create struct type */
CooType *coo_create_type(CooState *s, const char *name);

/* remove existing struct type and all allocs for that type */
void coo_remove_type(CooState *s, const char *name);

/* find or create alloc for a specific type (struct or primitive) */
CooAlloc *coo_get_alloc(CooState *s, CooType *type);

/* find or create alloc for pointers to specific type (struct or primitive) */
CooAlloc *coo_get_ptr_alloc(CooState *s, CooType *type);

/* remove existing alloc for a specific type (struct or primitive) */
void coo_remove_alloc(CooState *s, CooType *type);

/* remove existing alloc for pointers to specific type (struct or primitive) */
void coo_remove_ptr_alloc(CooState *s, CooType *type);

/* delete all data in an alloc */
void coo_clear_alloc(CooAlloc *a);

/* struct layout updating with pointer redirection */
void coo_begin_update(CooState *s);
void coo_end_update(CooState *s);
void *coo_update_pointer(void *ptr);

/* adding/inserting single/array value variables */
void coo_add_var(CooType *t, const char *v_name, CooType *v_type);
void coo_ins_var(CooType *t, const char *v_name, CooType *v_type, int v_index);
void coo_add_arr(CooType *t, const char *v_name, CooType *v_type, int v_count);
void coo_ins_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index);

/* adding/inserting single/array pointer variables */
void coo_add_ptr_var(CooType *t, const char *v_name, CooType *v_type);
void coo_ins_ptr_var(CooType *t, const char *v_name, CooType *v_type, int v_index);
void coo_add_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count);
void coo_ins_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index);

/* modifying variables */
void coo_remove_var(CooType *t, const char *var_name);
void coo_resize_array(CooType *t, const char *var_name, int length);
void coo_move_var(CooType *t, const char *var_name, int position);
void coo_retype_var(CooType *t, const char *var_name, CooType *to_type);

/* allocating and freeing data in an alloc */
void *coo_alloc(CooAlloc *a, int count);
void coo_free(CooAlloc *a, void *data);

/* primitive types */
extern CooType CooI8, CooI16, CooI32, CooI64, CooF32, CooF64;

#endif
