
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
    struct _pf_base_type *base;	/* Base type of this node in type tree. */
    char *name;			/* Parameter or field name. May be NULL. */
    };

struct _pf_base
/* This is a base type - either a built in type like int or string, or
 * a class. */
    {
    struct _pf_base *parent;	/* Parent in class hierarchy */
    char *name;			/* Type name */
    int rootType;		/* class? int? */
    struct _pf_type *fields;	/* Elements of class. */
    };

struct _pf_base_info
/* Information to help initialize base classes */
    {
    int id;		/* Unique id */
    char *name;		/* Name in scope:class format. */
    int parentId;	/* Id of parent class. */
    };

struct _pf_type_info
/* Information on how base types are composed to form types */
    {
    int id;		/* Unique id */
    char *parenCode;	/* type tree encoded as numbers,commas,parens */
    }

struct _pf_field_info
/* Information on class fields. */
    {
    int classId;	/* Points to _pf_base_info.id. */
    char *typeValList;  /* comma sep. list of type:val (type is pf_type_info.id) */
    };
