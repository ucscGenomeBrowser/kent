/* hgSeq - Fetch and format DNA sequence data (to stdout) for gene records. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hdb.h"
#include "rmskOut.h"
#include "fa.h"
#include "genePred.h"
#include "bed.h"

/* Both hgc and hgText define this as a global: */
extern struct cart *cart;

void hgSeqTrackInfoDb(char *db, char *table, boolean *isGene,
		      boolean *canDoUTR, boolean *canDoIntrons, char **type)
{
boolean foundFields;
char fChrom[32], fStart[32], fEnd[32], fName[32], fStrand[32];
char fCdsStart[32], fCdsEnd[32], fCount[32], fStarts[32], fEndsSizes[32];

foundFields = hFindGenePredFieldsDb(db, table,
				    fChrom, fStart, fEnd, fName,
				    fStrand, fCdsStart, fCdsEnd,
				    fCount, fStarts, fEndsSizes);

*isGene = *canDoUTR = *canDoIntrons = FALSE;
if (foundFields)
    {
    if (sameString(fStarts, "exonStarts"))
	{
	/* genePred or variant */
	*isGene = *canDoUTR = *canDoIntrons = TRUE;
	*type = cloneString("genePred");
	}
    else if (sameString(fStarts, "chromStarts") ||
	     sameString(fStarts, "blockStarts"))
	{
	/* bed 12 */
	*isGene = *canDoUTR = *canDoIntrons = TRUE;
	*type = cloneString("bed 12");
	}
    else if (sameString(fStarts, "tStarts"))
	{
	/* psl */
	*canDoIntrons = TRUE;
	*type = cloneString("psl");
	}
    else if (fCdsStart[0] != 0)
	{
	/* bed 8 */
	*isGene = *canDoUTR = TRUE;
	*type = cloneString("bed 8");
	}
    else if (fStrand[0] !=0  &&  fChrom[0] == 0)
	{
	*type = cloneString("gl");
	}
    else if (fStrand[0] !=0)
	{
	*isGene = TRUE;
	*type = cloneString("bed 6");
	}
    else if (fName[0] !=0)
	{
	*type = cloneString("bed 4");
	}
    else
	{
	*type = cloneString("bed 3");
	}
    }
else
    {
    *type = NULL;
    }
}

void hgSeqTrackInfo(char *table, boolean *isGene, boolean *canDoUTR, 
		    boolean *canDoIntrons, char **type)
{
hgSeqTrackInfoDb(hGetDb(), table, isGene, canDoUTR, canDoIntrons, type);
}

void hgSeqFeatureRegionOptions(boolean isGene, boolean canDoUTR,
			       boolean canDoIntrons)
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
    if (canDoIntrons)
	{
	cgiMakeCheckBox("hgSeq.utrIntron5", TRUE);
	puts("5' UTR Introns <BR>\n");
	}
    }

if (canDoIntrons)
    {
    cgiMakeCheckBox("hgSeq.cdsExon", TRUE);
    if (canDoUTR)
	printf("CDS Exons <BR>\n");
    else
	printf("Blocks <BR>\n");
    cgiMakeCheckBox("hgSeq.cdsIntron", TRUE);
    if (canDoUTR)
	puts("CDS Introns <BR>\n");
    else
	puts("Regions between Blocks <BR>\n");
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
    if (canDoIntrons)
	{
	cgiMakeCheckBox("hgSeq.utrIntron3", TRUE);
	puts("3' UTR Introns <BR>\n");
	}
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
	puts("One FASTA record per gene (concatenate selected regions) <BR>\n");
    else
	puts("One FASTA record per item (concatenate selected regions) <BR>\n");
    cgiMakeRadioButton("hgSeq.granularity", "feature", FALSE);
	puts("One FASTA record per selected region with \n");
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

cgiMakeCheckBox("hgSeq.revComp", TRUE);
puts("Reverse-complement if from - strand <BR>\n");
}

