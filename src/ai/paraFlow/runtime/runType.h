/* runType - run time type system for paraFlow. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef RUNTYPE_H
#define RUNTYPE_H

enum _pf_single_type
/* Categorize a type at a fairly high level */
    {
    pf_stNone,		/* Should never be used */
    pf_stBit,		/* Hopefully a bit someday, now a byte. */
    pf_stByte,		/* A 8-bit unsigned number. */
    pf_stShort,		/* A 16-bit signed number. */
    pf_stInt,		/* A 32-bit signed number. */
    pf_stLong,		/* A 64-bit signed number. */
    pf_stFloat,		/* Single precision floating point. */
    pf_stDouble,	/* Double precision floating point. */
    pf_stChar,		/* A character. */
    pf_stString,	/* A text string. */
    pf_stArray,		/* A dynamically sized array of something. */
    pf_stList,		/* A doubly-linked list (fast add head/tail remove). */
    pf_stDir,		/* A directory (container keyed by strings). */
    pf_stTree,		/* A binary tree keyed by floating point numbers. */
    pf_stVar,		/* Variable type. Actual type determined at run time. */
    pf_stClass,		/* A type containing multiple fields, and/or methods. */
    pf_stTo,		/* A regular function declaration. */
    pf_stFlow,		/* A pure flow (no side effects) function. */
    pf_stToPt,		/* A pointer to a regular function */
    pf_stFlowPt,	/* A pointer to a flow function */
    };

struct _pf_type
/* Type tree structure.  Can have such things as:
 *     to
 *       input-tuple
 *          input1
 *          input2
 *       output-tuple
 *          output1
 * Which represents a function that takes two inputs and has one
 * output.   Also could have:
 *     array
 *       list
 *         string
 * which represents an array of list of string.
 */
    {
    struct _pf_type *next;	/* Sibling in type tree. */
    struct _pf_type *children;	/* Children in type tree. */
    struct _pf_base *base;	/* Base type of this node in type tree. */
    char *name;			/* Parameter or field name. May be NULL. */
    int offset;			/* Offset of field from start of structure. */
    int typeId;			/* Id of this type (index in type table). */
    };

struct _pf_base
/* This is a base type - either a built in type like int or string, or
 * a class. */
    {
    struct _pf_base *parent;	/* Parent in class hierarchy */
    int scope;			/* Scope number defined in. (1 for built-in, 2 for user high level) */
    char *name;			/* Type name */
    int singleType;		/* class? int? */
    struct _pf_type *fields;	/* Elements of class. */
    char needsCleanup;	/* Does this type need cleanup? */
    int size;		/* Type size - just size of pointer for objects. */
    int objSize;	/* Object size for objects. */
    int alignment;	/* This type likes to be aligned on an even multiple of this in structures. */
    int aliAdd;		/* Computational aid for alignment.  alignedStart = ((start+aliAdd)&aliMask)*/
    int aliMask;	/* Computational aid for alignment. */
    _pf_polyFunType *polyTable;	/* Polymorphic function table. */
    };

struct _pf_base_info
/* Information to help initialize base classes */
    {
    int id;		/* Unique id */
    char *name;		/* Name in scope:class format. */
    int parentId;	/* Id of parent class. */
    int needsCleanup;	/* Does this type need cleanup? */
    int size;		/* Type size. */
    };

struct _pf_type_info
/* Information on how base types are composed to form types */
    {
    int id;		/* Unique id */
    char *parenCode;	/* type tree encoded as numbers,commas,parens */
    };

struct _pf_local_type_info
/* Information on just the local types */
    {
    int id;		/* ID - just unique in this module. */
    char *parenCode;	/* Type tree encoded as className,commas,parents. */
    };

struct _pf_field_info
/* Information on class fields. */
    {
    int classId;	/* Points to _pf_base_info.id. */
    char *typeValList;  /* comma sep. list of type:val (type is pf_type_info.id) */
    };

struct _pf_poly_info
/* Information on polymorphic functions. */
    {
    char *className;		/* Name of class. */
    _pf_polyFunType *polyTable;	/* Polymorphic function table. */
    };

struct _pf_module_info
/* Information on a module. */
    {
    char *name;		/* Module name. */
    struct _pf_local_type_info *lti;	/* Information on types pushed on stack. */
    struct _pf_poly_info *polyInfo;	/* Information on polymorphic methods. */
    void (*entry)(_pf_Stack *_pf_stack);/* Where execution starts for this module. */
    };

void _pf_init_types( struct _pf_base_info *baseInfo, int baseCount,
		     struct _pf_type_info *typeInfo, int typeCount,
		     struct _pf_field_info *fieldInfo, int fieldCount,
		     struct _pf_module_info *moduleInfo, int moduleCount);
/* Build up run-time type information from initialization.  Called
 * at startup for whole program. */

void _pf_rtar_init_tables(struct _pf_functionFixedInfo **table,
	int tableSize, struct _pf_local_type_info *lti);
/* Convert local type IDs to global type IDs, and fill in local
 * var field offset in fixed part of run-time activation records. 
 * Called at startup of each module. */

extern struct _pf_type **_pf_type_table;	
/* Table of all types we'll see during run time. Created by _pf_init_types. */

int _pf_find_int_type_id();
/* Return int type ID. */

int _pf_find_string_type_id();
/* Return string type ID. */

int _pf_find_error_type_id();
/* Return id of error */

int _pf_find_serious_error_type_id();
/* Return id of seriousError. */

_pf_Bit _pf_check_types(int destType, int sourceType);
/* Check that sourceType can be converted to destType. */

_pf_Bit _pf_base_is_ancestor(struct _pf_base *base, 
	struct _pf_base *ancestor);
/* Return TRUE if ancestor really is same as base or one of bases's
 * ancestors. */

#endif /* RUNTYPE_H */

