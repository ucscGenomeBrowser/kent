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

static char const rcsid[] = "$Id: gffOut.c,v 1.8 2004/08/28 21:50:37 kent Exp $";

static void addGffLineFromBed(struct gffLine **pGffList, struct bed *bed,
			      char *source, char *feature,
			      int start, int end, char frame, char *txName)
/* Create a gffLine from a bed and line-specific parameters, add to list. */
{
struct gffLine *gff;
AllocVar(gff);
gff->seq = cloneString(bed->chrom);
gff->source = cloneString(source);
gff->feature = cloneString(feature);
gff->start = start;
gff->end = end;
gff->score = bed->score;
gff->strand = bed->strand[0];
gff->frame = frame;
gff->group = cloneString(txName);
gff->geneId = cloneString(bed->name);
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
			      char *source, boolean gtf2StopCodons)
/* Translate a (list of) bed into list of gffLine elements. */
{
struct hash *nameHash = newHash(20);
struct gffLine *gffList = NULL;
struct bed *bed;
int i, j, s, e;
char txName[256];

for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    /* Enforce unique transcript_ids. */
    struct hashEl *hel = hashLookup(nameHash, bed->name);
    int dupCount = (hel != NULL ? ptToInt(hel->val) : 0);
    if (dupCount > 0)
	{
	safef(txName, sizeof(txName), "%s_dup%d", bed->name, dupCount);
	hel->val = NULL + dupCount + 1;
	}
    else
	{
	safef(txName, sizeof(txName), "%s", bed->name);
	hashAddInt(nameHash, bed->name, 1);
	}
    if (hti->hasBlocks && hti->hasCDS)
	{
	char *frames = needMem(bed->blockCount);
	boolean gotFirstCds = FALSE;
	int nextPhase = 0;
	int startIndx = 0;
	int stopIndx = 0;
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
    else if (hti->hasBlocks)
	{
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.',
			      txName);
	    }
	}
    else if (hti->hasCDS)
	{
	if (bed->thickStart > bed->chromStart)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			      bed->thickStart, '.', txName);
	    }
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

void doOutGff(struct trackDb *track, struct sqlConnection *conn)
/* Save as GFF. */
{
char *table = track->tableName;
struct hTableInfo *hti = getHti(database, table);
struct bed *bedList;
struct gffLine *gffList, *gffPtr;
char source[64];
int itemCount;
// Would be nice to allow user to select this, but I don't want to 
// make an options page for just one param... any others?  
// ? exon / CDS ?
boolean gtf2StopCodons = FALSE;
struct region *region, *regionList = getRegions();

textOpen();

safef(source, sizeof(source), "%s_%s", database, table);
itemCount = 0;
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    bedList = cookedBedList(conn, track, region, lm);
    gffList = bedToGffLines(bedList, hti, source, gtf2StopCodons);
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

