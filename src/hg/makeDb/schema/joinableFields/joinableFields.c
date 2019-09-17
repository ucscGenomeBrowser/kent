/* joinableFields - Return list of good join targets for a table. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "joiner.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinableFields - Return list of good join targets for a table\n"
  "usage:\n"
  "   joinableFields all.joiner database table\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void joinableFields(char *joinerFile, char *database, char *table)
/* joinableFields - Return list of good join targets for a table. */
{
struct joiner *joiner = joinerRead(joinerFile);
struct joinerPair *jpList, *jp;

jpList = joinerRelate(joiner, database, table, NULL);
for (jp = jpList; jp != NULL; jp = jp->next)
    {
    printf("%s\t%s\t%s\t%s\t%s\t%s\n", 
    	jp->a->database, jp->a->table, jp->a->field,
    	jp->b->database, jp->b->table, jp->b->field);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
joinableFields(argv[1], argv[2], argv[3]);
return 0;
}
