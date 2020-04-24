#include "layout.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>


static CooTag *_data_to_tag(void *data) {
    return (CooTag *)data - 1;
}

static void *_tag_to_data(CooTag *tag) {
    return (void *)(tag + 1);
}

static void *_update_pointer(void *ptr) {
    return ptr ? _tag_to_data(_data_to_tag(ptr)->redirect) : 0;
}

void *coo_update_pointer(void *ptr) {
    return _update_pointer(ptr);
}

void _init_type(CooType *t, const char *name, int size) {
    assert(strlen(name) < COO_MAX_NAME);
    assert(size >= 0);
    strcpy_s(t->name, COO_MAX_NAME, name);
    t->vars_count = 0;
    t->new_vars_count = 0;
    t->casts_count = 0;
    t->diffs_count = 0;
    t->size = size;
    t->old_size = size;
    t->alignment = size ? size : 1;
    t->update_id = 0;
    t->is_fixed = size != 0;
}

void _init_alloc(CooAlloc *a, struct CooType *type, CooIndirection indirection) {
    a->type = type;
    a->indirection = indirection;
    a->first = 0;
    a->old_first = 0;
}

void _clear_alloc(CooAlloc *a) {
    while (a->first) {
        CooTag *tag = a->first;
        a->first = a->first->next;
        free(tag);
    }
}

static void _apply_diffs(CooType *t, char *src_mem, char *dst_mem) {
    for (int i = 0; i < t->diffs_count; ++i) {
        CooDiff *d = t->diffs + i;
        if (d->diff_type == CDT_COPY) {
            if (d->indirection != IT_VAL)
                memcpy(dst_mem + d->dst_offset, src_mem + d->src_offset, sizeof(void *) * d->count);
            else if (d->to_type->is_fixed)
                memcpy(dst_mem + d->dst_offset, src_mem + d->src_offset, d->to_type->size * d->count);
            else
                for (int j = 0; j < d->count; ++j)
                    _apply_diffs(d->to_type,
                                 src_mem + d->src_offset + j * d->src_stride,
                                 dst_mem + d->dst_offset + j * d->dst_stride);
        }
        else if (d->diff_type == CDT_CAST) {
            if (d->indirection != IT_VAL)
                memset(dst_mem + d->dst_offset, 0, sizeof(void *) * d->count);
            else if (d->cast)
                for (int j = 0; j < d->count; ++j)
                    d->cast->func(src_mem + d->src_offset + j * d->src_stride,
                                  dst_mem + d->dst_offset + j * d->dst_stride);
            else
                memset(dst_mem + d->dst_offset, 0, d->to_type->size * d->count);
        }
        else if (d->diff_type == CDT_NULL) {
            if (d->indirection != IT_VAL)
                memset(dst_mem + d->dst_offset, 0, sizeof(void *) * d->count);
            else
                memset(dst_mem + d->dst_offset, 0, d->to_type->size * d->count);
        }
    }
}

static CooTag *_malloc_with_tag(int size, int count, CooTag *prev, CooTag *next) {
    CooTag *tag = malloc(sizeof(CooTag) + size * count);
    tag->count = count;
    tag->prev = prev;
    tag->next = next;
    return tag;
}

void _update_alloc_data_layout(CooAlloc *a) {
    CooTag *o_tag = a->first;
    if (a->indirection == IT_VAL && a->type->is_fixed == false) {
        while (o_tag) {
            CooTag *n_tag = _malloc_with_tag(a->type->size, o_tag->count,
                                             o_tag->prev, o_tag->next);
            for (int i = 0; i < o_tag->count; ++i)
                _apply_diffs(a->type,
                             (char *)_tag_to_data(o_tag) + a->type->old_size * i,
                             (char *)_tag_to_data(n_tag) + a->type->size * i);
            o_tag->redirect = n_tag; /* old tag redirects to new tag */
            o_tag = o_tag->next;
        }
    }
}

static CooCast *_find_cast(CooType *t, CooType *to_type) {
    for (int i = 0; i < t->casts_count; ++i)
        if (t->casts[i].to_type == to_type)
            return t->casts + i;
    return 0;
}

static int _min(int a, int b) {
    return (a < b) ? a : b;
}

static int _max(int a, int b) {
    return (a > b) ? a : b;
}

static int _round_up(int value, int base) {
    return value + (base - (value % base)) % base;
}

static int _variable_alignment(CooVar *v) {
    return (v->indirection != IT_VAL) ? sizeof(void *) : v->type->alignment;
}

static int _variable_size(CooVar *v) {
    return (v->indirection != IT_VAL) ? sizeof(void *) : v->type->size;
}

static int _variable_old_size(CooVar *v) {
    return (v->indirection != IT_VAL) ? sizeof(void *) : v->type->old_size;
}

