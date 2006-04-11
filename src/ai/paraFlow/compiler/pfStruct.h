/* pfStruct - declaration of structures used by ParaFlow compiler */

#ifndef PFSTRUCT
#define PFSTRUCT

struct pfCompile;   /* Represents compiler as a whole. */
struct pfModule;    /* A single ParaFlow module. */
struct pfParse;	    /* A node in the parse tree */
struct pfBaseType;  /* Information on a single type */
struct pfType;	    /* Type info, may be array of dir of ... of baseType */
struct pfScope;	    /* A name space for variables. */
struct pfVar;	    /* A single variable. */
struct pfToken;	    /* A tokenized string from a ParaFlow source file. */
struct pfBackEnd;   /* Interface to code generator. */
struct backEndString; /* Back end string representation. */
struct hash;	    /* A hash table. */
struct dyString;    /* Dynamically sized string */

#endif /* PFSTRUCT */

