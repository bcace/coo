# Coo

## What is Coo?

Coo is an attempt at having hot-reloadable struct field layouts for [POD](https://en.wikipedia.org/wiki/Passive_data_structure) style data.

Some compiled statically typed languages either already have the ability to hot-reload code, or are working on it (see Links in the paragraph at the bottom), but as far as I can see the limitation is still that code changes should not include changes to struct layouts of the program's live state (adding, moving, removing and/or modifying struct fields).

I believe that a mechanism for updating struct layouts of live data would make hot-reloading code even more attractive, especially since I had to implement limited versions of this mechanisms at my job and in some of my private projects.

## How does it work?

First, Coo has to know what all the struct layouts look like. Then all the data must be allocated and freed through Coo, which ensures that Coo knows where all the data of a certain type is. When a struct layout is changed Coo can go through all affected data, create a new version of each struct or array of structs, and then redirect all the pointers so the entire state is still valid. Update is separated into two calls (```coo_begin_update``` and ```coo_end_update```) so that all the outside pointers pointing to the managed data have the opportunity to be updated.

(a few simple examples, fields, arrays, composition, implicit casts)

```C
/* initial layout */
struct A1 {
    int a;
    int b;
};

/* describe the A1 type layout to Coo */
CooType *A_type = coo_create_type(coo, "A");
coo_add_var(A_type, "a", &CooI32);
coo_add_var(A_type, "b", &CooI32);

/* commit the new type layout */
coo_begin_update(coo);
coo_end_update(coo);

/* make an allocator for type A */
CooAlloc *A_alloc = coo_get_alloc(coo, A_type);

/* allocate one struct of that type and initialize its fields */
A1 *a1 = (A1 *)coo_alloc(A_alloc, 1);
a1->a = 1;
a1->b = 2;

/* modified layout (swapped fields) */
struct A2 {
    int b;
    int a;
};

/* describe the layout change to Coo (move "b" field to position 0) */
coo_move_var(A_type, "b", 0);

/* update layout of allocated object */
coo_begin_update(coo);
A2 *a2 = coo_update_pointer(a1);
coo_end_update(coo);

/* verify that the data in fields is still intact */
assert(a1->a == 1);
assert(a1->b == 2);
```

(pointers, stack pointers, in-pointers, out-pointers)

## What's missing?

* Update data of only affected types.
* Avoid having all data duplicated at the same time using dependencies between types.
* Allow different alignment rules or explicit packing for individual structs.
* Allow alternative allocators.
* Unions and bit fields.
* Demo language with x64 code generator and a simple REPL for demoing.
* Managed pointers inside structs and arrays.

## Links

* [Nim](https://nim-lang.org/docs/hcr.html)
* [Mun](https://mun-lang.org/)
* [Zig](https://github.com/ziglang/zig/issues/68)
* [Live++](https://liveplusplus.tech/)
* [Projucer](https://juce.com/discover/projucer)