void _update_type_layout(CooType *t, int update_id) {
    if (t->is_fixed || t->update_id == update_id) /* get out if fixed or already updated */
        return;
    t->update_id = update_id;
    t->old_size = t->size;
    t->size = 0;
    t->alignment = 1;
    t->diffs_count = 0;
    for (int i = 0; i < t->new_vars_count; ++i) {
        CooVar *v = t->new_vars + i;
        _update_type_layout(v->type, update_id);
        v->offset = _round_up(t->size, _variable_alignment(v));

        if (v->old_index == -1) { /* new variable */
            CooDiff *d = t->diffs + t->diffs_count++;
            d->diff_type = CDT_NULL;
            d->dst_offset = v->offset;
            d->count = v->count;
            d->to_type = v->type;
            d->indirection = v->indirection;
        }
        else { /* old variable */
            CooVar *old_v = t->vars + v->old_index;
            int copied_count = _min(v->count, old_v->count);
            if (v->type != old_v->type) { /* type changed, cast variable value(s) if cast exists */
                CooDiff *d = t->diffs + t->diffs_count++;
                d->diff_type = CDT_CAST;
                d->src_offset = old_v->offset;
                d->dst_offset = v->offset;
                d->src_stride = _variable_old_size(old_v);
                d->dst_stride = _variable_size(v);
                d->count = copied_count;
                d->cast = _find_cast(old_v->type, v->type);
                d->indirection = v->indirection;
            }
            else { /* type remained same, copy variable value(s) */
                CooDiff *d = t->diffs + t->diffs_count++;
                d->diff_type = CDT_COPY;
                d->src_offset = old_v->offset;
                d->dst_offset = v->offset;
                d->src_stride = _variable_old_size(old_v);
                d->dst_stride = _variable_size(v);
                d->count = copied_count;
                d->to_type = v->type;
                d->indirection = v->indirection;
            }
            if (v->count > old_v->count) { /* new array value(s), initialize to 0 */
                CooDiff *d = t->diffs + t->diffs_count++;
                d->diff_type = CDT_NULL;
                d->dst_offset = v->offset + old_v->count * _variable_size(v);
                d->count = v->count - old_v->count;
                d->to_type = v->type;
                d->indirection = v->indirection;
            }
        }

        t->size = v->offset + _variable_size(v) * v->count;
        t->alignment = _max(t->alignment, _variable_alignment(v));
        v->old_index = i;
    }
    t->size = _round_up(t->size, t->alignment);
    for (int i = 0; i < t->new_vars_count; ++i)
        t->vars[i] = t->new_vars[i];
    t->vars_count = t->new_vars_count;
}

static void _redirect_pointers(char *mem, int count) {
    for (int i = 0; i < count; ++i) {
        *(void **)mem = _update_pointer(*(void **)mem);
        mem += sizeof(void *);
    }
}

static void _redirect_struct_pointers(char *mem, CooType *type, int count, CooIndirection indirection) {
    if (type->vars_count == 0)
        return;
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < type->vars_count; ++j) {
            CooVar *v = type->vars + j;
            if (v->indirection == IT_MPTR)
                _redirect_pointers(mem + v->offset, v->count);
            else if (v->indirection == IT_VAL)
                _redirect_struct_pointers(mem + v->offset, v->type, v->count, v->indirection);
        }
        mem += type->size;
    }
}

void _update_alloc_pointers(CooAlloc *a) {
    if (a->indirection == IT_MPTR) { /* pointers to managed data */
        for (CooTag *tag = a->first; tag; tag = tag->next)
            _redirect_pointers((char *)_tag_to_data(tag), tag->count);
    }
    else if (a->indirection == IT_VAL) { /* structs */
        if (a->type->is_fixed) { /* unmanaged structs */
            for (CooTag *tag = a->first; tag; tag = tag->next)
                _redirect_struct_pointers((char *)_tag_to_data(tag), a->type, tag->count, a->indirection);
        }
        else { /* managed structs */
            a->old_first = a->first; /* for freeing old data later */
            CooTag *tag = a->first = a->first ? a->first->redirect : 0;
            while (tag) {
                tag->prev = tag->prev ? tag->prev->redirect : 0;
                tag->next = tag->next ? tag->next->redirect : 0;
                _redirect_struct_pointers((char *)_tag_to_data(tag), a->type, tag->count, a->indirection);
                tag = tag->next;
            }
        }
    }
}

void _free_old_versions_of_data(CooAlloc *a) {
    if (a->indirection == IT_VAL && a->type->is_fixed == false) {
        while (a->old_first) {
            CooTag *tag = a->old_first;
            a->old_first = a->old_first->next;
            free(tag);
        }
    }
}

static void _init_var(CooVar *v, const char *name, CooType *t, int count, CooIndirection indirection) {
    strcpy_s(v->name, COO_MAX_NAME, name);
    v->type = t;
    v->count = count;
    v->indirection = indirection;
    v->old_index = -1;
}

