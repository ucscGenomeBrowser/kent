/* pfCompile - High level structures and routines for paraFlow compiler. */
/* Copyright 2005-2006 Jim Kent.  All rights reserved. */

#ifndef PFCOMPILE_H
#define PFCOMPILE_H

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef PFTOKEN_H
#include "pfToken.h"
#endif

#ifndef PFSCOPE_H
#include "pfScope.h"
#endif

#ifndef PFTYPE_H
#include "pfType.h"
#endif


struct pfModule
/* Info on a module. */
    {
    struct pfModule *next;	/* Next module in list. */
    char *name;			/* Module symbolic name. */
    char *fileName;		/* File name of module. */
    struct pfToken *tokList;	/* All modules's tokens. */
    struct pfParse *pp;		/* Parse tree for module. */
    struct pfSource *source;	/* Source file associated with module. */
    struct pfScope *scope;	/* Scope associated with module. */
    boolean isPfh;		/* True if it's just a .pfh rather than .pf */
    };

struct pfCompile
/* Paraflow compiler. */
    {
    struct pfCompile *next;
    char *baseDir;		/* paraFlow source file base dir. */
    char *cIncludeDir;		/* Directory where pfPreamble.h lives. */
    char *runtimeLib;		/* Location of runtime lib. */
    char *jkwebLib;		/* Location of jkweb.a lib. */
    struct slName *paraLibPath; /* Where paraFlow library modules live. */
    struct hash *cfgHash;	/* Unparsed config hash. */
    struct hash *moduleHash;	/* Module hash.  pfModule valued, name keyed. */
    struct pfModule *moduleList;/* List of all modules. */
    bool	isSys;		/* True if compiling a built-in module. */
    bool	isFunc;		/* True if compiling a function. */
    bool	isPfh;		/* Tree if compiling a pfh file. */
    bool     codingPara;	/* Tree if generating code for para. */
    struct pfTokenizer *tkz;	/* Tokenizer. */
    struct hash *reservedWords;	/* Reserved words, can't be used for user data */
    struct pfScope *scope;	/* Outermost scope - for built in symbols */
    struct slRef *scopeRefList;	/* List of refences to all scopes. */
    struct hash *runTypeHash;	/* Hash of run time types for code generator */
    struct hash *moduleTypeHash;/* Hash of run time types for just one module. */

    /* Some called out types parser needs to know about. */
    struct pfBaseType *moduleType;	/* Base type for separate source files. */
    struct pfBaseType *varType;		/* Base type for all variables/functions */
    struct pfBaseType *nilType;		/* Object/string with no value. */
    struct pfBaseType *keyValType;	/* Used for tree/dir initializations */
    struct pfBaseType *streamType;	/* Ancestor for string, maybe file */
    struct pfBaseType *numType;		/* Ancestor for float/int etc. */
    struct pfBaseType *collectionType;	/* Ancestor for tree, list etc. */
    struct pfBaseType *tupleType;	/* Type for tuples */
    struct pfBaseType *indexRangeType;	/* Type for index ranges. */
    struct pfBaseType *functionType;	/* Ancestor of to/para/flow */
    struct pfBaseType *toType;		/* Type for to functions */
    struct pfBaseType *flowType;	/* Type for flow declarations */
    struct pfBaseType *operatorType;	/* Type for operator declarations */
    struct pfBaseType *classType;	/* Type for class declarations. */
    struct pfBaseType *interfaceType;	/* Type for interface declarations. */
    struct pfBaseType *toPtType;	/* "to" function pointer type. */
    struct pfBaseType *flowPtType;	/* "flow" function pointer type. */

    /* Base types for the simple types that need no dynamic memory. */
    struct pfBaseType *bitType;		/* A single bit.  0 or 1 */
    struct pfBaseType *byteType;	/* A signed 8 bit quantity. */
    struct pfBaseType *shortType;	/* Signed 16 bit number. */
    struct pfBaseType *intType;		/* Signed 32 bit number. */
    struct pfBaseType *longType;	/* Signed 64 bit number. */
    struct pfBaseType *floatType;	/* Single precision floating point */
    struct pfBaseType *doubleType;	/* Double precision floating point */
    struct pfBaseType *charType;	/* An 8-bit char.  */
    //struct pfBaseType *unicharType;	/* Will implement 32 bit unicode here */

    /* Base types for the built-in types that do need dynamic memory */
    struct pfBaseType *stringType;	        /* A string.  See also unistring. */
    struct pfBaseType *dyStringType;	/* A dynamic string (can write to it) */
    //struct pfBaseType *unistringType;	/* Will implement 32 bit unicode here */
    struct pfBaseType *arrayType;	/* Arrays of another type. */
    struct pfBaseType *dirType;		/* Hash tables - arrays indexed by string */
    // struct pfBaseType *listType;	/* A doubly-linked list someday? */
    // struct pfBaseType *treeType;	/* A binary tree someday? */

    /* It's handy to have simple instances of the higher order types built
     * on top of the base types here.  The higher order types can include
     * things like array of dir of string.  These are all just simple ones. */
    struct pfType *bitFullType;	
    struct pfType *byteFullType;
    struct pfType *charFullType;
    struct pfType *shortFullType;
    struct pfType *intFullType;
    struct pfType *longFullType;
    struct pfType *floatFullType;
    struct pfType *doubleFullType;

    /* These next ones are initialized after compiling the <builtin> module */
    struct pfType *strFullType;
    struct pfType *dyStrFullType;
    struct pfType *arrayFullType;   /* Array type info including .size etc. */
    struct pfType *dirFullType;	    /* Dir full type including .keys() */
    struct pfType *elTypeFullType;  /* Generic element of collection type. */
    struct pfType *moduleFullType;
    struct pfType *seriousErrorFullType;
    struct pfBaseType *seriousErrorType;

    /* Here's some stuff used by assembly code generator.  These are all 
     * initialized freshly for each module.*/
    struct pfBackEnd *backEnd;	/* Specific back end. */
    int isxLabelMaker;		/* Isix label generator */
    int tempLabelMaker;		/* Temp var label generator */
    struct hash *constStringHash; /* Look up string constants here */
    };

enum pfAccessType
/* Access type for variables, members, and functions */
    {
    paUsual=0,	/* Nothing special. */
    paGlobal,
    paLocal,
    paStatic,
    paReadable,
    paWritable,
    };
char *pfAccessTypeAsString(enum pfAccessType pa);
/* Return string representation of access qualifier. */

struct pfCompile *pfCompileNew();
/* Create new pfCompile.  */

char *fetchBuiltInCode();
/* Return a string with the built in stuff. */

char *fetchStringDef();
/* Return a string with definition of string. */

char *mangledModuleName(char *modName);
/* Return mangled version of module name that hopefully someday
 * will be unique across directories, but for now is just the last
 * bit. */

boolean pfBaseTypeIsPassedByValue(struct pfCompile *pfc, 
	struct pfBaseType *base);
/* Return TRUE if this type is passed by value.  (Strings
 * since they are read-only are considered to be passed by
 * value. */

/* --- utility functions --- */
void printEscapedString(FILE *f, char *s);
/* Print string in such a way that C can use it. */

char *replaceSuffix(char *path, char *oldSuffix, char *newSuffix);
/* Return a string that's a copy of path, but with old suffix
 * replaced by new suffix. */

#endif /* PFCOMPILE_H */

