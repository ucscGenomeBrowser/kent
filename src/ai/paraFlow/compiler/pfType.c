/* pfType - ParaFlow type heirarchy */

#include "common.h"
#include "hash.h"
#include "pfType.h"

struct pfBaseType *pfBaseTypeNew(struct pfScope *scope, char *name, 
	boolean isCollection, struct pfBaseType *parent)
/* Create new base type. */
{
struct pfBaseType *base;
AllocVar(base);
if (parent != NULL)
    {
    base->parent = parent;
    slAddHead(&parent->children, base);
    }
base->name = cloneString(name);
base->scope = scope;
base->isCollection = isCollection;
return base;
}

struct pfType *pfTypeNew(struct pfBaseType *base)
/* Create new high level type object */
{
struct pfType *ty;
AllocVar(ty);
ty->base = base;
return ty;
}

void pfTypeDump(struct pfType *ty, FILE *f)
/* Write out info on ty to file.  (No newlines written) */
{
if (ty == NULL)
    fprintf(f, "void");
else if (ty->isTuple)
    {
    fprintf(f, "(");
    for (ty = ty->children; ty != NULL; ty = ty->next)
        {
	pfTypeDump(ty, f);
	if (ty->next != NULL)
	    fprintf(f, " ");
	}
    fprintf(f, ")");
    }
else if (ty->isFunction)
    {
    fprintf(f, "%s ", ty->base->name);
    pfTypeDump(ty->children, f);
    fprintf(f, " into ");
    pfTypeDump(ty->children->next, f);
    }
else if (ty->isModule)
    {
    fprintf(f, ".module.");
    }
else
    {
    fprintf(f, "%s", ty->base->name);
    ty = ty->children;
    if (ty != NULL)
        {
	fprintf(f, " of ");
	pfTypeDump(ty, f);
	}
    }
}

