/* gffOut - output GFF (from bed data structures). */

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

static char const rcsid[] = "$Id: gffOut.c,v 1.16 2006/06/12 23:06:11 angie Exp $";

static void addGffLineFromBed(struct gffLine **pGffList, struct bed *bed,
			      char *source, char *feature,
			      int start, int end, char frame, char *txName)
/* Create a gffLine from a bed and line-specific parameters, add to list. */
{
struct gffLine *gff;
char strand;
AllocVar(gff);
gff->seq = cloneString(bed->chrom);
gff->source = cloneString(source);
gff->feature = cloneString(feature);
gff->start = start;
gff->end = end;
gff->score = bed->score;
strand = bed->strand[0];
if (strand != '+' && strand != '-')
    strand = '.';
gff->strand = strand;
gff->frame = frame;
gff->group = cloneString(txName);
if (bed->name != NULL)
    gff->geneId = cloneString(bed->name);
else
    {
    static int namelessIx = 0;
    char buf[64];
    safef(buf, sizeof(buf), "gene%d", ++namelessIx);
    gff->geneId = cloneString(buf);
    }
slAddHead(pGffList, gff);
}


static void addCdsStartStop(struct gffLine **pGffList, struct bed *bed,
			    char *source, int s, int e, char *frames,
			    int i, int startIndx, int stopIndx,
			    boolean gtf2StopCodons, char *txName)
{
/* start_codon (goes first for + strand) overlaps with CDS */
if ((i == startIndx) && (bed->strand[0] != '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      s, s+3, '.', txName);
    }
/* stop codon does not overlap with CDS as of GTF2 */
if ((i == stopIndx) && gtf2StopCodons)
    {
    if (bed->strand[0] == '-')
	{
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  s, s+3, '.', txName);
	addGffLineFromBed(pGffList, bed, source, "CDS", s+3, e,
			  frames[i], txName);
	}
    else
	{
	addGffLineFromBed(pGffList, bed, source, "CDS", s, e-3,
			  frames[i], txName);
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  e-3, e, '.', txName);
	}
    }
else
    {
    addGffLineFromBed(pGffList, bed, source, "CDS", s, e,
		      frames[i], txName);
    }
/* start_codon (goes last for - strand) overlaps with CDS */
if ((i == startIndx) && (bed->strand[0] == '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      e-3, e, '.', txName);
    }
}


struct gffLine *bedToGffLines(struct bed *bedList, struct hTableInfo *hti,
			      int fieldCount, char *source, boolean gtf2StopCodons)
/* Translate a (list of) bed into list of gffLine elements. 
 * Note that field count (perhaps reduced by bitwise intersection)
 * can in effect override hti. */
{
struct hash *nameHash = newHash(20);
struct gffLine *gffList = NULL;
struct bed *bed;
int i, j, s, e;
char txName[256];
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
        {
	safef(txName, sizeof(txName), "tx%d\n", ++namelessIx);
	}
    if (hti->hasBlocks && hti->hasCDS && fieldCount > 4)
	{
	char *frames = needMem(bed->blockCount);
	boolean gotFirstCds = FALSE;
	int nextPhase = 0;
	int startIndx = 0;
	int stopIndx = 0;
	if (bed->thickStart == 0 && bed->thickEnd == 0)
	    bed->thickStart = bed->thickEnd = bed->chromStart;
	/* first pass: compute frames, in order dictated by strand. */
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    if (bed->strand[0] == '-')
		j = bed->blockCount-i-1;
	    else
		j = i;
	    s = bed->chromStart + bed->chromStarts[j];
	    e = s + bed->blockSizes[j];
	    if ((s < bed->thickEnd) && (e > bed->thickStart))
		{
		int cdsSize = e - s;
		if (e > bed->thickEnd)
		    cdsSize = bed->thickEnd - s;
		else if (s < bed->thickStart)
		    cdsSize = e - bed->thickStart;
		if (! gotFirstCds)
		    {
		    gotFirstCds = TRUE;
		    startIndx = j;
		    }
		frames[j] = '0' + nextPhase;
		nextPhase = (3 + ((nextPhase - cdsSize) % 3)) % 3;
		stopIndx = j;
		}
	    else
		{
		frames[j] = '.';
		}
	    }
	/* second pass: one exon (possibly CDS, start/stop_codon) per block. */
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    if ((s >= bed->thickStart) && (e <= bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, e, frames,
				i, startIndx, stopIndx, gtf2StopCodons,
				txName);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source,
				bed->thickStart, bed->thickEnd,
				frames, i, startIndx, stopIndx,
				gtf2StopCodons, txName);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickStart))
		{
		addCdsStartStop(&gffList, bed, source, bed->thickStart, e,
				frames, i, startIndx, stopIndx,
				gtf2StopCodons, txName);
		}
	    else if ((s < bed->thickEnd) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, bed->thickEnd,
				frames, i, startIndx, stopIndx,
				gtf2StopCodons, txName);
		}
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.',
			      txName);
	    }
	freeMem(frames);
	}
    else if (hti->hasBlocks && fieldCount > 4)
	{
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.',
			      txName);
	    }
	}
    else if (hti->hasCDS && fieldCount > 4)
	{
	if (bed->thickStart == 0 && bed->thickEnd == 0)
	    bed->thickStart = bed->thickEnd = bed->chromStart;
	if (bed->thickStart > bed->chromStart)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			      bed->thickStart, '.', txName);
	    }
	if (bed->thickEnd > bed->thickStart)
	    addGffLineFromBed(&gffList, bed, source, "CDS", bed->thickStart,
			      bed->thickEnd, '0', txName);
	if (bed->thickEnd < bed->chromEnd)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->thickEnd,
			      bed->chromEnd, '.', txName);
	    }
	}
    else
	{
	addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			  bed->chromEnd, '.', txName);
	}
    }
slReverse(&gffList);
hashFree(&nameHash);
return(gffList);
}

void doOutGff(char *table, struct sqlConnection *conn)
/* Save as GFF. */
{
struct hTableInfo *hti = getHti(database, table);
struct bed *bedList;
struct gffLine *gffList, *gffPtr;
char source[64];
int itemCount;
boolean gtf2StopCodons = FALSE;
struct region *region, *regionList = getRegions();

textOpen();

safef(source, sizeof(source), "%s_%s", database, table);
itemCount = 0;
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    int fieldCount;
    bedList = cookedBedList(conn, table, region, lm, &fieldCount);
    gffList = bedToGffLines(bedList, hti, fieldCount, source, gtf2StopCodons);
    bedList = NULL;
    lmCleanup(&lm);
    for (gffPtr = gffList;  gffPtr != NULL;  gffPtr = gffPtr->next)
	{
	gffTabOut(gffPtr, stdout);
	itemCount++;
	}
    slFreeList(&gffList);
    }
if (itemCount == 0)
    hPrintf("\n# No results returned from query.\n\n");
}

