/* cJoinX - Experiment in C joining.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "joiner.h"

static char const rcsid[] = "$Id: cJoinX.c,v 1.2 2004/07/16 23:26:09 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cJoinX - Experiment in C joining.\n"
  "usage:\n"
  "   cJoinX db1.table.field db2.table.field ...\n" 
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct slName *commaSepToSlName(char *commaSep)
/* Convert comma-separated list of items to slName list. */
{
struct slName *list = NULL, *el;
char *s, *e;

s = commaSep;
while (s != NULL && s[0] != 0)
    {
    e = strchr(s, ',');
    if (e == NULL)
        {
	el = slNameNew(s);
	slAddHead(&list, el);
	break;
	}
    else
        {
	el = slNameNewN(s, e - s);
	slAddHead(&list, el);
	s = e+1;
	}
    }
slReverse(&list);
return list;
}

boolean sameTable(struct joinerDtf *a, struct joinerDtf *b)
/* Return TRUE if they are in the same database and table. */
{
return sameString(a->database, b->database) && sameString(a->table, b->table);
}

static struct joinerSet *identifierStack[4];	/* Help keep search from looping. */

boolean identifierInArray(struct joinerSet *identifier, 
	struct joinerSet **array, int count)
/* Return TRUE if identifier is in array. */
{
int i;
for (i=0; i<count; ++i)
    {
    if (identifier == array[i])
	{
        return TRUE;
	}
    }
return FALSE;
}

struct joinerPair *joinerPairDupe(struct joinerPair *jp)
/* Return duplicate (deep copy) of joinerPair. */
{
struct joinerPair *dupe;
AllocVar(dupe);
dupe->a = joinerDtfNew(jp->a->database, jp->a->table, jp->a->field);
dupe->b = joinerDtfNew(jp->b->database, jp->b->table, jp->b->field);
dupe->identifier = jp->identifier;
return dupe;
}

struct joinerPair *rFindRoute(struct joiner *joiner, 
	struct joinerDtf *a,  struct joinerDtf *b, int level, int maxLevel)
/* Find a path that connects the two fields if possible. */
{
struct joinerPair *jpList, *jp;
struct joinerPair *path = NULL;
char buf[256];
jpList = joinerRelate(joiner, a->database, a->table);
for (jp = jpList; jp != NULL; jp = jp->next)
    {
    if (sameTable(jp->b, b))
	{
	path = joinerPairDupe(jp);
	break;
	}
    }
if (path == NULL && level < maxLevel)
    {
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
	identifierStack[level] = jp->identifier;
	if (!identifierInArray(jp->identifier, identifierStack, level))
	    {
	    identifierStack[level] = jp->identifier;
	    path = rFindRoute(joiner, jp->b, b, level+1, maxLevel);
	    if (path != NULL)
		{
		struct joinerPair *jpDupe = joinerPairDupe(jp);
		slAddHead(&path, jpDupe);
		break;
		}
	    }
	}
    }
joinerPairFreeList(&jpList);
return path;
}

struct joinerPair *findRoute(struct joiner *joiner, 
	struct joinerDtf *a,  struct joinerDtf *b)
/* Find route between a and b. */
{
int i;
struct joinerPair *jpList = NULL;
for (i=1; i<ArraySize(identifierStack); ++i)
    {
    jpList = rFindRoute(joiner, a, b, 0, i);
    if (jpList != NULL)
        break;
    }
return jpList;
}



void cJoinX(char *j1, char *j2)
/* cJoinX - Experiment in C joining.. */
{
struct joiner *joiner = joinerRead("../../makeDb/schema/all.joiner");
struct joinerDtf *a = joinerDtfFromDottedTriple(j1);
struct joinerDtf *b = joinerDtfFromDottedTriple(j2);
struct joinerPair *jpList = NULL, *jp;
struct hash *visitedHash = hashNew(0);
if (sameTable(a,b))
    printf("All in same table, easy enough!\n");
else
    {
    jpList = findRoute(joiner, a, b);
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	printf("%s.%s.%s -> %s.%s.%s\n", 
	    jp->a->database, jp->a->table, jp->a->field,
	    jp->b->database, jp->b->table, jp->b->field);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cJoinX(argv[1], argv[2]);
return 0;
}
