/* fixConrad - Assign unique identifer to Conrad input. */

#include "common.h"

#include "chromInfo.h"
#include "hdb.h"
#include "linefile.h"
#include "jksql.h"

static char const rcsid[] = "$Id: fixConrad.c,v 1.1 2006/01/23 19:07:08 heather Exp $";

static char *database = NULL;
static struct hash *chromHash = NULL;
int countByChrom[25];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixConrad - Assign unique identifier to Conrad input\n"
  "usage:\n"
  "  fixConrad database file\n");
}


/* Copied from hgLoadWiggle. */
static struct hash *loadAllChromInfo()
/* Load up all chromosome infos. */
{
struct chromInfo *el;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;

ret = newHash(0);

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}

/* also copied from hgLoadWiggle. */
static unsigned getChromSize(char *chrom)
/* Return size of chrom.  */
{
struct hashEl *el = hashLookup(chromHash,chrom);

if (el == NULL)
    errAbort("Couldn't find size of chrom %s", chrom);
return *(unsigned *)el->val;
}   



void fixConrad(char *filename)
/* fixConrad - read file. */
{
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
int lineSize;
char *row[5];
int wordCount;
int count = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopByWhite(line, row, ArraySize(row));
    count++;
    printf("conrad%d %s %s %s %s %s\n", 
            count, row[0], row[1], row[2], row[3], row[4]);
    }
}

int main(int argc, char *argv[])
{

if (argc != 3)
    usage();

database = cloneString(argv[1]);
hSetDb(database);

chromHash = loadAllChromInfo();

fixConrad(argv[2]);

return 0;
}
