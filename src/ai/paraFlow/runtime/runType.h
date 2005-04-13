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
    pf_stString,	/* A text string. */
    pf_stArray,		/* A dynamically sized array of something. */
    pf_stList,		/* A doubly-linked list (fast add head/tail remove). */
    pf_stDir,		/* A directory (container keyed by strings). */
    pf_stTree,		/* A binary tree keyed by floating point numbers. */
    pf_stVar,		/* Variable type. Actual type determined at run time. */
    pf_stClass,		/* A type containing multiple fields, and/or methods. */
    pf_stTo,		/* A regular function declaration. */
    pf_stPara,		/* A parallelizable function declaration. */
    pf_stFlow,		/* A pure flow (no latch required) function. */
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
    };

struct _pf_base_info
/* Information to help initialize base classes */
    {
    int id;		/* Unique id */
    char *name;		/* Name in scope:class format. */
    int parentId;	/* Id of parent class. */
    char needsCleanup;	/* Does this type need cleanup? */
    int size;		/* Type size. */
    };

struct _pf_type_info
/* Information on how base types are composed to form types */
    {
    int id;		/* Unique id */
    char *parenCode;	/* type tree encoded as numbers,commas,parens */
    };

struct _pf_field_info
/* Information on class fields. */
    {
    int classId;	/* Points to _pf_base_info.id. */
    char *typeValList;  /* comma sep. list of type:val (type is pf_type_info.id) */
    };

void _pf_init_types( struct _pf_base_info *baseInfo, int baseCount,
		     struct _pf_type_info *typeInfo, int typeCount,
		     struct _pf_field_info *fieldInfo, int fieldCount);
/* Build up run-time type information from initialization. */

struct _pf_type **_pf_type_table;	
/* Table of all types we'll see during run time. Created by _pf_init_types. */

#endif /* RUNTYPE_H */

