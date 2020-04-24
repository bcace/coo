#include "test.h"
#include "coo.h"
#include <stdio.h>
#include <assert.h>


void coo_test_alloc() {
    CooState *coo = coo_create_state();

    /* create a new empty type and allocate one object */

    CooType *A_type = coo_create_type(coo, "A");
    CooAlloc *A_alloc = coo_get_alloc(coo, A_type);
    coo_begin_update(coo);
    coo_end_update(coo);

    void *a = coo_alloc(A_alloc, 1);

    /* add one integer variable to the type and check that its value is zeroed */

    typedef struct {
        int a;
    } A1;

    coo_add_var(A_type, "a", &CooI32);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A1 *)a)->a == 0);

    ((A1 *)a)->a = 13;

    /* add another variable and verify that it retained its value after update */

    typedef struct {
        int b;
        int a;
    } A2;

    coo_ins_var(A_type, "b", &CooI32, 0);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A2 *)a)->a == 13);
    assert(((A2 *)a)->b == 0);

    /* remove newly created variable and verify the retained value */

    coo_remove_var(A_type, "b");
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A1 *)a)->a == 13);

    /* change variable into an array and back and verify value */

    typedef struct {
        int a[10];
    } A3;

    coo_resize_array(A_type, "a", 10);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A3 *)a)->a[0] == 13);
    assert(((A3 *)a)->a[9] == 0);

    coo_resize_array(A_type, "a", 1);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A1 *)a)->a == 13);

    /* add variables and move them around */

    typedef struct {
        int a;
        int b;
        int c;
    } A4;

    coo_add_var(A_type, "b", &CooI32);
    coo_add_var(A_type, "c", &CooI32);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A4 *)a)->a == 13);

    ((A4 *)a)->b = 14;
    ((A4 *)a)->c = 15;

    typedef struct {
        int c;
        int b;
        int a;
    } A5;

    coo_move_var(A_type, "b", 0);
    coo_move_var(A_type, "c", 0);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A5 *)a)->a == 13);
    assert(((A5 *)a)->b == 14);
    assert(((A5 *)a)->c == 15);

    /* change types of variables and verify values */

    typedef struct {
        long long int c;
        float b;
        double a;
    } A6;

    coo_retype_var(A_type, "a", &CooF64);
    coo_retype_var(A_type, "b", &CooF32);
    coo_retype_var(A_type, "c", &CooI64);
    coo_begin_update(coo);
    a = coo_update_pointer(a);
    coo_end_update(coo);

    assert(((A6 *)a)->a == 13.0);
    assert(((A6 *)a)->b == 0.0f); /* because of loss of data */
    assert(((A6 *)a)->c == 15ll);

    /* delete old object and create a new array of objects */

    coo_free(A_alloc, a);
    a = coo_alloc(A_alloc, 10);

    assert(((A6 *)a)[9].a == 0.0);
    assert(((A6 *)a)[9].b == 0.0f);
    assert(((A6 *)a)[9].c == 0ll);

    /* create 10 objects and link them in a list */

    typedef struct A7 {
        int a;
        struct A7 *next;
    } A7;

    coo_free(A_alloc, a);
    coo_retype_var(A_type, "a", &CooI32);
    coo_remove_var(A_type, "b");
    coo_remove_var(A_type, "c");
    coo_add_ptr_var(A_type, "next", A_type);
    coo_begin_update(coo);
    coo_end_update(coo);

    A7 *a7_root = coo_alloc(A_alloc, 1);
    A7 *a7_prev = a7_root;
    for (int i = 1; i < 10; ++i) {
        A7 *e = coo_alloc(A_alloc, 1);
        e->a = i;
        a7_prev->next = e;
        a7_prev = e;
    }

    /* swap variables and verify that the list is still linked correctly */

    typedef struct A8 {
        struct A8 *next;
        int a;
    } A8;

    coo_move_var(A_type, "a", 1);
    coo_begin_update(coo);
    A8 *a8_root = coo_update_pointer(a7_root);
    coo_end_update(coo);

    int e_a = 0;
    for (A8 *e = a8_root; e; e = e->next, ++e_a)
        assert(e->a == e_a);

    /* allocate an array of pointers to structs and link them to elements in list */

    CooAlloc *A_alloc_ptrs = coo_get_ptr_alloc(coo, A_type);
    A8 **v8 = coo_alloc(A_alloc_ptrs, 10);

    for (A8 *e = a8_root; e; e = e->next)
        v8[e->a] = e;

    /* swap A type fields back and verify that array pointers are correct */

    coo_move_var(A_type, "a", 0);
    coo_begin_update(coo);
    a7_root = coo_update_pointer(a8_root);
    A7 **v7 = coo_update_pointer(v8);
    coo_end_update(coo);

    for (int i = 0; i < 10; ++i)
        assert(v7[i]->a == i);

    /* destroy coo state */

    coo_destroy_state(coo);

    printf("Alloc test OK.\n");
}
