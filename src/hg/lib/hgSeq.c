/* hgSeq - Fetch and format DNA sequence data (to stdout) for gene records. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "rmskOut.h"
#include "fa.h"
#include "genePred.h"
#include "bed.h"

/* Both hgc and hgText define this as a global: */
extern struct cart *cart;

void hgSeqFeatureRegionOptions(boolean canDoUTR, boolean canDoIntrons)
/* Print out HTML FORM entries for feature region options. */
{
char *exonStr = canDoIntrons ? " Exons" : "";

puts("<H3> Sequence Retrieval Region Options: </H3>\n");

if (canDoIntrons || canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.promoter", FALSE);
    puts("Promoter/Upstream by ");
    cgiMakeIntVar("hgSeq.promoterSize", 1000, 5);
    puts("bases <BR>\n");
    }

if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.utrExon5", TRUE);
    printf("5' UTR%s <BR>\n", exonStr);
    }

if (canDoIntrons)
    {
    cgiMakeCheckBox("hgSeq.cdsExon", TRUE);
    if (canDoUTR)
	printf("CDS Exons <BR>\n");
    else
	printf("Blocks <BR>\n");
    }
else if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.cdsExon", TRUE);
    printf("CDS <BR>\n");
    }
else
    {
    cgiMakeHiddenVar("hgSeq.cdsExon", "1");
    }

if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.utrExon3", TRUE);
    printf("3' UTR%s <BR>\n", exonStr);
    }

if (canDoIntrons)
    {
    cgiMakeCheckBox("hgSeq.intron", TRUE);
    if (canDoUTR)
	puts("Introns <BR>");
    else
	puts("Regions between blocks <BR>");
    }

if (canDoIntrons || canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.downstream", FALSE);
    puts("Downstream by ");
    cgiMakeIntVar("hgSeq.downstreamSize", 1000, 5);
    puts("bases <BR>\n");
    }

if (canDoIntrons || canDoUTR)
    {
    cgiMakeRadioButton("hgSeq.granularity", "gene", TRUE);
    if (canDoUTR)
	puts("One FASTA record per gene. <BR>\n");
    else
	puts("One FASTA record per item. <BR>\n");
    cgiMakeRadioButton("hgSeq.granularity", "feature", FALSE);
    if (canDoUTR)
	puts("One FASTA record per region (exon, intron, etc.) with \n");
    else
	puts("One FASTA record per region (block/between blocks) with \n");
    }
else
    {
    puts("Add ");
    }
cgiMakeIntVar("hgSeq.padding5", 0, 5);
puts("extra bases upstream (5') and \n");
cgiMakeIntVar("hgSeq.padding3", 0, 5);
puts("extra downstream (3') <BR>\n");
puts("<P>\n");

}


void hgSeqDisplayOptions(boolean canDoUTR, boolean canDoIntrons)
/* Print out HTML FORM entries for sequence display options. */
{
puts("\n<H3> Sequence Formatting Options: </H3>\n");

if (canDoIntrons)
    {
    cgiMakeRadioButton("hgSeq.casing", "exon", TRUE);
    if (canDoUTR)
	puts("Exons in upper case, everything else in lower case. <BR>\n");
    else
	puts("Blocks in upper case, everything else in lower case. <BR>\n");
    }
if (canDoUTR)
    {
    cgiMakeRadioButton("hgSeq.casing", "cds", !canDoIntrons);
    puts("CDS in upper case, UTR in lower case. <BR>\n");
    }
cgiMakeRadioButton("hgSeq.casing", "upper", !(canDoIntrons || canDoUTR));
puts("All upper case. <BR>\n");
cgiMakeRadioButton("hgSeq.casing", "lower", FALSE);
puts("All lower case. <BR>\n");

cgiMakeCheckBox("hgSeq.maskRepeats", FALSE);
puts("Mask repeats: \n");
cgiMakeRadioButton("hgSeq.repMasking", "lower", TRUE);
puts(" to lower case \n");
cgiMakeRadioButton("hgSeq.repMasking", "N", FALSE);
puts(" to N <BR>\n");
}

