/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

struct string;
struct array;
struct list;
struct tree;
struct dir;
struct var;
struct pfType;

typedef struct string *String;
typedef struct array *Array;
typedef struct list *List;
typedef struct tree *Tree;
typedef struct dir *Dir;
typedef struct var Var;

typedef char Bit;
typedef unsigned char Byte;
typedef short Short;
typedef int Int;
typedef long long Long;
typedef float Float;
typedef double Double;

union pfVarless
    {
    Bit Bit;
    Byte Byte;
    Short Short;
    Int Int;
    Long Long;
    Float Float;
    Double Double;
    String String;
    Array Array;
    List List;
    Tree Tree;
    Dir Dir;
    void *v;
    };

struct var
    {
    union pfVarless val;
    struct pfType *type;
    };

struct array
    {
    char *elements;		/* Pointer to elements. */
    int count;			/* Count of elements used. */
    int allocated;		/* Count of elements allocated. */
    int elSize;			/* Size of each element. */
    struct pfType *type;	/* Type of each element. */
    };

union pfStack
    {
    Bit Bit;
    Byte Byte;
    Short Short;
    Int Int;
    Long Long;
    Float Float;
    Double Double;
    String String;
    Array Array;
    List List;
    Tree Tree;
    Dir Dir;
    void *v;
    Var Var;
    };
typedef union pfStack PfStack;

struct _pf_iterator
/* Something to iterate over a collection */
    {
    boolean (*next)(struct _pf_iterator *it, void *pItem);
    	/* Get next item into *pItem.  Return FALSE if no more items.  */
    int i;	/* Iterator specific integer data. */
    void *pt;	/* Iterator specific pointer data. */
    };
typedef struct _pf_iterator Pf_iterator;

Pf_iterator _pf_list_iterator_init(List list);
Pf_iterator _pf_tree_iterator_init(Tree tree);
Pf_iterator _pf_dir_iterator_init(Dir dir);

Array _pf_bit_array_from_tuple(PfStack *stack, int count);
Array _pf_byte_array_from_tuple(PfStack *stack, int count);
Array _pf_short_array_from_tuple(PfStack *stack, int count);
Array _pf_int_array_from_tuple(PfStack *stack, int count);
Array _pf_long_array_from_tuple(PfStack *stack, int count);
Array _pf_float_array_from_tuple(PfStack *stack, int count);
Array _pf_double_array_from_tuple(PfStack *stack, int count);
Array _pf_string_array_from_tuple(PfStack *stack, int count);
Array _pf_class_array_from_tuple(PfStack *stack, int count);

List _pf_bit_list_from_tuple(PfStack *stack, int count);
List _pf_byte_list_from_tuple(PfStack *stack, int count);
List _pf_short_list_from_tuple(PfStack *stack, int count);
List _pf_int_list_from_tuple(PfStack *stack, int count);
List _pf_long_list_from_tuple(PfStack *stack, int count);
List _pf_float_list_from_tuple(PfStack *stack, int count);
List _pf_double_list_from_tuple(PfStack *stack, int count);
List _pf_string_list_from_tuple(PfStack *stack, int count);
List _pf_class_list_from_tuple(PfStack *stack, int count);

Tree _pf_bit_tree_from_tuple(PfStack *stack, int count);
Tree _pf_byte_tree_from_tuple(PfStack *stack, int count);
Tree _pf_short_tree_from_tuple(PfStack *stack, int count);
Tree _pf_int_tree_from_tuple(PfStack *stack, int count);
Tree _pf_long_tree_from_tuple(PfStack *stack, int count);
Tree _pf_float_tree_from_tuple(PfStack *stack, int count);
Tree _pf_double_tree_from_tuple(PfStack *stack, int count);
Tree _pf_string_tree_from_tuple(PfStack *stack, int count);
Tree _pf_class_tree_from_tuple(PfStack *stack, int count);

Dir _pf_bit_dir_from_tuple(PfStack *stack, int count);
Dir _pf_byte_dir_from_tuple(PfStack *stack, int count);
Dir _pf_short_dir_from_tuple(PfStack *stack, int count);
Dir _pf_int_dir_from_tuple(PfStack *stack, int count);
Dir _pf_long_dir_from_tuple(PfStack *stack, int count);
Dir _pf_float_dir_from_tuple(PfStack *stack, int count);
Dir _pf_double_dir_from_tuple(PfStack *stack, int count);
Dir _pf_string_dir_from_tuple(PfStack *stack, int count);
Dir _pf_class_dir_from_tuple(PfStack *stack, int count);

void print(Var v);
