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

struct pfCompile
/* Paraflow compiler */
    {
    struct pfCompile *next;
    char *baseFile;		/* First file to compile. */
    struct hash *modules;	/* Full path to all modules. */
    struct pfTokenizer *tkz;	/* Tokenizer. */
    struct hash *reservedWords;	/* Reserved words, can't be used for type or symbols */
    struct pfScope *scope;	/* Outermost scope - for built in types and symbols */

    /* Some called out types parser needs to know about. */
    struct pfBaseType *varType;		/* Base tyhpe for all variables/functions */
    struct pfBaseType *streamType;	/* Ancestor for string, maybe file */
    struct pfBaseType *numType;		/* Ancestor for float/int etc. */
    struct pfBaseType *collectionType;	/* Ancestor for tree, list etc. */
    struct pfBaseType *tupleType;	/* Type for tuples */
    struct pfBaseType *functionType;	/* Ancestor of to/para/flow */
    struct pfBaseType *toType;		/* Type for to functions */
    struct pfBaseType *paraType;	/* Type for para declarations */
    struct pfBaseType *flowType;	/* Type for flow declarations */
    struct pfBaseType *classType;	/* Type for class declarations. */
    };

struct pfCompile *pfCompileNew(char *fileName);
/* Create new pfCompile.  */


#endif /* PFCOMPILE_H */

