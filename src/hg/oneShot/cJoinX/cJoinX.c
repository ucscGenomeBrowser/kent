/* cJoinX - Experiment in C joining.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: cJoinX.c,v 1.1 2004/07/16 19:03:28 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cJoinX - Experiment in C joining.\n"
  "usage:\n"
  "   cJoinX db1.table.field db2.table.field fieldList\n"
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


void cJoinX(char *j1, char *j2, char *fields)
/* cJoinX - Experiment in C joining.. */
{
struct slName *field, *fieldList = commaSepToSlName(fields);
uglyf("%s %s\n", j1, j2);
for (field = fieldList; field != NULL; field = field->next)
    uglyf(" %s\n", field->name);
slFreeList(&fieldList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
cJoinX(argv[1], argv[2], argv[3]);
return 0;
}
