/* Determine best positions to split input to
 * a multiple alignment -- places where the MAF
 * is likely to split; e.g. at gaps in the reference.
 * For use with "mafSplit"  */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "jksql.h"
#include "sqlNum.h"
#include "hdb.h"

static char const rcsid[] = "$Id: mafSplitPos.c,v 1.5 2008/09/03 19:21:16 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSplitPos - Pick positions to split multiple alignment input files\n"
  "usage:\n"
  "   mafSplitPos database size(Mbp) out.bed\n"
  "options:\n"
  "   -chrom=chrN   Restrict to one chromosome\n"
  "   -minGap=N     Split only on gaps >N bp\n"
  "   -minRepeat=N  Split only on repeats >N bp\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"minGap", OPTION_INT},
   {"minRepeat", OPTION_INT},
   {NULL, 0},
};

char *chrom = NULL;
int minGap = 100;
int minRepeat = 100;

int nextGapPos(char *chrom, int desiredPos, struct sqlConnection *conn)
{
/* Find next gap on the chrom and return midpoint */
char where[256];
struct sqlResult *sr;
char **row;
int pos = -1;
int start, end;

safef(where, sizeof where, 
    "chromStart >= %d and chromEnd-chromStart > %d\
    order by chromStart limit 1",
        desiredPos, minGap);
sr = hExtendedChromQuery(conn, "gap", chrom, where, FALSE,
        "chromStart, chromEnd", NULL);
if ((row = sqlNextRow(sr)) != NULL)
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
    genoEnd-genoStart>%d order by genoStart limit 1",
        desiredPos, minRepeat);
sr = hExtendedChromQuery(conn, "rmsk", chrom, where, FALSE,
        "genoStart, genoEnd", NULL);
if ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlSigned(row[0]);
    end = sqlSigned(row[1]);
    pos = start + (end - start)/2;
    }
sqlFreeResult(&sr);
return pos;
}

void chromSplits(char *chrom, int chromSize, int splitSize, 
                        struct sqlConnection *conn, FILE *f)
/* determine positions on a single chromosome */
{
int desiredPos = splitSize;
int splitPos = -1;
int gapPos = -1;
int prevPos = 0;

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
    fprintf(f, "%s\t%d\t%d\n", chrom, splitPos, splitPos+1), 
    verbose(2, "      %s\t%d\n", splitPos == gapPos ? "gap" : "repeat", 
           (splitPos - prevPos)/1000000);
    prevPos = splitPos;
    desiredPos += splitSize;
    }
}

void mafSplitPos(char *database, char *size, char *outFile)
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
FILE *f;

verbose(1, "Finding split positions for %s at ~%s Mbp intervals\n", 
                database, size);
splitSize = sqlSigned(size) * 1000000;
if (chrom == NULL)
    {
    chromHash = hChromSizeHash(database);
    }
else
    {
    chromHash = hashNew(6);
    hashAddInt(chromHash, chrom, hChromSize(database, chrom));
    }
conn = sqlConnect(database);
f = mustOpen(outFile, "w");
hc = hashFirst(chromHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    chrom = hel->name;
    chromSize = ptToInt(hel->val);
    chromSplits(chrom, chromSize, splitSize, conn, f);
    }
sqlDisconnect(&conn);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
chrom = optionVal("chrom", NULL);
minGap = optionInt("minGap", minGap);
minRepeat = optionInt("minRepeat", minRepeat);
if (argc != 4)
    usage();
mafSplitPos(argv[1], argv[2], argv[3]);
return 0;
}
