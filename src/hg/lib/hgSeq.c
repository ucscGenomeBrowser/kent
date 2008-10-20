/* hgSeq - Fetch and format DNA sequence data (to stdout) for gene records. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "web.h"
#include "rmskOut.h"
#include "fa.h"
#include "genePred.h"
#include "bed.h"
#include "hgSeq.h"

static char const rcsid[] = "$Id: hgSeq.c,v 1.38 2008/10/20 17:27:29 angie Exp $";

int hgSeqChromSize(char *db, char *chromName)
/* get chrom size if there's a database out there,
 * otherwise just return 0 */
{
int thisSize = 0;

/* first check to see if we can connect to the database */
struct sqlConnection *conn = sqlMayConnect(db);

if (conn != NULL)
    {
    sqlDisconnect(&conn);

    /* we know there is a db out there */
    thisSize = hChromSize(db, chromName);
    }

return thisSize;
}

static void hgSeqFeatureRegionOptions(struct cart *cart, boolean canDoUTR,
			       boolean canDoIntrons)
/* Print out HTML FORM entries for feature region options. */
{
char *exonStr = canDoIntrons ? " Exons" : "";
char *setting;

puts("\n<H3> Sequence Retrieval Region Options: </H3>\n");

if (canDoIntrons || canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.promoter",
		    cartCgiUsualBoolean(cart, "hgSeq.promoter", FALSE));
    puts("Promoter/Upstream by ");
    setting = cartCgiUsualString(cart, "hgSeq.promoterSize", "1000");
    cgiMakeTextVar("hgSeq.promoterSize", setting, 5);
    puts("bases <BR>");
    }

if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.utrExon5",
		    cartCgiUsualBoolean(cart, "hgSeq.utrExon5", TRUE));
    printf("5' UTR%s <BR>\n", exonStr);
    }

if (canDoIntrons)
    {
    cgiMakeCheckBox("hgSeq.cdsExon",
		    cartCgiUsualBoolean(cart, "hgSeq.cdsExon", TRUE));
    if (canDoUTR)
	printf("CDS Exons <BR>\n");
    else
	printf("Blocks <BR>\n");
    }
else if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.cdsExon",
		    cartCgiUsualBoolean(cart, "hgSeq.cdsExon", TRUE));
    printf("CDS <BR>\n");
    }
else
    {
    cgiMakeHiddenVar("hgSeq.cdsExon", "1");
    }

if (canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.utrExon3",
 		    cartCgiUsualBoolean(cart, "hgSeq.utrExon3", TRUE));
   printf("3' UTR%s <BR>\n", exonStr);
    }

if (canDoIntrons)
    {
    cgiMakeCheckBox("hgSeq.intron",
		    cartCgiUsualBoolean(cart, "hgSeq.intron", TRUE));
    if (canDoUTR)
	puts("Introns <BR>");
    else
	puts("Regions between blocks <BR>");
    }

if (canDoIntrons || canDoUTR)
    {
    cgiMakeCheckBox("hgSeq.downstream",
		    cartCgiUsualBoolean(cart, "hgSeq.downstream", FALSE));
    puts("Downstream by ");
    setting = cartCgiUsualString(cart, "hgSeq.downstreamSize", "1000");
    cgiMakeTextVar("hgSeq.downstreamSize", setting, 5);
    puts("bases <BR>");
    }

if (canDoIntrons || canDoUTR)
    {
    setting = cartCgiUsualString(cart, "hgSeq.granularity", "gene");
    cgiMakeRadioButton("hgSeq.granularity", "gene",
		       sameString(setting, "gene"));
    if (canDoUTR)
	puts("One FASTA record per gene. <BR>");
    else
	puts("One FASTA record per item. <BR>");
    cgiMakeRadioButton("hgSeq.granularity", "feature",
		       sameString(setting, "feature"));
    if (canDoUTR)
	puts("One FASTA record per region (exon, intron, etc.) with ");
    else
	puts("One FASTA record per region (block/between blocks) with ");
    }
else
    {
    puts("Add ");
    }
