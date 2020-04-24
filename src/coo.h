#ifndef coo_h
#define coo_h


typedef struct CooState CooState;
typedef struct CooType CooType;
typedef struct CooAlloc CooAlloc;

CooState *coo_create_state();
void coo_destroy_state(CooState *s);

CooType *coo_create_type(CooState *s, const char *name);
void coo_remove_type(CooState *s, const char *name);
CooAlloc *coo_get_alloc(CooState *s, CooType *type);
CooAlloc *coo_get_ptr_alloc(CooState *s, CooType *type);
void coo_remove_alloc(CooState *s, CooType *type);
void coo_remove_ptr_alloc(CooState *s, CooType *type);
void coo_clear_alloc(CooAlloc *a);

void coo_begin_update(CooState *s);
void coo_end_update(CooState *s);
void *coo_update_pointer(void *ptr);

void coo_add_var(CooType *t, const char *v_name, CooType *v_type);
void coo_ins_var(CooType *t, const char *v_name, CooType *v_type, int v_index);
void coo_add_arr(CooType *t, const char *v_name, CooType *v_type, int v_count);
void coo_ins_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index);

void coo_add_ptr_var(CooType *t, const char *v_name, CooType *v_type);
void coo_ins_ptr_var(CooType *t, const char *v_name, CooType *v_type, int v_index);
void coo_add_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count);
void coo_ins_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index);

void coo_remove_var(CooType *type, const char *var_name);
void coo_resize_array(CooType *type, const char *var_name, int length);
void coo_move_var(CooType *type, const char *var_name, int position);
void coo_retype_var(CooType *type, const char *var_name, CooType *to_type);

void *coo_alloc(CooAlloc *a, int count);
void coo_free(CooAlloc *a, void *data);

extern CooType CooI8, CooI16, CooI32, CooI64, CooF32, CooF64;

#endif
