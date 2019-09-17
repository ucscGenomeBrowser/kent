/* gffOut - output GFF (from bed data structures). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "trackDb.h"
#include "bed.h"
#include "hdb.h"
#include "gff.h"
#include "hgTables.h"


static void addGffLineFromBed(struct bed *bed, char *source, char *feature,
			      int start, int end, char frame, char *txName)
/* Create a gffLine from a bed and line-specific parameters and print it out. */
{
struct gffLine gff;
ZeroVar(&gff);
char strand;
gff.seq = bed->chrom;
gff.source = source;
gff.feature = feature;
gff.start = start;
gff.end = end;
gff.score = bed->score;
strand = bed->strand[0];
if (strand != '+' && strand != '-')
    strand = '.';
gff.strand = strand;
gff.frame = frame;
gff.group = txName;
if (bed->name != NULL)
    gff.geneId = bed->name;
else
    {
    static int namelessIx = 0;
    char buf[64];
    safef(buf, sizeof(buf), "gene%d", ++namelessIx);
    gff.geneId = buf;
    }
gffTabOut(&gff, stdout);
}


static char *computeFrames(struct bed *bed, char *exonFrames, int *retStartIndx, int *retStopIndx)
/* Compute frames, in order dictated by strand.  bed must be BED12. 
 * Convert exonFrames to gtf frame if available. */
{
int *ef;
if (exonFrames)
    {
    int efSize = 0;
    sqlSignedStaticArray(exonFrames, &ef, &efSize); // not thread safe.
    if (efSize != bed->blockCount)
	errAbort("Unexpected error, name=%s blockCount=%d but exonFrames size = %d", bed->name, bed->blockCount, efSize);
    }
char *frames = needMem(bed->blockCount);
boolean gotFirstCds = FALSE;
int nextPhase = 0, startIndx = 0, stopIndx = 0;
// If lack of thick region has been represented this way, fix:
if (bed->thickStart == 0 && bed->thickEnd == 0)
    bed->thickStart = bed->thickEnd = bed->chromStart;
int i;
for (i=0;  i < bed->blockCount;  i++)
    {
    int j = (bed->strand[0] == '-') ? bed->blockCount-i-1 : i;
    int exonStart = bed->chromStart + bed->chromStarts[j];
    int exonEnd = exonStart + bed->blockSizes[j];
    if ((exonStart < bed->thickEnd) && (exonEnd > bed->thickStart))
	{
	int cdsS = max(exonStart, bed->thickStart);
	int cdsE = min(exonEnd, bed->thickEnd);
	int cdsSize = cdsE - cdsS;
	if (! gotFirstCds)
	    {
	    gotFirstCds = TRUE;
	    startIndx = j;
	    }
	if (exonFrames)
	    {
	    int n = ef[j];
	    char c = '.';     // -1 exonFrame becomes '.' in gtf 
	    if (n == 0)
		c = '0';
	    else if (n == 1)  // gtf frames are really "phases", so 1 and 2 swap.
		c = '2';
	    else if (n == 2)
		c = '1';
	    frames[j] = c;
	    }
	else
	    {
	    frames[j] = '0' + nextPhase;
	    nextPhase = (3 + ((nextPhase - cdsSize) % 3)) % 3;
	    }
	stopIndx = j;
	}
    else
	{
	frames[j] = '.';
	}
    }
if (retStartIndx)
    *retStartIndx = startIndx;
if (retStopIndx)
    *retStopIndx = stopIndx;
return frames;
}


static void addStartStopCodon(struct bed *bed, int exonIndx, int anchor, int offset, char *codon,
			      char *source, char *txName)