setting = cartCgiUsualString(cart, "hgSeq.padding5", "0");
cgiMakeTextVar("hgSeq.padding5", setting, 5);
puts("extra bases upstream (5') and ");
setting = cartCgiUsualString(cart, "hgSeq.padding3", "0");
cgiMakeTextVar("hgSeq.padding3", setting, 5);
puts("extra downstream (3') <BR>");
if (canDoIntrons && canDoUTR)
    {
    puts("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox("hgSeq.splitCDSUTR",
		    cartCgiUsualBoolean(cart, "hgSeq.splitCDSUTR", FALSE));
    puts("Split UTR and CDS parts of an exon into separate FASTA records");
    }
puts("<BR>\n");
puts("Note: if a feature is close to the beginning or end of a chromosome \n"
     "and upstream/downstream bases are added, they may be truncated \n"
     "in order to avoid extending past the edge of the chromosome. <P>");
}


static void hgSeqDisplayOptions(struct cart *cart, boolean canDoUTR,
                                boolean canDoIntrons, boolean offerRevComp)
/* Print out HTML FORM entries for sequence display options. */
{
char *casing, *repMasking;

puts("\n<H3> Sequence Formatting Options: </H3>\n");

casing = cartCgiUsualString(cart, "hgSeq.casing", "exon");
if (canDoIntrons)
    {
    cgiMakeRadioButton("hgSeq.casing", "exon", sameString(casing, "exon"));
    if (canDoUTR)
	puts("Exons in upper case, everything else in lower case. <BR>");
    else
	puts("Blocks in upper case, everything else in lower case. <BR>");
    }
if (canDoUTR)
    {
    if (sameString(casing, "exon") && !canDoIntrons)
	casing = "cds";
    cgiMakeRadioButton("hgSeq.casing", "cds", sameString(casing, "cds"));
    puts("CDS in upper case, UTR in lower case. <BR>");
    }
if ((sameString(casing, "exon") && !canDoIntrons) ||
    (sameString(casing, "cds") && !canDoUTR))
    casing = "upper";
cgiMakeRadioButton("hgSeq.casing", "upper", sameString(casing, "upper"));
puts("All upper case. <BR>");
cgiMakeRadioButton("hgSeq.casing", "lower", sameString(casing, "lower"));
puts("All lower case. <BR>");

cgiMakeCheckBox("hgSeq.maskRepeats",
		cartCgiUsualBoolean(cart, "hgSeq.maskRepeats", FALSE));
puts("Mask repeats: ");

repMasking = cartCgiUsualString(cart, "hgSeq.repMasking", "lower");
cgiMakeRadioButton("hgSeq.repMasking", "lower",
		   sameString(repMasking, "lower"));
puts(" to lower case ");
cgiMakeRadioButton("hgSeq.repMasking", "N", sameString(repMasking, "N"));
puts(" to N <BR>");
if (offerRevComp)
    {
    cgiMakeCheckBox("hgSeq.revComp",
		    cartCgiUsualBoolean(cart, "hgSeq.revComp", FALSE));
    puts("Reverse complement (get \'-\' strand sequence)");
    }
}


void hgSeqOptionsHtiCart(struct hTableInfo *hti, struct cart *cart)
/* Print out HTML FORM entries for gene region and sequence display options. 
 * Use defaults from CGI and cart. */
{
boolean canDoUTR, canDoIntrons, offerRevComp;

if (hti == NULL)
    {
    canDoUTR = canDoIntrons = FALSE;
    offerRevComp = TRUE;
    }
else
    {
    canDoUTR = hti->hasCDS;
    canDoIntrons = hti->hasBlocks;
    offerRevComp = (hti->strandField[0] == 0);
    }
hgSeqFeatureRegionOptions(cart, canDoUTR, canDoIntrons);
hgSeqDisplayOptions(cart, canDoUTR, canDoIntrons, offerRevComp);
}


#ifdef NEVER
void hgSeqOptionsHti(struct hTableInfo *hti)
/* Print out HTML FORM entries for gene region and sequence display options.
 * Use defaults from CGI. */
{
hgSeqOptionsHtiCart(hti, NULL);
}
#endif /* NEVER */


