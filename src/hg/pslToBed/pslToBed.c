/* pslToBed -- tranform a psl format file to a bed format file */

#include "common.h"
#include "psl.h"
#include "bed.h"
#include "options.h"


void usage()
/* print usage infomation and exit */
{
errAbort("pslToBed: tranform a psl format file to a bed format file.\n"
         "usage:\n"
         "    pslToBed psl bed\n"
	 "options:\n"
	 "    -cds=cdsFile\n"
	 "cdsFile specifies a input cds tab-separated file which contains\n"
	 "genbank-style CDS records showing cdsStart..cdsEnd\n"
	 "e.g. NM_123456 34..305\n"
	 "These coordinates are assumed to be in the query coordinate system\n"
	 "of the psl, like those that are created from genePredToFakePsl\n");

} 

static struct optionSpec options[] = {
   {"cds", OPTION_STRING},
   {NULL, 0},
};

struct cds
{
int start, end;   // cds start and end 
};

static void setThick(struct psl *psl, struct bed *bed, struct cds *cds)
// set thickStart and thickEnd based on CDS record
{
int ii;
int thickStart, thickEnd;

thickStart = thickEnd = 0;
for(ii=0; ii < psl->blockCount; ii++)
    {
    if (psl->qStarts[ii] + psl->blockSizes[ii] > cds->start - 1)
	{
	int offset = cds->start - psl->qStarts[ii];
	if (offset < 0)
	    offset = 0;
	thickEnd = thickStart = psl->tStarts[ii] + offset - 1;
	break;
	}
    }

for(; ii < psl->blockCount; ii++)
    {
    if (psl->qStarts[ii] + psl->blockSizes[ii] >= cds->end)
	{
	int offset = cds->end - psl->qStarts[ii];
	if (offset < 0)
	    offset = 0;
	thickEnd = psl->tStarts[ii] + offset;
	break;
	}
    }
if (ii >= psl->blockCount)
    {
    thickEnd = psl->tStarts[ii - 1] + psl->blockSizes[ii - 1];
    }
bed->thickStart = thickStart;
bed->thickEnd = thickEnd;
}

void pslToBed(char *pslFile, char *bedFile, struct hash *cdsHash)
/* pslToBed -- tranform a psl format file to a bed format file */
{
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *bedFh = mustOpen(bedFile, "w");
struct psl *psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    struct bed *bed = bedFromPsl(psl);
    if (cdsHash)
	{
	struct cds *cds = hashFindVal(cdsHash, psl->qName);
	if (cds != NULL)
	    setThick(psl, bed, cds);
	}
    bedTabOutN(bed, 12, bedFh);
    bedFree(&bed);
    pslFree(&psl);
    }
carefulClose(&bedFh);
lineFileClose(&pslLf);
}

struct hash *getCdsHash(char *cdsFile)
/* read CDS start and end for list of id's */
{
struct lineFile *lf = lineFileOpen(cdsFile, TRUE);
char *row[2];
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    {
    // lines are of the form of <start>..<end>
    struct cds *cds;
    AllocVar(cds);
    cds->start = atoi(row[1]);
    char *ptr = strchr(row[1], '.');	 // point to first '.'
    ptr+=2; // step past two '.'s
    cds->end = atoi(ptr);
    hashAdd(hash, row[0], cds);
    }
lineFileClose(&lf);
return hash;
}

int main(int argc, char* argv[])
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *cdsFile = optionVal("cds", NULL);
struct hash *cdsHash = NULL;

if (cdsFile != NULL)
    cdsHash = getCdsHash(cdsFile);

pslToBed(argv[1], argv[2], cdsHash);
return 0;
}
    

