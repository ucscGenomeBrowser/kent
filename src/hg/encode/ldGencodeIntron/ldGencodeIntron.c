/* ldGencodeIntron - load intron id's and status from Gencode project GTF file */
#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"
#include "gff.h"
#include "hgRelate.h"
#include "gencodeIntron.h"

static char const rcsid[] = "$Id: ldGencodeIntron.c,v 1.1 2005/03/22 03:04:22 kate Exp $";

void usage()
{
errAbort(
       "ldGencodeIntron - load intron table from a Gencode gtf file.\n"
       "usage:\n"
       "     ldGencodeIntron database table file(s).gtf\n");
}

void ldGencodeIntron(char *database, char *table,  
                        int gtfCount, char *gtfNames[])
/* Load Gencode intron status table from GTF files with
 * intron_id and intron_status keywords */
{
struct gffFile *gff, *gffList = NULL;
struct gffLine *gffLine;
struct gencodeIntron *intron, *intronList = NULL;
struct sqlConnection *conn;
FILE *f;
int i;
int introns = 0;

for (i=0; i<gtfCount; i++)
    {
    verbose(1, "Reading %s\n", gtfNames[i]);
    slAddHead(&gffList, gffRead(gtfNames[i]));
    }
for (gff = gffList; gff != NULL; gff = gff->next)
    {
    for (gffLine = gff->lineList; gffLine != NULL; gffLine = gffLine->next)
        {
        if (sameWord(gffLine->feature, "intron"))
            {
            AllocVar(intron);
            intron->chrom = gffLine->seq;
            intron->chromStart = gffLine->start;
            intron->chromEnd = gffLine->end;
            intron->name = gffLine->intronId;
            intron->strand[0] = gffLine->strand;
            intron->strand[1] = 0;
            intron->status = gffLine->intronStatus;
            intron->transcript = gffLine->group;
            intron->geneId = gffLine->geneId;
            slAddHead(&intronList, intron);
            introns++;
            verbose(1, "intron %s\n", intron->name);
            }
        }
    }
slSort(&intronList, bedCmp);
f = hgCreateTabFile(".", table);
for (intron = intronList; intron != NULL; intron = intron->next)
    gencodeIntronTabOut(intron, f);
carefulClose(&f);

verbose(1, "%d introns in %d files\n", introns, gtfCount);
hSetDb(database);
conn = sqlConnect(database);
gencodeIntronTableCreate(conn, table, hGetMinIndexLength());
hgLoadTabFile(conn, ".", table, &f);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line */
{
if (argc < 3)
    usage();
ldGencodeIntron(argv[1], argv[2], argc-3, argv+3);
return 0;
}