void hgSeqOptionsDb(char *db, char *table)
/* Print out HTML FORM entries for gene region and sequence display options. */
{
struct hTableInfo *hti;
char chrom[32];
char rootName[256];

hParseTableName(table, rootName, chrom);
hti = hFindTableInfoDb(db, chrom, rootName);
if (hti == NULL)
    webAbort("Error", "Could not find table info for table %s (%s)",
	     rootName, table);
hgSeqFeatureRegionOptions(hti->hasCDS, hti->hasBlocks);
hgSeqDisplayOptions(hti->hasCDS, hti->hasBlocks);
}


void hgSeqOptions(char *table)
/* Print out HTML FORM entries for gene region and sequence display options. */
{
hgSeqOptionsDb(hGetDb(), table);
}


static void maskRepeats(char *chrom, int chromStart, int chromEnd,
			DNA *dna, boolean soft)
/* Lower case bits corresponding to repeats. */
{
int rowOffset;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;

conn = hAllocConn();
sr = hRangeQuery(conn, "rmsk", chrom, chromStart, chromEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct rmskOut ro;
    rmskOutStaticLoad(row+rowOffset, &ro);
    if (ro.genoEnd > chromEnd) ro.genoEnd = chromEnd;
    if (ro.genoStart < chromStart) ro.genoStart = chromStart;
    if (soft)
	toLowerN(dna+ro.genoStart-chromStart, ro.genoEnd - ro.genoStart);
    else
        memset(dna+ro.genoStart-chromStart, 'n', ro.genoEnd - ro.genoStart);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void hgSeqConcatRegionsDb(char *db, char *chrom, char strand, char *name,
			  int rCount, unsigned *rStarts, unsigned *rSizes,
			  boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions. */
{
struct dnaSeq *rSeq;
struct dnaSeq *cSeq;
char recName[256];
int seqStart, seqEnd;
int offset, cSize;
int i;
boolean isRc     = (strand == '-');
boolean maskRep  = cartBoolean(cart, "hgSeq.maskRepeats");
int padding5     = cartInt(cart, "hgSeq.padding5");
int padding3     = cartInt(cart, "hgSeq.padding3");
char *casing     = cartString(cart, "hgSeq.casing");
char *repMasking = cartString(cart, "hgSeq.repMasking");

if (rCount < 1)
    return;

/* Don't support padding if we're concatenating multiple regions. */
if (rCount > 1)
    {
    padding5 = padding3 = 0;
    }

i = rCount - 1;
seqStart = rStarts[0]             - (isRc ? padding3 : padding5);
seqEnd   = rStarts[i] + rSizes[i] + (isRc ? padding5 : padding3);
rSeq = hDnaFromSeq(chrom, seqStart, seqEnd, dnaLower);

/* Handle casing and compute size of concatenated sequence */
if (sameString(casing, "upper"))
    touppers(rSeq->dna);
cSize = 0;
for (i=0;  i < rCount;  i++)
    {
    if (sameString(casing, "exon") && exonFlags[i])
	{
	int rStart = rStarts[i] - seqStart;
	toUpperN(rSeq->dna+rStart, rSizes[i]);
	}
    else if (sameString(casing, "cds") && cdsFlags[i])
	{
	int rStart = rStarts[i] - seqStart;
	toUpperN(rSeq->dna+rStart, rSizes[i]);
	}
    cSize += (rSizes[i] + padding5 + padding3);
    }
AllocVar(cSeq);
cSeq->dna = needMem(cSize+1);
cSeq->size = cSize;

if (maskRep)
    if (sameString(repMasking, "lower"))
	maskRepeats(chrom, seqStart, seqEnd, rSeq->dna, TRUE);
    else
	maskRepeats(chrom, seqStart, seqEnd, rSeq->dna, FALSE);

offset = 0;
for (i=0;  i < rCount;  i++)
    {
    int start = rStarts[i] - (isRc ? padding3 : padding5) - seqStart;
    int size  = rSizes[i] + padding5 + padding3;
    memcpy(cSeq->dna+offset, rSeq->dna+start, size);
    offset += size;
    }
assert(offset == cSeq->size);
cSeq->dna[offset] = 0;
freeDnaSeq(&rSeq);

if (isRc)
    reverseComplement(cSeq->dna, cSeq->size);

sprintf(recName, "%s%s%s_%s range=%s:%d-%d 5'pad=%d 3'pad=%d revComp=%s strand=%c repeatMasking=%s",
	db, 
	(sameString(db, hGetDb()) ? "" : "_"),
	(sameString(db, hGetDb()) ? "" : hGetDb()),
	name,
	chrom, seqStart+1, seqEnd,
	padding5, padding3,
	(isRc ? "TRUE" : "FALSE"),
	strand,
	(maskRep ? repMasking : "none"));
faWriteNext(stdout, recName, cSeq->dna, cSeq->size);
}


void hgSeqRegionsDb(char *db, char *chrom, char strand, char *name,
		    boolean concatRegions,
		    int rCount, unsigned *rStarts, unsigned *rSizes,
		    boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions. */
{
if (concatRegions)
    {
    hgSeqConcatRegionsDb(hGetDb(), chrom, strand, name,
			 rCount, rStarts, rSizes, exonFlags, cdsFlags);
    }
else
    {
    int i, j;
    boolean isRc = (strand == '-');
    char rName[256];
    for (i=0;  i < rCount;  i++)
	{
	snprintf(rName, sizeof(rName), "%s_%d", name, i);
	j = (isRc ? (rCount - i - 1) : i);
	hgSeqConcatRegionsDb(hGetDb(), chrom, strand, rName,
			     1, &rStarts[j], &rSizes[j], &exonFlags[j],
			     &cdsFlags[j]);
	}
    }
}

void hgSeqRegions(char *chrom, char strand, char *name,
		  boolean concatRegions,
		  int rCount, unsigned *rStarts, unsigned *rSizes,
		  boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions. */
{
hgSeqRegionsDb(hGetDb(), chrom, strand, name, concatRegions,
	       rCount, rStarts, rSizes, exonFlags, cdsFlags);
}


void hgSeqRange(char *chrom, int chromStart, int chromEnd, char strand,
		char *name)
/* Print out dna sequence for the given range. */
{
int count;
int starts[1];
int sizes[1];
boolean exonFlags[1];
boolean cdsFlags[1];

count = 1;
starts[0] = chromStart;
sizes[0] = chromEnd - chromStart;
exonFlags[0] = FALSE;
cdsFlags[0] = FALSE;
hgSeqRegions(chrom, strand, name, FALSE, count, starts, sizes, exonFlags,
	     cdsFlags);
}


void addFeature(int *count, unsigned *starts, unsigned *sizes,
		boolean *exonFlags, boolean *cdsFlags,
		int start, int size, boolean eFlag, boolean cFlag)
{
starts[*count]    = start;
sizes[*count]     = size;
exonFlags[*count] = eFlag;
cdsFlags[*count]  = cFlag;
(*count)++;
}


int hgSeqItemsInRangeDb(char *db, char *table, char *chrom, int chromStart,
			int chromEnd, char *sqlConstraints)
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  Return number of items. */
{
struct hTableInfo *hti;
struct bed *bedList, *bedItem;
char itemName[128];
char parsedChrom[32];
char rootName[256];
boolean isRc;
int count;
unsigned *starts = NULL;
unsigned *sizes = NULL;
boolean *exonFlags = NULL;
boolean *cdsFlags = NULL;
int i, rowCount, totalCount;
boolean promoter   = cgiBoolean("hgSeq.promoter");
boolean intron     = cgiBoolean("hgSeq.intron");
boolean utrExon5   = cgiBoolean("hgSeq.utrExon5");
boolean utrIntron5 = utrExon5 && intron;
boolean cdsExon    = cgiBoolean("hgSeq.cdsExon");
boolean cdsIntron  = cdsExon && intron;
boolean utrExon3   = cgiBoolean("hgSeq.utrExon3");
boolean utrIntron3 = utrExon3 && intron;
boolean downstream = cgiBoolean("hgSeq.downstream");
int promoterSize   = cgiOptionalInt("hgSeq.promoterSize", 0);
int downstreamSize = cgiOptionalInt("hgSeq.downstreamSize", 0);
char *granularity  = cgiOptionalString("hgSeq.granularity");
boolean concatRegions = granularity && sameString("gene", granularity);
boolean isCDS, doIntron;
boolean foundFields;
boolean canDoUTR, canDoIntrons;

hParseTableName(table, rootName, parsedChrom);
hti = hFindTableInfoDb(db, chrom, rootName);
if (hti == NULL)
    webAbort("Error", "Could not find table info for table %s (%s)",
	     rootName, table);
canDoUTR = hti->hasCDS;
canDoIntrons = hti->hasBlocks;

bedList = hGetBedRangeDb(db, table, chrom, chromStart, chromEnd,
			 sqlConstraints);

rowCount = totalCount = 0;
for (bedItem = bedList;  bedItem != NULL;  bedItem = bedItem->next)
    {
    rowCount++;
    // bed: translate relative starts to absolute starts
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	bedItem->chromStarts[i] += bedItem->chromStart;
	}
    isRc = (bedItem->strand[0] == '-');
    // here's the max # of feature regions:
    if (canDoIntrons)
	count = 2 + (2 * bedItem->blockCount);
    else
	count = 5;
    starts    = needMem(sizeof(unsigned) * count);
    sizes     = needMem(sizeof(unsigned) * count);
    exonFlags = needMem(sizeof(boolean) * count);
    cdsFlags  = needMem(sizeof(boolean) * count);
    // build up a list of selected regions
    count = 0;
    if (!isRc && promoter && (promoterSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   (bedItem->chromStart - promoterSize), promoterSize,
		   FALSE, FALSE);
	}
    else if (isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   (bedItem->chromStart - downstreamSize), downstreamSize,
		   FALSE, FALSE);
	}
    if (canDoIntrons && canDoUTR)
	{
	for (i=0;  i < bedItem->blockCount;  i++)
	    {
	    if ((bedItem->chromStarts[i] + bedItem->blockSizes[i]) <=
		bedItem->thickStart)
		{
		if ((!isRc && utrExon5)   || (isRc && utrExon3))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       bedItem->chromStarts[i], bedItem->blockSizes[i],
			       TRUE, FALSE);
		    }
		if (((!isRc && utrIntron5) || (isRc && utrIntron3)) &&
		    (i < bedItem->blockCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (bedItem->chromStarts[i] +
				bedItem->blockSizes[i]),
			       (bedItem->chromStarts[i+1] -
				bedItem->chromStarts[i] -
				bedItem->blockSizes[i]),
			       FALSE, FALSE);
		    }
		}
	    else if (bedItem->chromStarts[i] < bedItem->thickEnd)
		{
		if (bedItem->chromStarts[i] < bedItem->thickStart)
		    {
		    if ((!isRc && utrExon5)	  || (isRc && utrExon3))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->chromStarts[i],
				   (bedItem->thickStart -
				    bedItem->chromStarts[i]),
				   TRUE, FALSE);
			}
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickStart,
				   (bedItem->chromStarts[i] +
				    bedItem->blockSizes[i] -
				    bedItem->thickStart),
				   TRUE, TRUE);
			}
		    }
		else if ((bedItem->chromStarts[i] + bedItem->blockSizes[i]) >
			 bedItem->thickEnd)
		    {
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->chromStarts[i],
				   (bedItem->thickEnd -
				    bedItem->chromStarts[i]),
				   TRUE, TRUE);
			}
		    if ((!isRc && utrExon3)	  || (isRc && utrExon5))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickEnd,
				   (bedItem->chromStarts[i] +
				    bedItem->blockSizes[i] -
				    bedItem->thickEnd),
				   TRUE, FALSE);
			}
		    }
		else if (cdsExon)
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       bedItem->chromStarts[i], bedItem->blockSizes[i],
			       TRUE, TRUE);
		    }
		isCDS = ! ((bedItem->chromStarts[i] + bedItem->blockSizes[i]) >
			   bedItem->thickEnd);
		doIntron = (isCDS ? cdsIntron :
			    ((!isRc) ? utrIntron3 : utrIntron5));
		if (doIntron && (i < bedItem->blockCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (bedItem->chromStarts[i] +
				bedItem->blockSizes[i]),
			       (bedItem->chromStarts[i+1] -
				bedItem->chromStarts[i] -
				bedItem->blockSizes[i]),
			       FALSE, isCDS);
		    }
		}
	    else
		{
		if ((!isRc && utrExon3)   || (isRc && utrExon5))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       bedItem->chromStarts[i], bedItem->blockSizes[i],
			       TRUE, FALSE);
		    }
		if (((!isRc && utrIntron3) || (isRc && utrIntron5)) &&
		    (i < bedItem->blockCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (bedItem->chromStarts[i] +
				bedItem->blockSizes[i]),
			       (bedItem->chromStarts[i+1] -
				bedItem->chromStarts[i] -
				bedItem->blockSizes[i]),
			       FALSE, FALSE);
		    }
		}
	    }
	}
    else if (canDoIntrons)
	{
	for (i=0;  i < bedItem->blockCount;  i++)
	    {
	    if (cdsExon)
		{
		addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			   bedItem->chromStarts[i], bedItem->blockSizes[i],
			   TRUE, FALSE);
		}
	    if (cdsIntron && (i < bedItem->blockCount - 1))
		{
		addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			   (bedItem->chromStarts[i] + bedItem->blockSizes[i]),
			   (bedItem->chromStarts[i+1] -
			    bedItem->chromStarts[i] -
			    bedItem->blockSizes[i]),
			   FALSE, FALSE);
		}
	    }
	}
    else if (canDoUTR)
	{
	if ((!isRc && utrExon5)   || (isRc && utrExon3))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->chromStart,
		       (bedItem->thickStart - bedItem->chromStart),
		       TRUE, FALSE);
	    }
	if (cdsExon)
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->thickStart,
		       (bedItem->thickEnd - bedItem->thickStart),
		       TRUE, TRUE);
	    }
	if ((!isRc && utrExon3)   || (isRc && utrExon5))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->thickEnd,
		       (bedItem->chromEnd - bedItem->thickEnd),
		       TRUE, FALSE);
	    }
	}
    else
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromStart,
		   (bedItem->chromEnd - bedItem->chromStart),
		   TRUE, FALSE);
	}
    if (!isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromEnd, downstreamSize, FALSE, FALSE);
	}
    else if (isRc && promoter && (promoterSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromEnd, promoterSize, FALSE, FALSE);
	}
    if (hti->nameField[0] != 0)
	snprintf(itemName, sizeof(itemName), "%s_%s", table, bedItem->name);
    else
	strncpy(itemName, table, sizeof(itemName));
    hgSeqRegionsDb(db, chrom, bedItem->strand[0], itemName, concatRegions,
		   count, starts, sizes, exonFlags, cdsFlags);
    totalCount += count;
    freeMem(starts);
    freeMem(sizes);
    freeMem(exonFlags);
    freeMem(cdsFlags);
    }
bedFreeList(&bedList);
return totalCount;
}


int hgSeqItemsInRange(char *table, char *chrom, int chromStart, int chromEnd,
		       char *sqlConstraints)
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  Return number of items. */
{
return hgSeqItemsInRangeDb(hGetDb(), table, chrom, chromStart, chromEnd,
			   sqlConstraints);
}
