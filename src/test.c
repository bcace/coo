#include "test.h"
#include "coo.h"
#include <stdio.h>
#include <assert.h>


void coo_test_basics() {
    CooState *coo = coo_create_state();

    /* create a new empty type and allocate one object */

    CooType *A_type = coo_create_type(coo, "A");
    CooAlloc *A_alloc = coo_get_alloc(coo, A_type);
    coo_begin_update(coo);
    coo_end_update(coo);

    void *a0 = coo_alloc(A_alloc, 1);

    /* add one integer variable to the type and check that its value is zeroed */

    typedef struct {
        int a;
    } A1;

    coo_add_var(A_type, "a", &CooI32);
    coo_begin_update(coo);
    A1 *a1 = coo_update_pointer(a0);
    coo_end_update(coo);

    assert(a1->a == 0);

    a1->a = 13;

    /* add another variable and verify that it retained its value after update */

    typedef struct {
        int b;
        int a;
    } A2;

    coo_ins_var(A_type, "b", &CooI32, 0);
    coo_begin_update(coo);
    A2 *a2 = coo_update_pointer(a1);
    coo_end_update(coo);

    assert(a2->a == 13);
    assert(a2->b == 0);

    /* remove newly created variable and verify the retained value */

    coo_remove_var(A_type, "b");
    coo_begin_update(coo);
    a1 = coo_update_pointer(a2);
    coo_end_update(coo);

    assert(a1->a == 13);

    /* change variable into an array and back and verify value */

    typedef struct {
        int a[10];
    } A3;

    coo_resize_array(A_type, "a", 10);
    coo_begin_update(coo);
    A3 *a3 = coo_update_pointer(a1);
    coo_end_update(coo);

    assert(a3->a[0] == 13);
    assert(a3->a[9] == 0);

    coo_resize_array(A_type, "a", 1);
    coo_begin_update(coo);
    a1 = coo_update_pointer(a3);
    coo_end_update(coo);

    assert(a1->a == 13);

    /* add variables and move them around */

    typedef struct {
        int a;
        int b;
        int c;
    } A4;

    coo_add_var(A_type, "b", &CooI32);
    coo_add_var(A_type, "c", &CooI32);
    coo_begin_update(coo);
    A4 *a4 = coo_update_pointer(a1);
    coo_end_update(coo);

    assert(a4->a == 13);

    a4->b = 14;
    a4->c = 15;

    typedef struct {
        int c;
        int b;
        int a;
    } A5;

    coo_move_var(A_type, "b", 0);
    coo_move_var(A_type, "c", 0);
    coo_begin_update(coo);
    A5 *a5 = coo_update_pointer(a4);
    coo_end_update(coo);

    assert(a5->a == 13);
    assert(a5->b == 14);
    assert(a5->c == 15);

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
    A6 *a6 = coo_update_pointer(a5);
    coo_end_update(coo);

    assert(a6->a == 13.0); /* implicit cast */
    assert(a6->b == 0.0f); /* zeroed because of loss of data */
    assert(a6->c == 15ll); /* implicit cast */

    coo_destroy_state(coo);
}

void coo_test_pointers() {
    CooState *coo = coo_create_state();

    /* make linked list */

    typedef struct A1 {
        int a;
        struct A1 *next;
    } A1;

    CooType *A_type = coo_create_type(coo, "A");
    CooAlloc *A_alloc = coo_get_alloc(coo, A_type);
    coo_begin_update(coo);
    coo_end_update(coo);

    coo_add_var(A_type, "a", &CooI32);
    coo_add_ptr_var(A_type, "next", A_type);
    coo_begin_update(coo);
    coo_end_update(coo);

    A1 *a1_root = coo_alloc(A_alloc, 1);
    A1 *a1_prev = a1_root;
    for (int i = 1; i < 10; ++i) {
        A1 *a1 = coo_alloc(A_alloc, 1);
        a1->a = i;
        a1_prev->next = a1;
        a1_prev = a1;
    }

    /* swap variables and verify that the list is still linked correctly */

    typedef struct A2 {
        struct A2 *next;
        int a;
    } A2;

    coo_move_var(A_type, "a", 1);
    coo_begin_update(coo);
    A2 *a2_root = coo_update_pointer(a1_root);
    coo_end_update(coo);

    int a2_i = 0;
    for (A2 *a2 = a2_root; a2; a2 = a2->next, ++a2_i)
        assert(a2->a == a2_i);

    /* allocate an array of pointers to structs and link them to elements in list */

    CooAlloc *A_alloc_ptrs = coo_get_ptr_alloc(coo, A_type);
    A2 **v2 = coo_alloc(A_alloc_ptrs, 10);

    for (A2 *a2 = a2_root; a2; a2 = a2->next)
        v2[a2->a] = a2;

    /* swap A type fields back and verify that array pointers are correct */

    coo_move_var(A_type, "a", 0);
    coo_begin_update(coo);
    a1_root = coo_update_pointer(a2_root);
    coo_end_update(coo);

    A1 **v1 = (A1 **)v2; /* array of pointers is not updateable, just cast */

    for (int i = 0; i < 10; ++i)
        assert(v1[i]->a == i);

    coo_destroy_state(coo);
}

void coo_test_struct_composition() {
    CooState *coo = coo_create_state();

    typedef struct {
        char c;
        int i[10];
        short s;
    } A1;

    typedef struct {
        char c;
        A1 a;
        double d;
    } B1;

    CooType *A_type = coo_create_type(coo, "A");
    CooAlloc *A_alloc = coo_get_alloc(coo, A_type);
    coo_add_var(A_type, "c", &CooI8);
    coo_add_arr(A_type, "i", &CooI32, 10);
    coo_add_var(A_type, "s", &CooI16);

    CooType *B_type = coo_create_type(coo, "B");
    CooAlloc *B_alloc = coo_get_alloc(coo, B_type);
    coo_add_var(B_type, "c", &CooI8);
    coo_add_var(B_type, "a", A_type);
    coo_add_var(B_type, "d", &CooF64);

    coo_begin_update(coo);
    coo_end_update(coo);

    B1 *b1 = coo_alloc(B_alloc, 1);
    b1->c = 1;
    b1->a.c = 2;
    b1->a.i[0] = 3;
    b1->a.s = 4;
    b1->d = 5.0;

    typedef struct {
        short s;
        int i;
    } A2;

    typedef struct {
        char c;
        double d;
        A2 a[10];
    } B2;

    coo_remove_var(A_type, "c");
    coo_move_var(A_type, "i", 1);
    coo_resize_array(A_type, "i", 1);
    coo_move_var(B_type, "d", 1);
    coo_resize_array(B_type, "a", 10);

    coo_begin_update(coo);
    B2 *b2 = coo_update_pointer(b1);
    coo_end_update(coo);

    assert(b2->c == 1);
    assert(b2->a[0].i == 3);
    assert(b2->a[0].s == 4);
    assert(b2->d == 5.0);

    coo_destroy_state(coo);
}

void coo_test_alloc() {
    coo_test_basics();
    coo_test_pointers();
    coo_test_struct_composition();
}
