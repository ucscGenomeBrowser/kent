/* codedType - Types in form that can be used at run time. */

#ifndef CODEDTYPE_H
#define CODEDTYPE_H

#ifndef PFSTRUCT_H
#include "pfStruct.h"
#endif 

struct codedType
/* Composite output type. */
    {
    char *code;		/* number:name and parenthesis code. */
    int id;		/* Unique id. */
    struct pfType *type;	/* Unpacked version of type */
    };

int codedTypeId(struct pfCompile *pfc, struct pfType *type);
/* Return run time type ID associated with type */

struct hash *codedTypesToC(struct pfCompile *pfc, 
	struct pfParse *program, FILE *f);
/* Traverse parse tree and encode all types referenced in it.
 * Also print out the types in C structures that the runtime
 * system can interpret. */

struct hash *codedTypesToBackEnd(struct pfCompile *pfc, 
	struct pfParse *program, struct backEndString **pStrings, FILE *f);
/* Traverse parse tree and encode all types referenced in it.
 * Save these out in assembly language data structures for runtime. */

void encodeType(struct pfType *type, struct dyString *dy);
/* Encode type into dy. */

#endif /* CODEDTYPE_H */