/* Output a start or stop codon -- if it is split across multiple exons, output multiple lines. 
 * anchor must fall in the exonIndx exon of bed.
 * If offset is positive, we are computing an end coord; if negative, a start coord. */
{
int simpleAnswer = anchor + offset;
int exonStart = bed->chromStart + bed->chromStarts[exonIndx];
int exonEnd = exonStart + bed->blockSizes[exonIndx];
if ((offset >= 0 && (anchor >= exonEnd || anchor < exonStart)) ||
    (offset < 0 && (anchor > exonEnd || anchor <= exonStart)))
    errAbort("addStartStopCodon: anchor %d is not in exon %d [%d,%d]",
	     anchor, exonIndx, exonStart, exonEnd);
if (offset < 0)
    {
    if (simpleAnswer < exonStart)
	{
	if (exonIndx >= 1)
	    {
	    int stillNeeded = simpleAnswer - exonStart;
	    int prevExonEnd = (bed->chromStart + bed->chromStarts[exonIndx-1] +
			       bed->blockSizes[exonIndx-1]);
	    addStartStopCodon(bed, exonIndx-1, prevExonEnd, stillNeeded, codon, source, txName);
	    }
	addGffLineFromBed(bed, source, codon, exonStart, anchor, '.', txName);
	}
    else
	addGffLineFromBed(bed, source, codon, simpleAnswer, anchor, '.', txName);
    }
else if (offset > 0)
    {
    if (simpleAnswer > exonEnd)
	{
	addGffLineFromBed(bed, source, codon, anchor, exonEnd, '.', txName);
	if (exonIndx < bed->blockCount - 1)
	    {
	    int stillNeeded = simpleAnswer - exonEnd;
	    int nextExonStart = bed->chromStart + bed->chromStarts[exonIndx+1];
	    addStartStopCodon(bed, exonIndx+1, nextExonStart, stillNeeded, codon, source, txName);
	    }
	}
    else
	addGffLineFromBed(bed, source, codon, anchor, simpleAnswer, '.', txName);
    }
}

static void addCdsStartStop(struct bed *bed, char *source, int exonCdsStart, int exonCdsEnd,
			    char *frames, int exonIndx, int cdsStartIndx, int cdsStopIndx,
			    boolean gtf2StopCodons, char *txName)
/* Output a CDS record for the trimmed CDS portion of exonIndx, and a start_codon or
 * stop_codon if applicable. */
{
boolean isRc = (bed->strand[0] == '-');
/* start_codon (goes first for + strand) overlaps with CDS */
if ((exonIndx == cdsStartIndx) && !isRc)
    addStartStopCodon(bed, exonIndx, exonCdsStart, 3, "start_codon", source, txName);
/* If gtf2StopCodons is set, then we follow the GTF2 convention of excluding
 * the stop codon from the CDS region.  In other GFF flavors, stop_codon is
 * part of the CDS, which is the case in our table coords too.
 * This function would already be complicated enough without gtf2StopCodons,
 * due to strand and the possibility of a start or stop codon being split
 * across exons.  Excluding the stop codon from CDS complicates it further
 * because cdsEnd on the current exon may affect the CDS line of the previous
 * or next exon.  The cdsPortion* variables below are the CDS extremities that
 * may have the stop codon excised. */
if (exonIndx == cdsStopIndx)
    {
    if (isRc)
	{
	addStartStopCodon(bed, exonIndx, exonCdsStart, 3, "stop_codon", source, txName);
	int cdsPortionStart = gtf2StopCodons ? exonCdsStart + 3 : exonCdsStart;
	if (cdsPortionStart < exonCdsEnd)
	    addGffLineFromBed(bed, source, "CDS", cdsPortionStart, exonCdsEnd,
			      frames[exonIndx], txName);
	}
    else
	{
	int cdsPortionEnd = gtf2StopCodons ? exonCdsEnd - 3 : exonCdsEnd;
	if (cdsPortionEnd > exonCdsStart)
	    addGffLineFromBed(bed, source, "CDS", exonCdsStart, cdsPortionEnd,
			      frames[exonIndx], txName);
	addStartStopCodon(bed, exonIndx, exonCdsEnd, -3, "stop_codon", source, txName);
	}
    }
else
    {
    int cdsPortionStart = exonCdsStart;
    int cdsPortionEnd = exonCdsEnd;
    if (gtf2StopCodons)
	{
	if (isRc && exonIndx - 1 == cdsStopIndx)
	    {
	    int lastExonEnd = (bed->chromStart + bed->chromStarts[exonIndx-1] +
			       bed->blockSizes[exonIndx-1]);
	    int basesWereStolen = 3 - (lastExonEnd - bed->thickStart);
	    if (basesWereStolen > 0)
		cdsPortionStart += basesWereStolen;
	    }
	if (!isRc && exonIndx + 1 == cdsStopIndx)
	    {
	    int nextExonStart = bed->chromStart + bed->chromStarts[exonIndx+1];
	    int basesWillBeStolen = 3 - (bed->thickEnd - nextExonStart);
	    if (basesWillBeStolen > 0)
		cdsPortionEnd -= basesWillBeStolen;
	    }
	}
    if (cdsPortionEnd > cdsPortionStart)
	addGffLineFromBed(bed, source, "CDS", cdsPortionStart, cdsPortionEnd,
			  frames[exonIndx], txName);
    }
/* start_codon (goes last for - strand) overlaps with CDS */
if ((exonIndx == cdsStartIndx) && isRc)
    addStartStopCodon(bed, exonIndx, exonCdsEnd, -3, "start_codon", source, txName);
}


