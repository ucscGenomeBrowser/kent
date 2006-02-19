/* Determine best positions to split input to
 * a multiple alignment -- places where the MAF
 * is likely to split; e.g. at gaps in the reference.
 * For use with "mafSplit"  */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "sqlNum.h"
#include "hdb.h"

static char const rcsid[] = "$Id: mafSplitPos.c,v 1.1 2006/02/19 05:35:29 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSplitPos - Pick positions to split multiple alignment input files\n"
  "usage:\n"
  "   mafSplitPos database size(Mbp)\n"
  "options:\n"
  "   -chrom=chrN  Restrict to one chromosome\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

char *chrom = NULL;

int nextGapPos(char *chrom, int desiredPos, struct sqlConnection *conn)
{
/* Find next gap on the chrom and return midpoint */
char where[256];
struct sqlResult *sr;
char **row;
int pos = -1;
int start, end;

safef(where, sizeof where, 
    "chromStart >= %d \
    order by chromStart limit 1",
        desiredPos);
sr = hExtendedChromQuery(conn, "gap", chrom, where, FALSE,
        "chromStart, chromEnd", NULL);
if (row = sqlNextRow(sr))
    {
    start = sqlSigned(row[0]);
    end = sqlSigned(row[1]);
    pos = start + (end - start)/2;
    }
sqlFreeResult(&sr);
return pos;
}

int nextRepeatPos(char *chrom, int desiredPos, struct sqlConnection *conn)
/* Find next 0% diverged repeat on the chrom and return midpoint */
{
char where[256];
struct sqlResult *sr;
char **row;
int pos = -1;
int start, end;

safef(where, sizeof where, 
    "genoStart >= %d and \
    milliDiv=0 and \
    repClass<>'Simple_repeat' and repClass<>'Low_complexity' and \
    genoEnd-genoStart>100 order by genoStart limit 1",
        desiredPos);
sr = hExtendedChromQuery(conn, "rmsk", chrom, where, FALSE,
        "genoStart, genoEnd", NULL);
if (row = sqlNextRow(sr))
    {
    start = sqlSigned(row[0]);
    end = sqlSigned(row[1]);
    pos = start + (end - start)/2;
    }
sqlFreeResult(&sr);
return pos;
}

void chromSplits(char *chrom, int chromSize, int splitSize, 
                        struct sqlConnection *conn)
/* determine positions on a single chromosome */
{
int desiredPos = splitSize;
int splitPos = -1;
int gapPos = -1;
int prevPos = 0;

verbose(1, "    %s\t%d\n", chrom, chromSize);
while (desiredPos < chromSize)
    {
    splitPos = gapPos = nextGapPos(chrom, desiredPos, conn);
    if (splitPos < 0 || splitPos > desiredPos + (desiredPos/10))
        {
        /* next gap is further out than 10% of desired position,
         * so look for the next new repeat */
        splitPos = nextRepeatPos(chrom, desiredPos, conn);
        if (splitPos < 0 || splitPos > desiredPos + (desiredPos/10))
            splitPos = min(splitPos, gapPos);
        }
    if (splitPos < 0)
        /* no places to split on this chrom */
        return;
    printf("%s\t%d\t%d\n", chrom, splitPos, splitPos+1), 
    verbose(2, "      %s\t%d\n", splitPos == gapPos ? "gap" : "repeat", 
           (splitPos - prevPos)/1000000);
    prevPos = splitPos;
    desiredPos += splitSize;
    }
}

void mafSplitPos(char *database, char *size)
/* Pick best positions for split close to size.
 * Use middle of a gap as preferred site.
 * If not gaps are in range, use recent repeats (0% diverged) */
{
int splitSize = 0;
int chromSize = 0;
struct hash *chromHash;
struct hashCookie hc;
struct hashEl *hel;
struct sqlConnection *conn = sqlConnect(database);

splitSize = atoi(size) * 1000000;
verbose(1, "Finding split positions for %s at ~%d Mbp intervals\n", 
                database, splitSize);
if (chrom == NULL)
    {
    chromHash = hChromSizeHash(database);
    }
else
    {
    chromHash = hashNew(6);
    hashAddInt(chromHash, chrom, hChromSize(chrom));
    }
/*chrSlNameCmp(1,2);
chrNameCmp(1,2);
*/
conn = sqlConnect(database);
hc = hashFirst(chromHash);
while (hel = hashNext(&hc))
    {
    chrom = hel->name;
    chromSize = (int)hel->val;
    chromSplits(chrom, chromSize, splitSize, conn);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
chrom = optionVal("chrom", NULL);
if (argc != 3)
    usage();
mafSplitPos(argv[1], argv[2]);
return 0;
}
