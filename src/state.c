#include "state.h"
#include "layout.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>


int primitives_inited = 0;

CooType CooI8, CooI16, CooI32, CooI64, CooF32, CooF64;

static void _i8_to_i16(void *src, void *dst) { *(int16_t *)dst = *(int8_t *)src; }
static void _i8_to_i32(void *src, void *dst) { *(int32_t *)dst = *(int8_t *)src; }
static void _i8_to_i64(void *src, void *dst) { *(int64_t *)dst = *(int8_t *)src; }
static void _i8_to_f32(void *src, void *dst) { *(float *)dst = *(int8_t *)src; }
static void _i8_to_f64(void *src, void *dst) { *(double *)dst = *(int8_t *)src; }
static void _i16_to_i32(void *src, void *dst) { *(int32_t *)dst = *(int16_t *)src; }
static void _i16_to_i64(void *src, void *dst) { *(int64_t *)dst = *(int16_t *)src; }
static void _i16_to_f32(void *src, void *dst) { *(float *)dst = *(int16_t *)src; }
static void _i16_to_f64(void *src, void *dst) { *(double *)dst = *(int16_t *)src; }
static void _i32_to_i64(void *src, void *dst) { *(int64_t *)dst = *(int32_t *)src; }
static void _i32_to_f64(void *src, void *dst) { *(double *)dst = *(int32_t *)src; }
static void _f32_to_f64(void *src, void *dst) { *(double *)dst = *(float *)src; }

static void _add_cast(CooType *t, CooType *to_type, COO_CAST_FUNC func) {
    assert(func != 0);
    assert(t->casts_count < COO_MAX_CASTS);
    CooCast *c = t->casts + t->casts_count++;
    c->to_type = to_type;
    c->func = func;
}

CooState *coo_create_state() {
    CooState *s = malloc(sizeof(CooState));
    s->allocs_count = 0;
    s->types_count = 0;
    s->update_id = 0;

    if (primitives_inited == 0) {
        _init_type(&CooI8, "i8", sizeof(int8_t));
        _init_type(&CooI16, "i16", sizeof(int16_t));
        _init_type(&CooI32, "i32", sizeof(int32_t));
        _init_type(&CooI64, "i64", sizeof(int64_t));
        _init_type(&CooF32, "f32", sizeof(float));
        _init_type(&CooF64, "f64", sizeof(double));

        _add_cast(&CooI8, &CooI16, _i8_to_i16);
        _add_cast(&CooI8, &CooI32, _i8_to_i32);
        _add_cast(&CooI8, &CooI64, _i8_to_i64);
        _add_cast(&CooI8, &CooF32, _i8_to_f32);
        _add_cast(&CooI8, &CooF64, _i8_to_f64);
        _add_cast(&CooI16, &CooI32, _i16_to_i32);
        _add_cast(&CooI16, &CooI64, _i16_to_i64);
        _add_cast(&CooI16, &CooF32, _i16_to_f32);
        _add_cast(&CooI16, &CooF64, _i16_to_f64);
        _add_cast(&CooI32, &CooI64, _i32_to_i64);
        _add_cast(&CooI32, &CooF64, _i32_to_f64);
        _add_cast(&CooF32, &CooF64, _f32_to_f64);
    }

    return s;
}

static void _delete_alloc(CooAlloc *a) {
    _clear_alloc(a);
    free(a);
}

static void _delete_type(CooType *t) {
    free(t);
}

void coo_destroy_state(CooState *s) {
    for (int i = 0; i < s->allocs_count; ++i)
        _delete_alloc(s->allocs[i]);
    s->allocs_count = 0;
    for (int i = 0; i < s->types_count; ++i)
        _delete_type(s->types[i]);
    s->types_count = 0;
    free(s);
}

static int _find_type(CooState *s, const char *name) {
    for (int i = 0; i < s->types_count; ++i)
        if (strcmp(name, s->types[i]->name) == 0)
            return i;
    return -1;
}

CooType *coo_create_type(CooState *s, const char *name) {
    assert(_find_type(s, name) == -1);
    assert(s->types_count < COO_MAX_TYPES);
    CooType *type = malloc(sizeof(CooType));
    _init_type(type, name, 0);
    return s->types[s->types_count++] = type;
}

static void _remove_allocs_of_type(CooState *s, CooType *type) {
    int removed = 0;
    for (int i = 0; i < s->allocs_count; ++i)
        if (s->allocs[i]->type == type) {
            _delete_alloc(s->allocs[i]);
            ++removed;
        }
        else if (removed)
            s->allocs[i - removed] = s->allocs[i];
    s->allocs_count -= removed;
}

void coo_remove_type(CooState *s, const char *name) {
    int index = _find_type(s, name);
    if (index == -1)
        return;
    _delete_type(s->types[index]);
    _remove_allocs_of_type(s, s->types[index]);
    for (int i = index + 1; i < s->types_count; ++i)
        s->types[i - 1] = s->types[i];
    --s->types_count;
}

static int _index_of_alloc(CooState *s, CooType *type, CooIndirection indirection) {
    for (int i = 0; i < s->allocs_count; ++i)
        if (s->allocs[i]->type == type && s->allocs[i]->indirection == indirection)
            return i;
    return -1;
}

static CooAlloc *_get_alloc(CooState *s, CooType *type, CooIndirection indirection) {
    int index = _index_of_alloc(s, type, indirection);
    if (index != -1)
        return s->allocs[index];
    assert(s->allocs_count < COO_MAX_ALLOCS);
    CooAlloc *alloc = malloc(sizeof(CooAlloc));
    _init_alloc(alloc, type, indirection);
    return s->allocs[s->allocs_count++] = alloc;
}

CooAlloc *coo_get_alloc(CooState *s, CooType *type) {
    return _get_alloc(s, type, IT_VAL);
}

CooAlloc *coo_get_ptr_alloc(CooState *s, CooType *type) {
    return _get_alloc(s, type, IT_PTR);
}

void coo_remove_alloc(CooState *s, CooType *type, CooIndirection indirection) {
    int index = _index_of_alloc(s, type, indirection);
    if (index == -1)
        return;
    _delete_alloc(s->allocs[index]);
    for (int i = index + 1; i < s->allocs_count; ++i)
        s->allocs[i - 1] = s->allocs[i];
    --s->allocs_count;
}

void coo_clear_alloc(CooAlloc *a) {
    _clear_alloc(a);
}

void coo_begin_update(CooState *s) {
    ++s->update_id;
    for (int i = 0; i < s->types_count; ++i)
        _update_type_layout(s->types[i], s->update_id);
    for (int i = 0; i < s->allocs_count; ++i)
        _update_alloc_data_layout(s->allocs[i]);
}

void coo_end_update(CooState *s) {
    for (int i = 0; i < s->allocs_count; ++i)
        _update_alloc_pointers(s->allocs[i]);
    for (int i = 0; i < s->allocs_count; ++i)
        _free_old_versions_of_data(s->allocs[i]);
}
