/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

#ifndef PFPREAMBLE_H
#define PFPREAMBLE_H

#include <stdio.h>
#include <string.h>

union _pf_stack;
typedef union _pf_stack _pf_Stack;

struct _pf_object;
struct _pf_string;
struct _pf_array;
struct _pf_list;
struct _pf_tree;
struct _pf_dir;
struct _pf_var;

typedef struct _pf_object *_pf_Object;
typedef struct _pf_string *_pf_String;
typedef struct _pf_array *_pf_Array;
typedef struct _pf_list *_pf_List;
typedef struct _pf_tree *_pf_Tree;
typedef struct _pf_dir *_pf_Dir;
typedef struct _pf_var _pf_Var;
typedef void (*_pf_FunctionPt)(_pf_Stack *stack);

typedef unsigned char _pf_Bit;
typedef signed char _pf_Byte;
typedef short _pf_Short;
typedef int _pf_Int;
typedef long long _pf_Long;
typedef float _pf_Float;
typedef double _pf_Double;
typedef unsigned char _pf_Char;

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
    _pf_Char Char;
    _pf_String String;
    _pf_Object Obj;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    _pf_FunctionPt FunctionPt;
    void *v;
    };

struct _pf_var
    {
    union _pf_varless val;	/* Typeless value. */
    int typeId;			/* Index in run time type table. */
    };

union _pf_stack
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
    _pf_Char Char;
    _pf_String String;
    _pf_Object Obj;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    _pf_FunctionPt FunctionPt;
    void *v;
    _pf_Var Var;
    };

typedef void (*_pf_polyFunType)(_pf_Stack *stack);

struct _pf_localVarInfo
/* Information about a local variable for the run-time system. */
     {
     char *name;	/* Variable name */
     int type;		/* initially lti index, converted to typeId at startup */
     int offset;	/* Offset in local var data structure. Set at startup. */
     };

struct _pf_functionFixedInfo
/* Information on a function that doesn't change from
 * invocation to another.  This includes function name,
 * type, and (hopefully soon) names and types of all
 * the local variables. */
    {
    int classType; /* Local type id, converted to typeId at startup, or -1 */
    char *name;	   /* Function name. */
    int typeId;	   /* Index into types table describing function i/o etc. */
    int varCount;  /* Includes i/o parameters. */
    struct _pf_localVarInfo *vars; /* Info on each variable. */
    int dataSize;	/* Size of local data structure. */
    };

struct _pf_activation
/* Function activation record. */
    {
    struct _pf_activation *parent;		/* Caller's activation record. */
    struct _pf_functionFixedInfo *fixed;	/* Information valid for all calls. */
    void *data;			            /* Points to Data structure for locals. */
    };
extern struct _pf_activation *_pf_activation_stack;

struct _pf_iterator
/* Something to iterate over a collection */
    {
    	/* Get next item into *pItem.  Return FALSE if no more items.  */
    int (*next)(struct _pf_iterator *it, void *pItem, void *pKey);
    	/* Clean up iterator. */
    void (*cleanup)(struct _pf_iterator *it);
    void *data;	/* Iterator specific pointer data. */
    };

struct _pf_iterator _pf_list_iterator_init(_pf_List list);
struct _pf_iterator _pf_tree_iterator_init(_pf_Tree tree);
struct _pf_iterator _pf_dir_iterator_init(_pf_Dir dir);

struct _pf_moduleRuntime
/* Run time module information. */
    {
    struct _pf_moduleRuntime *next;	/* Next in list. */
    char *name;				/* Module name. */
    void (*cleanup)(struct _pf_moduleRuntime *module, _pf_Stack *stack);
    };
extern struct _pf_moduleRuntime *_pf_moduleList;

void *_pf_need_mem(int size);
/* Allocate memory, which is initialized to zero. */

void _pf_free_mem(void *pt);
/* Free memory. */

#include "../runtime/runType.h"
#include "../runtime/runErr.h"
#include "../runtime/object.h"
#include "../runtime/pfString.h"
#include "../runtime/dir.h"
#include "../runtime/array.h"
#include "../runtime/initVar.h"
#include "../runtime/paraRun.h"


#endif /* PFPREAMBLE_H */