static void _add_var(CooType *t, const char *v_name, CooType *v_type,
                     int v_count, int v_index, int v_is_pointer) {
    assert(t->diffs_count < COO_MAX_DIFFS); /* no room for another variable */
    for (int i = 0; i < t->vars_count; ++i)
        assert(strcmp(v_name, t->vars[i].name) != 0); /* no old variable with same name */
    for (int i = 0; i < t->new_vars_count; ++i)
        assert(strcmp(v_name, t->new_vars[i].name) != 0); /* no new variable with same name */
    if (v_index < 0 || v_index > t->new_vars_count) /* if index out of range, append variable */
        v_index = t->new_vars_count;
    for (int i = t->new_vars_count - 1; i >= v_index; --i) /* make room for the new variable */
        t->new_vars[i + 1] = t->new_vars[i];
    CooVar *v = t->new_vars + v_index;
    _init_var(v, v_name, v_type, v_count, v_is_pointer);
    ++t->new_vars_count;
}

void coo_add_var(CooType *t, const char *v_name, CooType *v_type) {
    _add_var(t, v_name, v_type, 1, -1, IT_VAL);
}

void coo_ins_var(CooType *t, const char *v_name, CooType *v_type, int v_index) {
    _add_var(t, v_name, v_type, 1, v_index, IT_VAL);
}

void coo_add_arr(CooType *t, const char *v_name, CooType *v_type, int v_count) {
    _add_var(t, v_name, v_type, v_count, -1, IT_VAL);
}

void coo_ins_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index) {
    _add_var(t, v_name, v_type, v_count, v_count, IT_VAL);
}

void coo_add_ptr_var(CooType *t, const char *v_name, CooType *v_type) {
    _add_var(t, v_name, v_type, 1, -1, IT_MPTR);
}

void coo_ins_ptr_var(CooType *t, const char *v_name, CooType *v_type, int v_index) {
    _add_var(t, v_name, v_type, 1, v_index, IT_MPTR);
}

void coo_add_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count) {
    _add_var(t, v_name, v_type, v_count, -1, IT_MPTR);
}

void coo_ins_ptr_arr(CooType *t, const char *v_name, CooType *v_type, int v_count, int v_index) {
    _add_var(t, v_name, v_type, v_count, v_count, IT_MPTR);
}

static int _variable_index(CooVar *vars, int vars_count, const char *v_name) {
    for (int i = 0; i < vars_count; ++i)
        if (strcmp(vars[i].name, v_name) == 0)
            return i;
    return -1;
}

void coo_remove_var(CooType *t, const char *v_name) {
    assert(t->is_fixed == false);
    int index = _variable_index(t->new_vars, t->new_vars_count, v_name);
    assert(index != -1); /* variable not found */
    for (int i = index + 1; i < t->new_vars_count; ++i)
        t->new_vars[i - 1] = t->new_vars[i];
    --t->new_vars_count;
}

void coo_resize_array(CooType *t, const char *v_name, int length) {
    assert(t->is_fixed == false);
    assert(length > 0);
    int index = _variable_index(t->new_vars, t->new_vars_count, v_name);
    assert(index != -1); /* variable not found */
    t->new_vars[index].count = length;
}

void coo_move_var(CooType *t, const char *v_name, int new_index) {
    assert(t->is_fixed == false);
    int old_index = _variable_index(t->new_vars, t->new_vars_count, v_name);
    assert(old_index != -1); /* variable not found */
    if (old_index == new_index)
        return;
    CooVar v = t->new_vars[old_index];
    if (new_index < old_index)
        for (int i = old_index; i > new_index; --i)
            t->new_vars[i] = t->new_vars[i - 1];
    else
        for (int i = old_index; i < new_index; ++i)
            t->new_vars[i] = t->new_vars[i + 1];
    t->new_vars[new_index] = v;
}

void coo_retype_var(struct CooType *t, const char *v_name, struct CooType *to_type) {
    assert(to_type != 0);
    assert(t->is_fixed == false);
    int index = _variable_index(t->new_vars, t->new_vars_count, v_name);
    assert(index != -1); /* variable not found */
    t->new_vars[index].type = to_type;
}

void *coo_alloc(CooAlloc *a, int count) {
    if (count <= 0)
        return 0;
    int size = (a->indirection != IT_VAL) ? sizeof(void *) : a->type->size;
    CooTag *tag = _malloc_with_tag(size, count, 0, a->first);
    if (a->first)
        a->first->prev = tag;
    a->first = tag;
    void *data = _tag_to_data(tag);
    memset(data, 0, size * count); /* zero all new allocated memory */
    return data;
}

void coo_free(CooAlloc *a, void *data) {
    if (data == 0)
        return;
    CooTag *tag = _data_to_tag(data);
    if (tag->prev)
        tag->prev->next = tag->next;
    else
        a->first = tag->next;
    if (tag->next)
        tag->next->prev = tag->prev;
    free(tag);
}