void hgSeqOptions(struct cart *cart, char *db, char *table)
/* Print out HTML FORM entries for gene region and sequence display options. */
{
struct hTableInfo *hti;
char chrom[32];
char rootName[256];

if ((table == NULL) || (table[0] == 0))
    {
    hti = NULL;
    }
else
    {
    hParseTableName(db, table, rootName, chrom);
    hti = hFindTableInfo(db, chrom, rootName);
    if (hti == NULL)
	webAbort("Error", "Could not find table info for table %s (%s)",
		 rootName, table);
    }
hgSeqOptionsHtiCart(hti, cart);
}

static void hgSeqConcatRegionsDb(char *db, char *chrom, char strand, char *name,
			  int rCount, unsigned *rStarts, unsigned *rSizes,
			  boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions. */
{
// Note: this code use to generate different sequence ids if the global
// database in hdb was different than the db parameter.  This functionality
// has been removed since the global database was removed and it didn't
// appear to be used.

struct dnaSeq *rSeq = NULL;
struct dnaSeq *cSeq = NULL;
char recName[256];
int seqStart, seqEnd;
int offset, cSize;
int i;
int chromSize = hgSeqChromSize(db, chrom);
boolean isRc     = (strand == '-') || cgiBoolean("hgSeq.revComp");
boolean maskRep  = cgiBoolean("hgSeq.maskRepeats");
int padding5     = cgiOptionalInt("hgSeq.padding5", 0);
int padding3     = cgiOptionalInt("hgSeq.padding3", 0);
char *casing     = cgiString("hgSeq.casing");
char *repMasking = cgiString("hgSeq.repMasking");
char *granularity  = cgiOptionalString("hgSeq.granularity");
boolean concatRegions = granularity && sameString("gene", granularity);

if (rCount < 1)
    return;

/* Don't support padding if granularity is gene (i.e. concat'ing all). */
if (concatRegions)
    {
    padding5 = padding3 = 0;
    }

i = rCount - 1;
seqStart = rStarts[0]             - (isRc ? padding3 : padding5);
seqEnd   = rStarts[i] + rSizes[i] + (isRc ? padding5 : padding3);
/* Padding might push us off the edge of the chrom; if so, truncate: */
if (seqStart < 0)
    {
    if (isRc)
	padding3 += seqStart;
    else
	padding5 += seqStart;
    seqStart = 0;
    }

/* if we know the chromSize, don't pad out beyond it */
if ((chromSize > 0) && (seqEnd > chromSize))
    {
    if (isRc)
	padding5 += (chromSize - seqEnd);
    else
	padding3 += (chromSize - seqEnd);
    seqEnd = chromSize;
    }
if (seqEnd <= seqStart)
    {
    printf("# Null range for %s_%s (range=%s:%d-%d 5'pad=%d 3'pad=%d)\n",
	   db, 
	   name,
	   chrom, seqStart+1, seqEnd,
	   padding5, padding3);
    return;
    }
if (maskRep)
    {
    rSeq = hDnaFromSeq(db, chrom, seqStart, seqEnd, dnaMixed);
    if (sameString(repMasking, "N"))
	lowerToN(rSeq->dna, strlen(rSeq->dna));
    if (!sameString(casing, "upper"))
	tolowers(rSeq->dna);
    }
else if (sameString(casing, "upper"))
    rSeq = hDnaFromSeq(db, chrom, seqStart, seqEnd, dnaUpper);
else
    rSeq = hDnaFromSeq(db, chrom, seqStart, seqEnd, dnaLower);

/* Handle casing and compute size of concatenated sequence */
cSize = 0;
for (i=0;  i < rCount;  i++)
    {
    if ((sameString(casing, "exon") && exonFlags[i]) ||
	(sameString(casing, "cds") && cdsFlags[i]))
	{
	int rStart = rStarts[i] - seqStart;
	toUpperN(rSeq->dna+rStart, rSizes[i]);
	}
    cSize += rSizes[i];
    }
cSize += (padding5 + padding3);
AllocVar(cSeq);
cSeq->dna = needLargeMem(cSize+1);
cSeq->size = cSize;

offset = 0;
for (i=0;  i < rCount;  i++)
    {
    int start = rStarts[i] - seqStart;
    int size  = rSizes[i];
    if (i == 0)
	{
	start -= (isRc ? padding3 : padding5);
	assert(start == 0);
	size  += (isRc ? padding3 : padding5);
	}
    if (i == rCount-1)
	{
	size  += (isRc ? padding5 : padding3);
	}
    memcpy(cSeq->dna+offset, rSeq->dna+start, size);
    offset += size;
    }
assert(offset == cSeq->size);
cSeq->dna[offset] = 0;
freeDnaSeq(&rSeq);

if (isRc)
    reverseComplement(cSeq->dna, cSeq->size);

safef(recName, sizeof(recName),
      "%s_%s range=%s:%d-%d 5'pad=%d 3'pad=%d "
      "strand=%c repeatMasking=%s",
      db, 
      name,
      chrom, seqStart+1, seqEnd,
      padding5, padding3,
      (isRc ? '-' : '+'),
      (maskRep ? repMasking : "none"));
faWriteNext(stdout, recName, cSeq->dna, cSeq->size);
freeDnaSeq(&cSeq);
}


static void hgSeqRegionsAdjDb(char *db, char *chrom, char strand, char *name,
		       boolean concatRegions, boolean concatAdjacent,
		       int rCount, unsigned *rStarts, unsigned *rSizes,
		       boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions, 
 * optionally concatenating adjacent exons. */
{
if (concatRegions || (rCount == 1))
    {
    hgSeqConcatRegionsDb(db, chrom, strand, name,
			 rCount, rStarts, rSizes, exonFlags, cdsFlags);
    }
else
    {
    int i, count;
    boolean isRc = (strand == '-');
    char rName[256];
    for (i=0,count=0;  i < rCount;  i++,count++)
	{
	int j, jEnd, len, lo, hi;
	snprintf(rName, sizeof(rName), "%s_%d", name, count);
	j = (isRc ? (rCount - i - 1) : i);
	jEnd = (isRc ? (j - 1) : (j + 1));
	if (concatAdjacent && exonFlags[j])
	    {
	    lo = (isRc ? jEnd       : (jEnd - 1));
	    hi = (isRc ? (jEnd + 1) : jEnd);
	    while ((i < rCount) &&
		   ((rStarts[lo] + rSizes[lo]) == rStarts[hi]) &&
		   exonFlags[jEnd])
		{
		i++;
		jEnd = (isRc ? (jEnd - 1) : (jEnd + 1));
		lo = (isRc ? jEnd       : (jEnd - 1));
		hi = (isRc ? (jEnd + 1) : jEnd);
		}
	    }
	len = (isRc ? (j - jEnd) : (jEnd - j));
	lo  = (isRc ? (jEnd + 1) : j);
	hgSeqConcatRegionsDb(db, chrom, strand, rName,
			     len, &rStarts[lo], &rSizes[lo], &exonFlags[lo],
			     &cdsFlags[lo]);
	}
    }
}

static void hgSeqRegionsDb(char *db, char *chrom, char strand, char *name,
		    boolean concatRegions,
		    int rCount, unsigned *rStarts, unsigned *rSizes,
		    boolean *exonFlags, boolean *cdsFlags)
/* Concatenate and print out dna for a series of regions. */
{
hgSeqRegionsAdjDb(db, chrom, strand, name, concatRegions, FALSE,
		  rCount, rStarts, rSizes, exonFlags, cdsFlags);
}

static int maxStartsOffset = 0;

static void addFeature(int *count, unsigned *starts, unsigned *sizes,
		boolean *exonFlags, boolean *cdsFlags,
                int start, int size, boolean eFlag, boolean cFlag,
                int chromSize)
{
if (start < 0)
    {
    size += start;
    start = 0;
    }

/* if we know the chromSize, don't try to get more than's there */
if ((chromSize > 0) && (start + size > chromSize))
    size = chromSize - start;
if (*count > maxStartsOffset)
    errAbort("addFeature: overflow (%d > %d)", *count, maxStartsOffset);
starts[*count]    = start;
sizes[*count]     = size;
exonFlags[*count] = eFlag;
cdsFlags[*count]  = cFlag;
(*count)++;
}

void hgSeqRange(char *db, char *chrom, int chromStart, int chromEnd, char strand,
		char *name)
/* Print out dna sequence for the given range. */
{
int count = 0;
unsigned starts[1];
unsigned sizes[1];
boolean exonFlags[1];
boolean cdsFlags[1];
int chromSize = hgSeqChromSize(db, chrom);
maxStartsOffset = 0;
addFeature(&count, starts, sizes, exonFlags, cdsFlags,
	   chromStart, chromEnd - chromStart, FALSE, FALSE, chromSize);

hgSeqRegionsDb(db, chrom, strand, name, FALSE, count, starts, sizes, exonFlags,
               cdsFlags);
}


int hgSeqBed(char *db, struct hTableInfo *hti, struct bed *bedList)
/* Print out dna sequence from the given database of all items in bedList.  
 * hti describes the bed-compatibility level of bedList items.  
 * Returns number of FASTA records printed out. */
{
struct bed *bedItem;
char itemName[128];
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
boolean concatAdjacent = (cgiBooleanDefined("hgSeq.splitCDSUTR") &&
			  (! cgiBoolean("hgSeq.splitCDSUTR")));
boolean isCDS, doIntron;
boolean canDoUTR, canDoIntrons;

/* catch a special case: introns selected, but no exons -> include all introns
 * instead of qualifying intron with exon flags. */
if (intron && !(utrExon5 || cdsExon || utrExon3))
    {
    utrIntron5 = cdsIntron = utrIntron3 = TRUE;
    }

canDoUTR = hti->hasCDS;
canDoIntrons = hti->hasBlocks;

rowCount = totalCount = 0;
for (bedItem = bedList;  bedItem != NULL;  bedItem = bedItem->next)
    {
    if (bedItem->blockCount == 0) /* An intersection may have made hti unreliable. */
        canDoIntrons = FALSE;
    rowCount++;
    int chromSize = hgSeqChromSize(db, bedItem->chrom);
    // bed: translate relative starts to absolute starts
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	bedItem->chromStarts[i] += bedItem->chromStart;
	}
    isRc = (bedItem->strand[0] == '-');
    // here's the max # of feature regions:
    if (canDoIntrons)
	count = 4 + (2 * bedItem->blockCount);
    else
	count = 5;
    maxStartsOffset = count-1;
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
		   FALSE, FALSE, chromSize);
	}
    else if (isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   (bedItem->chromStart - downstreamSize), downstreamSize,
		   FALSE, FALSE, chromSize);
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
			       TRUE, FALSE, chromSize);
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
			       FALSE, FALSE, chromSize);
		    }
		}
	    else if (bedItem->chromStarts[i] < bedItem->thickEnd)
		{
		if ((bedItem->chromStarts[i] < bedItem->thickStart) &&
		    ((bedItem->chromStarts[i] + bedItem->blockSizes[i]) >
		     bedItem->thickEnd))
		    {
		    if ((!isRc && utrExon5)	  || (isRc && utrExon3))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->chromStarts[i],
				   (bedItem->thickStart -
				    bedItem->chromStarts[i]),
				   TRUE, FALSE, chromSize);
			}
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickStart,
				   (bedItem->thickEnd - bedItem->thickStart),
				   TRUE, TRUE, chromSize);
			}
		    if ((!isRc && utrExon3)	  || (isRc && utrExon5))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickEnd,
				   (bedItem->chromStarts[i] +
				    bedItem->blockSizes[i] -
				    bedItem->thickEnd),
				   TRUE, FALSE, chromSize);
			}
		    }
		else if (bedItem->chromStarts[i] < bedItem->thickStart)
		    {
		    if ((!isRc && utrExon5)	  || (isRc && utrExon3))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->chromStarts[i],
				   (bedItem->thickStart -
				    bedItem->chromStarts[i]),
				   TRUE, FALSE, chromSize);
			}
		    if (cdsExon)
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickStart,
				   (bedItem->chromStarts[i] +
				    bedItem->blockSizes[i] -
				    bedItem->thickStart),
				   TRUE, TRUE, chromSize);
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
				   TRUE, TRUE, chromSize);
			}
		    if ((!isRc && utrExon3)	  || (isRc && utrExon5))
			{
			addFeature(&count, starts, sizes, exonFlags, cdsFlags,
				   bedItem->thickEnd,
				   (bedItem->chromStarts[i] +
				    bedItem->blockSizes[i] -
				    bedItem->thickEnd),
				   TRUE, FALSE, chromSize);
			}
		    }
		else if (cdsExon)
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       bedItem->chromStarts[i], bedItem->blockSizes[i],
			       TRUE, TRUE, chromSize);
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
			       FALSE, isCDS, chromSize);
		    }
		}
	    else
		{
		if ((!isRc && utrExon3)   || (isRc && utrExon5))
		    {
		    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			       bedItem->chromStarts[i], bedItem->blockSizes[i],
			       TRUE, FALSE, chromSize);
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
			       FALSE, FALSE, chromSize);
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
			   TRUE, FALSE, chromSize);
		}
	    if (cdsIntron && (i < bedItem->blockCount - 1))
		{
		addFeature(&count, starts, sizes, exonFlags, cdsFlags,
			   (bedItem->chromStarts[i] + bedItem->blockSizes[i]),
			   (bedItem->chromStarts[i+1] -
			    bedItem->chromStarts[i] -
			    bedItem->blockSizes[i]),
			   FALSE, FALSE, chromSize);
		}
	    }
	}
    else if (canDoUTR)
	{
	if (bedItem->thickStart == 0 && bedItem->thickEnd == 0)
	    bedItem->thickStart = bedItem->thickEnd = bedItem->chromStart;
	if ((!isRc && utrExon5)   || (isRc && utrExon3))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->chromStart,
		       (bedItem->thickStart - bedItem->chromStart),
		       TRUE, FALSE, chromSize);
	    }
	if (cdsExon)
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->thickStart,
		       (bedItem->thickEnd - bedItem->thickStart),
		       TRUE, TRUE, chromSize);
	    }
	if ((!isRc && utrExon3)   || (isRc && utrExon5))
	    {
	    addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		       bedItem->thickEnd,
		       (bedItem->chromEnd - bedItem->thickEnd),
		       TRUE, FALSE, chromSize);
	    }
	}
    else
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromStart,
		   (bedItem->chromEnd - bedItem->chromStart),
		   TRUE, FALSE, chromSize);
	}
    if (!isRc && downstream && (downstreamSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromEnd, downstreamSize, FALSE, FALSE, chromSize);
	}
    else if (isRc && promoter && (promoterSize > 0))
	{
	addFeature(&count, starts, sizes, exonFlags, cdsFlags,
		   bedItem->chromEnd, promoterSize, FALSE, FALSE, chromSize);
	}
    snprintf(itemName, sizeof(itemName), "%s_%s", hti->rootName, bedItem->name);
    hgSeqRegionsAdjDb(db, bedItem->chrom, bedItem->strand[0], itemName,
		      concatRegions, concatAdjacent,
		      count, starts, sizes, exonFlags, cdsFlags);
    totalCount += count;
    freeMem(starts);
    freeMem(sizes);
    freeMem(exonFlags);
    freeMem(cdsFlags);
    }
return totalCount;
}


int hgSeqItemsInRange(char *db, char *table, char *chrom, int chromStart,
                      int chromEnd, char *sqlConstraints)
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  Return number of items. */
{
struct hTableInfo *hti;
struct bed *bedList;
char rootName[256];
char parsedChrom[32];
int itemCount;

hParseTableName(db, table, rootName, parsedChrom);
hti = hFindTableInfo(db, chrom, rootName);
if (hti == NULL)
    webAbort("Error", "Could not find table info for table %s (%s)",
	     rootName, table);
bedList = hGetBedRange(db, table, chrom, chromStart, chromEnd,
		       sqlConstraints);

itemCount = hgSeqBed(db, hti, bedList);
bedFreeList(&bedList);
return itemCount;
}
