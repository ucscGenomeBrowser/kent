/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

#include "../runtime/runType.h"
#include "../runtime/object.h"

struct _pf_string;
struct _pf_array;
struct _pf_list;
struct _pf_tree;
struct _pf_dir;
struct _pf_var;

// typedef struct string *String;
typedef char *_pf_String;
typedef struct _pf_array *_pf_Array;
typedef struct _pf_list *_pf_List;
typedef struct _pf_tree *_pf_Tree;
typedef struct _pf_dir *_pf_Dir;
typedef struct _pf_var _pf_Var;

typedef char _pf_Bit;
typedef unsigned char _pf_Byte;
typedef short _pf_Short;
typedef int _pf_Int;
typedef long long _pf_Long;
typedef float _pf_Float;
typedef double _pf_Double;

union _pf_varless
/* All the types a variable can take except the
 * typeless 'var' type. */
    {
    _pf_Bit Bit;
    _pf_Byte Byte;
    _pf_Short Short;
    _pf_Int Int;
    _pf_Long Long;
    _pf_Float Float;
    _pf_Double Double;
    _pf_String String;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    void *v;
    };

struct _pf_var
    {
    union _pf_varless val;	/* Typeless value. */
    int typeId;			/* Index in run time type table. */
    };

union pfStack
/* All the types a variable can take including the
 * typeless 'var' type. */
    {
    _pf_Bit Bit;
    _pf_Byte Byte;
    _pf_Short Short;
    _pf_Int Int;
    _pf_Long Long;
    _pf_Float Float;
    _pf_Double Double;
    _pf_String String;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    void *v;
    _pf_Var Var;
    };
typedef union pfStack PfStack;

struct _pf_iterator
/* Something to iterate over a collection */
    {
    int (*next)(struct _pf_iterator *it, void *pItem);
    	/* Get next item into *pItem.  Return FALSE if no more items.  */
    int i;	/* Iterator specific integer data. */
    void *pt;	/* Iterator specific pointer data. */
    };
typedef struct _pf_iterator Pf_iterator;

Pf_iterator _pf_list_iterator_init(_pf_List list);
Pf_iterator _pf_tree_iterator_init(_pf_Tree tree);
Pf_iterator _pf_dir_iterator_init(_pf_Dir dir);

#include "../runtime/initVar.h"

void print(PfStack *stack);
