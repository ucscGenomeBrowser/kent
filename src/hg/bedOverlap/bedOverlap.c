#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "bed.h"
#include "hdb.h"
#include "binRange.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedOverlap -  Remove Overlaps from bed files\n"
  "usage:\n"
  "   bedOverlap db track output.bed\n"
  "options:\n"
  "   -xxx \n"
  );
}

void bedOverlap(char *db , char *table, FILE *f)
/* Sort a bed file (in place, overwrites old file. */
{
struct lineFile *lf = NULL;
struct bedLine *blList = NULL, *bl;
char *line;
char *prevChrom = cloneString("chr1");
int lineSize;
int size  = 300000000;
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
boolean hasBin = 0;
char **row;
struct bed *bed, *list, *bestMatch = NULL, *el2;
int overlap = 0 , bestOverlap = 0, i;
int biggest = 0;
int bedSize = 6;
struct binKeeper *bk;
struct binElement *el = NULL;
hSetDb(db);

    printf("Loading Predictions from %s\n",table);
    AllocVar(list);
    conn = hAllocConn();
    sprintf(query, "select * from %s order by chrom, chromStart\n",table);
    sr = sqlGetResult(conn, query);
    bk = binKeeperNew(0, size);
    while ((row = sqlNextRow(sr)) != NULL)
        {         
        bed = bedLoadNBin(row, bedSize);
        if (sameString(bed->chrom,prevChrom))
            {
            binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
            slAddHead(&list, bed);
            }
        else
            {
            //slReverse(&list);
            for (el2 = list ; el2 != NULL; el2 = el2->next)
                {
                biggest = 0; 
                bestMatch = NULL;
                for (el = binKeeperFindSorted(bk, el2->chromStart, el2->chromEnd); el != NULL; el = el->next)
                    {
                    assert(el != NULL);
                    bed = el->val;
                    if (bed->chromEnd - bed->chromStart > biggest && bed->score > 0)
                            {
                            biggest  = bed->chromEnd - bed->chromStart;
                            bestMatch = cloneBed(bed);
                            assert(bestMatch->strand[0] == '+' || bestMatch->strand[0] == '-');
                            }
                    }
                    if (bestMatch != NULL)
                        fprintf(f, "%s\t%d\t%d\t%s\t%d\t%c\n", 
                                bestMatch->chrom, bestMatch->chromStart, bestMatch->chromEnd, 
                                bestMatch->name, bestMatch->score, bestMatch->strand[0]);
                    if (ferror(f))
                        {
                        perror("Writing error\n");
                        errAbort(" file is truncated, sorry.");
                        }
                }
            binKeeperFree(&bk);
            bk = binKeeperNew(0, size);
            bed = bedLoadNBin(row, 4);
            binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
            } 
        prevChrom = cloneString(bed->chrom);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
FILE *f;
if (argc != 4)
    usage();
f = mustOpen(argv[3], "w");
bedOverlap(argv[1], argv[2], f);
return 0;
}
