/* pfType - ParaFlow type heirarchy */
#ifndef PFTYPE_H
#define PFTYPE_H

#ifndef PFSCOPE_H
#include "pfScope.h"
#endif

struct pfBaseType
/* A type, may have parents in object heirarchy, and
 * be composed of multiple elements. */
    {
    struct pfBaseType *next;		/* Sibling in class heirarchy */
    struct pfBaseType *children;	/* Derived classes. */
    struct pfBaseType *parent;		/* Pointer to parent class if any */
    char *name;			/* Type name.  Allocated in hash. */
    struct pfScope *scope;	/* The scope this class lives in */
    bool isCollection;		/* TRUE if it's a collection type */
    };

struct pfBaseType *pfBaseTypeNew(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parent);
/* Create new base type. */

struct pfType
/* A type tree that represents typed tuples and collections. 
 * The class heirarchy is not here, but instead in pfBaseType. */
    {
    struct pfType *next;	/* Next sibling. */
    struct pfType *children;	/* Children. */
    struct pfBaseType *base;	/* Collected type of this node in type tree. */
    char *fieldName;		/* Field name associated with this node. */
    struct pfParse *init;	/* Initialization if any. */
    bool isTuple;		/* True if it's a tuple. */
    bool isFunction;		/* True if it's a function. */
    bool isModule;		/* True if it's a module. */
    };

struct pfType *pfTypeNew(struct pfBaseType *base);
/* Create new high level type object from base type.  Pass in NULL to
 * create a tuple. */

void pfTypeDump(struct pfType *ty, FILE *f);
/* Write out info on ty to file.  (No newlines written) */

#endif /* PFTYPE_H */
