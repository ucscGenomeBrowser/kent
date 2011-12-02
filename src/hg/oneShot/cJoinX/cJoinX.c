/* cJoinX - Experiment in C joining.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "joiner.h"


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

void cJoinX(char *j1, char *j2, char *j3)
/* cJoinX - Experiment in C joining.. */
{
struct joiner *joiner = joinerRead("../../makeDb/schema/all.joiner");
struct joinerDtf *a = joinerDtfFromDottedTriple(j1);
struct joinerDtf *b = joinerDtfFromDottedTriple(j2);
struct joinerDtf *c = joinerDtfFromDottedTriple(j3);
struct joinerPair *jpList = NULL, *jp;
struct joinerDtf *fieldList = NULL;
struct hash *visitedHash = hashNew(0);

slAddTail(&fieldList, a);
slAddTail(&fieldList, b);
slAddTail(&fieldList, c);

if (joinerDtfAllSameTable(fieldList))
    printf("All in same table, easy enough!\n");
else
    {
    jpList = joinerFindRouteThroughAll(joiner, fieldList);
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
if (argc != 4)
    usage();
cJoinX(argv[1], argv[2],argv[3]);
return 0;
}
