/* pslToBed -- tranform a psl format file to a bed format file */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
	 "of the psl, like those that are created from genePredToFakePsl\n"
	 "    -posName\n"
	 "changes the qName field to qName:qStart-qEnd\n"
	 "(can be used to create links to query position on details page)\n"
         );

} 

static struct optionSpec options[] = {
   {"cds", OPTION_STRING},
   {"posName", OPTION_BOOLEAN},
   {NULL, 0},
};

struct cds
{
int start, end;   // cds start and end 
};

static unsigned getTargetForQuery(struct psl *psl, int queryAddress)
// get target address for query address from PSL
{
int  blockNum;

for (blockNum=0; blockNum < psl->blockCount; blockNum++)
    {
    if (psl->qStarts[blockNum] > queryAddress)
	{
	// this block starts past the queryAddress
	// just point to the beginning of this block
	return psl->tStarts[blockNum];
	}
    else if (psl->qStarts[blockNum] + psl->blockSizes[blockNum] > queryAddress)
	{
	// since block addresses are always increasing we know if the end
	// of the block is beyond our address that the address is 
	// in this block
	unsigned offsetInBlock = queryAddress - psl->qStarts[blockNum];

	assert(offsetInBlock < psl->blockSizes[blockNum]);

	return psl->tStarts[blockNum] + offsetInBlock;
	}
    }

// we don't have any blocks with this query in it, just point
// to the last base
return psl->tEnd - 1;
}

static void setThick(struct psl *psl, struct bed *bed, struct cds *cds)
// set thickStart and thickEnd based on CDS record
{
unsigned thickStart, thickEnd;
unsigned cdsStart = cds->start - 1;
unsigned cdsEnd = cds->end;

if (psl->strand[0] == '-')
    {
    // if on negative strand, cds is from end
    unsigned temp = cdsStart;
    cdsStart = psl->qSize - cdsEnd;
    cdsEnd = psl->qSize - temp;
    }

// we subtract one from start to convert to PSL coordinate system
thickStart = getTargetForQuery(psl, cdsStart);

// cdsEnd actually points to one base after the end, so
// we translate the base address, then add one
thickEnd = getTargetForQuery(psl, cdsEnd - 1) + 1;

// if thickStart equals thickEnd, then there is no CDS
if (thickStart == thickEnd)
    thickStart = thickEnd = 0;

bed->thickStart = thickStart;
bed->thickEnd = thickEnd;
}

void pslToBed(char *pslFile, char *bedFile, struct hash *cdsHash, bool doPosName)
/* pslToBed -- tranform a psl format file to a bed format file */
{
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *bedFh = mustOpen(bedFile, "w");
struct psl *psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    struct bed *bed = bedFromPsl(psl);
    if (doPosName)
        {
        char *newName = needMem(512);
        safef(newName, 512, "%s:%d-%d", psl->qName, psl->qStart, psl->qEnd);
        freeMem(bed->name);
        bed->name = newName;
        }

    if (cdsHash)
	{
	struct cds *cds = hashFindVal(cdsHash, psl->qName);
	if (cds == NULL)
	    bed->thickStart = bed->thickEnd = bed->chromStart;
	else
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
    cds->start = atoi(row[1]);  // atoi will stop at '.'
    char *ptr = strchr(row[1], '.');	 // point to first '.'
    ptr+=2; // step past two '.'s
    cds->end = atoi(ptr);
    if (cds->start > cds->end)
	errAbort("CDS start(%d) is before end(%d) on line %d",
		cds->start, cds->end, lf->lineIx);
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
bool doPosName = optionExists("posName");
struct hash *cdsHash = NULL;

if (cdsFile != NULL)
    cdsHash = getCdsHash(cdsFile);

pslToBed(argv[1], argv[2], cdsHash, doPosName);
return 0;
}
    