static int bedToGffLines(struct bed *bedList, struct slName *exonFramesList, struct hTableInfo *hti,
			 int fieldCount, char *source, boolean gtf2StopCodons)
/* Translate a (list of) bed into gff and print out.
 * Note that field count (perhaps reduced by bitwise intersection)
 * can in effect override hti. */
{
if (! bedList)
    return 0;
struct hash *nameHash = newHash(20);
struct bed *bed;
struct slName *exonFrames = exonFramesList;
int i, exonStart, exonEnd;
char txName[256];
int itemCount = 0;
static int namelessIx = 0;

for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    /* Enforce unique transcript_ids. */
    if (bed->name != NULL)
	{
	struct hashEl *hel = hashLookup(nameHash, bed->name);
	int dupCount = (hel != NULL ? ptToInt(hel->val) : 0);
	if (dupCount > 0)
	    {
	    safef(txName, sizeof(txName), "%s_dup%d", bed->name, dupCount);
	    hel->val = intToPt(dupCount + 1);
	    }
	else
	    {
	    safef(txName, sizeof(txName), "%s", bed->name);
	    hashAddInt(nameHash, bed->name, 1);
	    }
	}
    else
	safef(txName, sizeof(txName), "tx%d", ++namelessIx);
    if (hti->hasBlocks && hti->hasCDS && fieldCount > 4)
	{
	/* first pass: compute frames, in order dictated by strand. */
	int startIndx = 0, stopIndx = 0;
	char *frames = NULL;
	char *ef = NULL;
	if (exonFramesList)
    	    ef = exonFrames->name;
	frames = computeFrames(bed, ef, &startIndx, &stopIndx);

	/* second pass: one exon (possibly CDS, start/stop_codon) per block. */
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    exonStart = bed->chromStart + bed->chromStarts[i];
	    exonEnd = exonStart + bed->blockSizes[i];
	    if ((exonStart < bed->thickEnd) && (exonEnd > bed->thickStart))
		{
		int exonCdsStart = max(exonStart, bed->thickStart);
		int exonCdsEnd = min(exonEnd, bed->thickEnd);
		addCdsStartStop(bed, source, exonCdsStart, exonCdsEnd,
				frames, i, startIndx, stopIndx, gtf2StopCodons, txName);
		}
	    addGffLineFromBed(bed, source, "exon", exonStart, exonEnd, '.', txName);
	    }
	freeMem(frames);
	}
    else if (hti->hasBlocks && fieldCount > 4)
	{
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    exonStart = bed->chromStart + bed->chromStarts[i];
	    exonEnd = exonStart + bed->blockSizes[i];
	    addGffLineFromBed(bed, source, "exon", exonStart, exonEnd, '.', txName);
	    }
	}
    else if (hti->hasCDS && fieldCount > 4)
	{
	if (bed->thickStart == 0 && bed->thickEnd == 0)
	    bed->thickStart = bed->thickEnd = bed->chromStart;
	if (bed->thickStart > bed->chromStart)
	    {
	    addGffLineFromBed(bed, source, "exon", bed->chromStart, bed->thickStart, '.', txName);
	    }
	if (bed->thickEnd > bed->thickStart)
	    addGffLineFromBed(bed, source, "CDS", bed->thickStart, bed->thickEnd, '0', txName);
	if (bed->thickEnd < bed->chromEnd)
	    {
	    addGffLineFromBed(bed, source, "exon", bed->thickEnd, bed->chromEnd, '.', txName);
	    }
	}
    else
	{
	addGffLineFromBed(bed, source, "exon", bed->chromStart, bed->chromEnd, '.', txName);
	}
    itemCount++;
    if (exonFrames)
    	exonFrames = exonFrames->next;
    }
hashFree(&nameHash);
return itemCount;
}

