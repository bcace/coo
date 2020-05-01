# Coo

## What is Coo?

Coo is an attempt at having hot-reloadable field layouts for [POD](https://en.wikipedia.org/wiki/Passive_data_structure) style data.

Compiled and statically typed languages either already have the ability to hot-reload code, or are working to add it, but as far as I can see the limitation is still that reloaded code cannot contain changes to struct layouts of the program's live state (adding, moving, removing and/or modifying fields in a struct) since this would lead to inconsistency between layouts that the reloaded code assumes and layouts in the actual state, or in other words a break in backward compatibility.

Often if layouts are changed data also has to be adapted, and usually this is handled by upgrade code. Since Coo can obviously only do simple adaptations, like initializing new variables with default value or using registered cast functions when switching variable types, number of layout change situations where these adaptations are good enough is limited, but in my experience in [embedded scripting](https://en.wikipedia.org/wiki/Scripting_language#Extension/embeddable_languages) situations where code changes are usually smaller and submitted more often this number can be significant.

Additionally, in embedded scripting a nice advantage over most embeddable languages could be that using PODs created using standard alignment rules means that data can be used directly between the host program and the hot-reloadable environment, without need for copying or wrapping data in order to cross the boundary.

## How does it work?

Coo hot-reloadable state resides in a ```CooState``` structure and contains two types of information: struct layout descriptions and locations of all the allocated data.

```C
CooState *coo_state = coo_create_state(); /* create a Coo state */

/* use Coo */

coo_destroy_state(coo_state); /* remember to destroy Coo state, this frees all the memory allocated through Coo */
```

#### Layouts

Coo state's POD structure layout descriptions (or **Coo types**, ```CooType```), contain all the information about type's fields (or **Coo variables**, ```CooVar```), and accumulate changes to its layout (removed or added variables, changed types, etc.). Each Coo variable specifies its name, type, count (how many values it holds, one if it's a regular variable, many if it's a static array), and whether it contains values or pointers.

```C
/* struct definition in host */
struct MyType_v1 {
    int a;
    double b[20];
    char *c;
    struct NestedType_v1 {
        short int s;
    } n;
};

/* same struct definition in Coo */
CooType *my_type = coo_create_type(coo_state, "MyType");
coo_add_var(my_type, "a", &CooI32);
coo_add_arr(my_type, "b", &CooF64, 20);
coo_add_ptr_var(my_type, "c", &CooI8);
CooType *nested_type = coo_create_type(coo_state, "NestedType");
    coo_add_var(nested_type, "s", &CooI16);
coo_add_var(my_type, "n", nested_type);
```

#### Data

Coo only hot-reloads heap data and all hot-reloadable data must be allocated for a specific Coo type through **Coo allocators** (```CooAlloc```). Coo allocators use layout information from their Coo type to allocate data using standard C struct alignment rules.

```C
/* get allocator for MyType data */
CooAlloc *alloc = coo_get_alloc(coo_state, my_type);

/* allocate single struct */
MyType_v1 *my_object_v1 = coo_alloc(alloc, 1);

/* allocate array of 100000 structs */
MyType_v1 *my_array_v1 = coo_alloc(alloc, 100000);
```

#### Modifying layouts

Once we have our types and allocated data of those types we can start playing with the layouts by adding, removing and inserting variables, changing their type or count. These changes are not immediately reflected on the data, but accumulated in the Coo state to be applied during the update step.

```C
/* new definition of MyType in host */
struct MyType_v2 {
    long long int a;        /* type changed */
    double b[30];           /* array size changed */
    struct NestedType_v2 {
        double d;           /* new variable inserted */
        short int s;
    } n;
    char *c;                /* moved */
};

/* same layout changes in Coo */
coo_retype_var(my_type, "a", &CooI64);
coo_resize_array(my_type, "b", 30);
coo_move_var(my_type, "c", 3);
coo_ins_var(nested_type, "d", &CooF64, 0);
```

#### Update

During update step Coo compiles all the changes into simple instructions. Then a copy of each individual instance and array in Coo state is created in memory and instructions are applied to each pair to translate the data from old layout to new. While both versions of data exist in memory (between ```coo_begin_update``` and ```coo_end_update``` calls) all pointers in Coo state are redirected to point to new copies, and in host code pointers that point into the Coo state can be updated:

```C
coo_begin_update(coo_state);

MyType_v2 *my_object_v2 = (MyType_v1 *)coo_update_pointer(my_object_v1);
MyType_v2 *my_array_v2 = (MyType_v1 *)coo_update_pointer(my_array_v1);

coo_end_update(coo_state);
```

Data translations are done so that most data is kept unchanged; so if a variable just moved inside the type it keeps the value, if it changes type and a cast function between the two types is registered the cast is applied (if not variable is zeroed), if a static array increased in size additional elements are zeroed, and if it reduced in size all the remaining elements have their old values. All new variables' values are zeroed.

## What's missing?

* Replace group of variables with a struct with same layout and vice versa.
* Update data only of affected types.
* Avoid having all data duplicated at the same time using dependencies between types.
* Allow different alignment rules or explicit packing for individual structs.
* Allow allocation functions other than C's malloc and free.
* Register custom cast functions.
* When adding a variable specify default value other than 0.
* Unions and bit fields.
* Managed pointers that point inside structs and arrays.