void hgSeqOptionsDb(char *db, char *table)
/* Print out HTML FORM entries for gene region and sequence display options. */
{
boolean isGene, canDoUTR, canDoIntrons;
char *type;

hgSeqTrackInfoDb(db, table, &isGene, &canDoUTR, &canDoIntrons, &type);
hgSeqFeatureRegionOptions(isGene, canDoUTR, canDoIntrons);
hgSeqDisplayOptions(canDoUTR, canDoIntrons);
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
boolean isRc     = cartBoolean(cart, "hgSeq.revComp") && (strand == '-');
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
    boolean isRc = cartBoolean(cart, "hgSeq.revComp") && (strand == '-');
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

void hgSeqItemsInRangeDb(char *db, char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints)
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  */
{
struct dyString *query = newDyString(512);
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char itemName[128];
boolean isRc;
int count;
unsigned *starts = NULL;
unsigned *sizes = NULL;
boolean *exonFlags = NULL;
boolean *cdsFlags = NULL;
int i, rowCount;
boolean promoter   = cgiBoolean("hgSeq.promoter");
boolean utrExon5   = cgiBoolean("hgSeq.utrExon5");
boolean utrIntron5 = cgiBoolean("hgSeq.utrIntron5");
boolean cdsExon    = cgiBoolean("hgSeq.cdsExon");
boolean cdsIntron  = cgiBoolean("hgSeq.cdsIntron");
boolean utrExon3   = cgiBoolean("hgSeq.utrExon3");
boolean utrIntron3 = cgiBoolean("hgSeq.utrIntron3");
boolean downstream = cgiBoolean("hgSeq.downstream");
int promoterSize   = cgiOptionalInt("hgSeq.promoterSize", 0);
int downstreamSize = cgiOptionalInt("hgSeq.downstreamSize", 0);
char *granularity  = cgiOptionalString("hgSeq.granularity");
boolean revComp    = cgiBoolean("hgSeq.revComp");
boolean concatRegions = granularity && sameString("gene", granularity);
boolean isCDS, doIntron;
boolean foundFields;
char fChrom[32], fStart[32], fEnd[32], fName[32], fStrand[32];
char fCdsStart[32], fCdsEnd[32], fCount[32], fStarts[32], fEndsSizes[32];
boolean canDoUTR=FALSE, canDoIntrons=FALSE;
char *name, *strand;
int start, end, cdsStart, cdsEnd, blkCount;
unsigned *blkStarts, *blkSizes;

foundFields = hFindGenePredFieldsDb(db, table,
				    fChrom, fStart, fEnd, fName,
				    fStrand, fCdsStart, fCdsEnd,
				    fCount, fStarts, fEndsSizes);

if (sameString(db, hGetDb()))
    conn = hAllocConn();
else if (sameString(db, hGetDb2()))
    conn = hAllocConn2();
else
    {
    hSetDb2(db);
    conn = hAllocConn2();
    }
dyStringClear(query);
// row[0], row[1] -> start, end
dyStringPrintf(query, "select %s,%s", fStart, fEnd);
// row[2] -> name or placeholder
if (fName[0] != 0)
    dyStringPrintf(query, ",%s", fName);
else
    dyStringPrintf(query, ",%s", fStart);  // keep the same #fields!
// row[3] -> strand or placeholder
if (fStrand[0] != 0)
    dyStringPrintf(query, ",%s", fStrand);
else
    dyStringPrintf(query, ",%s", fStart);  // keep the same #fields!
// row[4], row[5] -> cdsStart, cdsEnd or placeholders
if (fCdsStart[0] != 0)
    {
    dyStringPrintf(query, ",%s,%s", fCdsStart, fCdsEnd);
    canDoUTR = TRUE;
    }
else
    dyStringPrintf(query, ",%s,%s", fStart, fStart);  // keep the same #fields!
// row[6], row[7], row[8] -> count, starts, ends/sizes or empty.
if (fCount[0] != 0)
    {
    dyStringPrintf(query, ",%s,%s,%s", fCount, fStarts, fEndsSizes);
    canDoIntrons = TRUE;
    }
dyStringPrintf(query, " from %s where %s < %d and %s > %d",
	       table, fStart, chromEnd, fEnd, chromStart);
if (fChrom[0] != 0)
    dyStringPrintf(query, " and %s = '%s'", fChrom, chrom);
if (sqlConstraints != NULL && sqlConstraints[0] != 0)
    dyStringPrintf(query, " and %s", sqlConstraints);

// uglyf("# query: %s\n", query->string);
sr = sqlGetResult(conn, query->string);

rowCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    rowCount++;
    start     = atoi(row[0]);
    end       = atoi(row[1]);
    name      = row[2];
    strand    = row[3];
    cdsStart  = atoi(row[4]);
    cdsEnd    = atoi(row[5]);
    /* thickStart, thickEnd fields are sometimes used for other-organism 
       coords (e.g. synteny100000, syntenyBuild30).  So if they look 
       completely wrong, fake them out to start/end.  */
    if (cdsStart < start)
	cdsStart = start;
    else if (cdsStart > end)
	cdsStart = start;
    if (cdsEnd < start)
	cdsEnd = end;
    else if (cdsEnd > end)
	cdsEnd = end;
    if (canDoIntrons)
	{
	blkCount  = atoi(row[6]);
	sqlUnsignedDynamicArray(row[7], &blkStarts, &count);
	assert(count == blkCount);
	sqlUnsignedDynamicArray(row[8], &blkSizes, &count);
	assert(count == blkCount);
	if (sameString("exonEnds", fEndsSizes))
	    {
	    // genePred: translate ends to sizes
	    for (i=0;  i < blkCount;  i++)
		{
		blkSizes[i] -= blkStarts[i];
		}
	    }
	if (sameString("chromStarts", fStarts) ||
	    sameString("blockStarts", fStarts))
	    {
	    // bed: translate relative starts to absolute starts
	    for (i=0;  i < blkCount;  i++)
		{
		blkStarts[i] += start;
		}
	    }
	}
    else
	{
	blkCount  = 0;
	blkStarts = NULL;
	blkSizes  = NULL;
	}
    if (fStrand[0] == 0)
	strand = "?";
    isRc = revComp && (strand[0] == '-');
    // here's the max # of feature regions:
    if (canDoIntrons)
	count = 2 + (2 * blkCount);
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
		   (start - promoterSize), promoterSize, FALSE, FALSE);
	}
    else if (isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   (start - downstreamSize), downstreamSize,
		   FALSE, FALSE);
	}
    if (canDoIntrons && canDoUTR)
	{
	for (i=0;  i < blkCount;  i++)
	    {
	    if ((blkStarts[i] + blkSizes[i]) <= cdsStart)
		{
		if ((!isRc && utrExon5)   || (isRc && utrExon3))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       blkStarts[i], blkSizes[i],
			       TRUE, FALSE);
		    }
		if (((!isRc && utrIntron5) || (isRc && utrIntron3)) &&
		    (i < blkCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (blkStarts[i] + blkSizes[i]),
			       (blkStarts[i+1] - blkStarts[i] - blkSizes[i]),
			       FALSE, FALSE);
		    }
		}
	    else if (blkStarts[i] < cdsEnd)
		{
		if (blkStarts[i] < cdsStart)
		    {
		    if ((!isRc && utrExon5)	  || (isRc && utrExon3))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   blkStarts[i],
				   (cdsStart - blkStarts[i]),
				   TRUE, FALSE);
			}
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   cdsStart,
				   (blkStarts[i] + blkSizes[i] - cdsStart),
				   TRUE, TRUE);
			}
		    }
		else if ((blkStarts[i] + blkSizes[i]) >
			 cdsEnd)
		    {
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   blkStarts[i],
				   (cdsEnd - blkStarts[i]),
				   TRUE, TRUE);
			}
		    if ((!isRc && utrExon3)	  || (isRc && utrExon5))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   cdsEnd,
				   (blkStarts[i] + blkSizes[i] - cdsEnd),
				   TRUE, FALSE);
			}
		    }
		else if (cdsExon)
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       blkStarts[i], blkSizes[i],
			       TRUE, TRUE);
		    }
		isCDS = ! ((blkStarts[i] + blkSizes[i]) > cdsEnd);
		doIntron = (isCDS ? cdsIntron :
			    ((!isRc) ? utrIntron3 : utrIntron5));
		if (doIntron && (i < blkCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (blkStarts[i] + blkSizes[i]),
			       (blkStarts[i+1] - blkStarts[i] - blkSizes[i]),
			       FALSE, isCDS);
		    }
		}
	    else
		{
		if ((!isRc && utrExon3)   || (isRc && utrExon5))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       blkStarts[i], blkSizes[i],
			       TRUE, FALSE);
		    }
		if (((!isRc && utrIntron3) || (isRc && utrIntron5)) &&
		    (i < blkCount - 1))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       (blkStarts[i] + blkSizes[i]),
			       (blkStarts[i+1] - blkStarts[i] - blkSizes[i]),
			       FALSE, FALSE);
		    }
		}
	    }
	}
    else if (canDoIntrons)
	{
	for (i=0;  i < blkCount;  i++)
	    {
	    if (cdsExon)
		{
		addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			   blkStarts[i], blkSizes[i],
			   TRUE, FALSE);
		}
	    if (cdsIntron && (i < blkCount - 1))
		{
		addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			   (blkStarts[i] + blkSizes[i]),
			   (blkStarts[i+1] - blkStarts[i] - blkSizes[i]),
			   FALSE, FALSE);
		}
	    }
	}
    else if (canDoUTR)
	{
	if ((!isRc && utrExon5)   || (isRc && utrExon3))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       start,
		       (cdsStart - start), TRUE, FALSE);
	    }
	if (cdsExon)
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       cdsStart, (cdsEnd - cdsStart),
		       TRUE, TRUE);
	    }
	if ((!isRc && utrExon3)   || (isRc && utrExon5))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       cdsEnd, (end - cdsEnd),
		       TRUE, FALSE);
	    }
	}
    else
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   start, (end - start),
		   TRUE, FALSE);
	}
    if (!isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   end, downstreamSize, FALSE, FALSE);
	}
    else if (isRc && promoter && (promoterSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   end, promoterSize, FALSE, FALSE);
	}
    if (fName[0] != 0)
	snprintf(itemName, sizeof(itemName), "%s_%s", table, name);
    else
	strncpy(itemName, table, sizeof(itemName));
    hgSeqRegionsDb(db, chrom, strand[0], itemName, concatRegions,
		   count, starts, sizes, exonFlags, cdsFlags);
    freeMem(starts);
    freeMem(sizes);
    freeMem(exonFlags);
    freeMem(cdsFlags);
    }
if (rowCount == 0)
    printf("\n# No items in table %s in the range %s:%d-%d\n\n",
	   table, chrom, chromStart+1, chromEnd);
sqlFreeResult(&sr);
if (sameString(db, hGetDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
}

void hgSeqItemsInRange(char *table, char *chrom, int chromStart, int chromEnd,
		       char *sqlConstraints)
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  */
{
hgSeqItemsInRangeDb(hGetDb(), table, chrom, chromStart, chromEnd,
		    sqlConstraints);
}