static struct slName* getExonFrames(char *table, struct sqlConnection *conn, struct bed *bedList)
/* get real exonFrames if they are available */
{
struct slName* list = NULL;
struct bed *bed;
for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    // be super specific, the same name may align to multiple locations
    //  or even the same location with alternate splicing or exon structure.
    
    // convert bed block coordinates to exonStarts, exonEnds
    int i;
    struct dyString *exonStarts = newDyString(256);
    struct dyString *exonEnds = newDyString(256);
    for( i = 0 ; i < bed->blockCount; i++ )
	{
	int exonStart = bed->chromStart + bed->chromStarts[i];
	int exonEnd = exonStart + bed->blockSizes[i];
	dyStringPrintf(exonStarts, "%d,", exonStart);
	dyStringPrintf(exonEnds,   "%d,", exonEnd);
	}
    char sql[4096+strlen(exonStarts->string)+strlen(exonEnds->string)];

    sqlSafef(sql, sizeof sql, "select exonFrames "
	"from %s where " 
	"name = '%s' and "  
	"chrom = '%s' and "
	"strand = '%c' and "
	"txStart = %d and "
	"txEnd = %d and "
	"cdsStart = %d and "
	"cdsEnd = %d and "
        "exonCount = %d and "
        "exonStarts = '%s' and "
        "exonEnds = '%s'"
	, 
	table, 
	bed->name,
	bed->chrom,
	bed->strand[0],
	bed->chromStart,
	bed->chromEnd,
	bed->thickStart,
	bed->thickEnd,
	bed->blockCount,
	exonStarts->string,
	exonEnds->string
	);
    char *exonFrames = sqlQuickString(conn, sql);
    slNameAddHead(&list, exonFrames);
    dyStringFree(&exonStarts);
    dyStringFree(&exonEnds);
    }
slReverse(&list);
return list;
}

static struct hash *makeChromHashForTable(struct sqlConnection *conn, char *table)
/* Get a hash of all the chroms that are actually being used for the table.
 * This is helpful for assemblies with huge numbers of chroms. */
{
char query[1024];
// Make sure that the table has a chrom field.
// a bbi table will NOT have a chrom field
int cIdx = sqlFieldIndex(conn, table, "chrom");
if (cIdx < 0)
    return NULL;
sqlSafef(query, sizeof query, "select distinct chrom, 'dummyvalue' from %s", table);
struct hash *hash = sqlQuickHash(conn, query);
return hash;
}

void doOutGff(char *table, struct sqlConnection *conn, boolean outputGtf)
/* Save as GFF/GTF. */
{
struct hTableInfo *hti = getHti(database, table, conn);
struct bed *bedList;
struct hash *chromHash = NULL;
struct slName *exonFramesList = NULL;
char source[HDB_MAX_TABLE_STRING];
int itemCount;
struct region *region, *regionList = getRegions();

textOpen();

int efIdx = -1;
safef(source, sizeof(source), "%s_%s", database, table);
if (conn)
    {
    boolean simpleTableExists = sqlTableExists(conn, table);
    // simpleTable means not split table, not custom track
    // However it still can include bbi table with bam fileName path
    if (simpleTableExists)  // no tables having exonFrames are split tables anyway
        efIdx = sqlFieldIndex(conn, table, "exonFrames");
    itemCount = 0;
    int regionCount = slCount(regionList);
    // regionList can have many thousands of items e.g. rheMac3 has 34000 chroms!
    // This regionCount threshold should be just above the # chroms in the latest human assembly
    if (simpleTableExists && (regionCount > 500))
        {
        chromHash = makeChromHashForTable(conn, table);
        }
    }
// Note: code could be added here to extract exonFrames from bigGenePred

// Process each region
for (region = regionList; region != NULL; region = region->next)
    {
    if (chromHash && (!hashFindVal(chromHash, region->chrom)))
	    continue;
    struct lm *lm = lmInit(64*1024);
    int fieldCount;
    bedList = cookedBedList(conn, table, region, lm, &fieldCount);
    // Use exonFrames field if available for better accuracy instead of calculating from coordinates
    if (efIdx != -1) 
	exonFramesList = getExonFrames(table, conn, bedList);
    itemCount += bedToGffLines(bedList, exonFramesList, hti, fieldCount, source, outputGtf);
    lmCleanup(&lm);
    }
if (itemCount == 0)
    hPrintf(NO_RESULTS);
}

