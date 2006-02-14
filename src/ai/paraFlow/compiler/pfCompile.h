/* pfCompile - High level structures and routines for paraFlow compiler. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

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
    struct pfToken *tokList;	/* All modules's tokens. */
    struct pfParse *pp;		/* Parse tree for module. */
    struct pfSource *source;	/* Source file associated with module. */
    boolean isPfh;		/* True if it's just a .pfh rather than .pf */
    };

struct pfCompile
/* Paraflow compiler */
    {
    struct pfCompile *next;
    char *baseDir;		/* Base directory. */
    struct hash *moduleHash;	/* Module hash.  pfModule valued, name keyed. */
    struct pfModule *moduleList;/* List of all modules. */
    boolean isSys;		/* True if compiling built-in module. */
    struct pfTokenizer *tkz;	/* Tokenizer. */
    struct hash *reservedWords;	/* Reserved words, can't be used for type or symbols */
    struct pfScope *scope;	/* Outermost scope - for built in types and symbols */
    struct pfScope *scopeList;	/* List of all scopes. */
    struct hash *runTypeHash;	/* Hash of run time types for code generator */
    struct hash *moduleTypeHash;/* Hash of run time types for just one module. */

    /* Some called out types parser needs to know about. */
    struct pfBaseType *moduleType;	/* Base type for separate compilation units. */
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

    struct pfBaseType *bitType;
    struct pfBaseType *byteType;
    struct pfBaseType *shortType;
    struct pfBaseType *intType;
    struct pfBaseType *longType;
    struct pfBaseType *floatType;
    struct pfBaseType *doubleType;
    struct pfBaseType *stringType;

    struct pfBaseType *arrayType;
    struct pfBaseType *listType;
    struct pfBaseType *treeType;
    struct pfBaseType *dirType;

    struct pfType *stringFullType;	/* String type info including .size etc. */
    struct pfType *arrayFullType;	/* Array type info including .size etc. */
    struct pfType *elTypeFullType;	/* Generic element of collection type. */
    struct pfType *intFullType;		/* This is handy to have around. */
    struct pfType *longFullType;	/* This is handy to have around. */
    };


struct pfCompile *pfCompileNew();
/* Create new pfCompile.  */

char *fetchBuiltInCode();
/* Return a string with the built in stuff. */

char *fetchStringDef();
/* Return a string with definition of string. */

/* --- utility functions --- */
void printEscapedString(FILE *f, char *s);
/* Print string in such a way that C can use it. */

#endif /* PFCOMPILE_H */

