/* compilerError - demonstrate cancer1 compiler problem. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "compilerError - demonstrate cancer1 compiler problem\n"
  "usage:\n"
  "   compilerError XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void compilerError()
/* compilerError - demonstrate cancer1 compiler problem. */
{
struct hash *hash =newHash(1);
char *name ="jing";
int value = 1;
hashAdd(hash, name, (void *) value );

struct hashEl *el;
el = hashLookup(hash,name);
int retVal = (int) el->val;
printf("%d\n",retVal);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
compilerError();
return 0;
}
