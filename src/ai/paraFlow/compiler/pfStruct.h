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
struct pfBackEnd;   /* Interface to code generator. */
struct backEndString; /* Back end string representation. */

#endif /* PFSTRUCT */

