# Coo

## What is Coo?

Coo is an attempt at having hot-reloadable field layouts for [POD](https://en.wikipedia.org/wiki/Passive_data_structure) style data.

Compiled and statically typed languages either already have the ability to hot-reload code, or are working to add it, but as far as I can see the limitation is still that reloaded code cannot contain changes to struct layouts of the program's live state (adding, moving, removing and/or modifying fields in a struct) since this would lead to inconsistency between layouts that the reloaded code assumes and layouts in the actual state, or in other words a break in backward compatibility.

Since Coo is trying to perform data upgrade automatically the number of cases where it can do a decent job is still limited, but in my experience in [embedded scripting](https://en.wikipedia.org/wiki/Scripting_language#Extension/embeddable_languages) situations this number can be significant since code changes are usually submitted more often and layout changes tend to be smaller.

## How does it work?

**Coo has to know about all types and their layouts**

```C
CooType *my_type = coo_create_type(coo_state, "MyType");
coo_add_var(my_type, "a", &CooI32);
coo_add_var(my_type, "b", &CooF64);

/* is equivalent to */

struct MyType {
    int a;
    double b;
};
```
**All data is allocated through Coo**

```C
CooAlloc *my_alloc = coo_get_alloc(coo_state, my_type);
MyType *my_struct = coo_alloc(my_alloc, 1);
```

**Coo duplicates data with all the appropriate changes**

**Coo updates pointers**

**Coo deletes old data**

First, Coo has to know what all the struct layouts look like. Then all the data must be allocated and freed through Coo, which ensures that Coo knows where all the data of a certain type is. When a struct layout is changed Coo can go through all affected data, create a new version of each struct or array of structs, and then redirect all the pointers so the entire state is still valid. Update is separated into two calls (```coo_begin_update``` and ```coo_end_update```) so that all the outside pointers pointing to the managed data have the opportunity to be updated.

((just describe what's possible and where to find it in code (test.c)))

    (a few simple examples, fields, arrays, composition, implicit casts)

    (pointers, stack pointers, in-pointers, out-pointers)

## What's missing?

* Update data of only affected types.
* Avoid having all data duplicated at the same time using dependencies between types.
* Allow different alignment rules or explicit packing for individual structs.
* Allow alternative allocators.
* Unions and bit fields.
* Demo language with x64 code generator and a simple REPL for demoing.
* Managed pointers inside structs and arrays.
