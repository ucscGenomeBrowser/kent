/* Activate - stuff to manage activation records.
 * We keep a stack of these that gets pushed at the
 * start of each function and popped at the end.
 * This lets us clean up local vars when there is
 * an exception, and also allows us to print a nice
 * stack trace. */

#ifndef CTAR_H
#define CTAR_H

struct ctar
/* A compile-time activation record - one exists for each function. */
    {
    struct ctar *next;	/* Next in list. */
    char *name;			/* Function name. */
    char *cName;		/* Function name in C. */
    int id;			/* Unique numeric ID. */
    struct pfParse *pp;		/* Function declaration in parse tree. */
    struct slRef *varRefList;	/* A reference to each local var.  Val is type pfVar. */
    };

struct ctar *ctarOnFunction(struct pfParse *pp);
/* Create a compile time activation record for function. Hangs it on pp->var->ctar
 * and returns it. */

void ctarFree(struct ctar **pCtar);
/* Free up memory associated with activation record. */

void ctarCodeFixedParts(struct ctar *ctarList, struct pfCompile *pfc, FILE *f);
/* Write parts of function activation record that don't change from invocation
 * to invocation of function. */

void ctarCodeStartupCall(struct ctar *ctarList, struct pfCompile *pfc, FILE *f);
/* Code call to initialize types and offsets of fixed run time activation records. */

void ctarCodeLocalStruct(struct ctar *ctar, struct pfCompile *pfc, FILE *f,
	char *refName);
/* Write out a structure that has all of the local variables for a function. 
 * If refName is zero then make a pointer of this type equal to refName,
 * otherwise make an actual instand initialized to zero. */

void ctarCodePush(struct ctar *ctar, struct pfCompile *pfc, FILE *f);
/* Write out code to define run-time activation structure and push
 * it on activation stack. */

void ctarCodePop(struct ctar *ctar, struct pfCompile *pfc, FILE *f);
/* Write out code to pop activation structure off of stack. */

#endif /* CTAR_H */

