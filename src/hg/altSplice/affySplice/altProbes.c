/* altProbes.c - Match probes to splicing events and do anaylysis. */
#include "common.h"
#include "bed.h"
#include "hash.h"
#include "options.h"
#include "chromKeeper.h"
#include "binRange.h"
#include "obscure.h"
#include "linefile.h"
#include "dystring.h"
#include "altGraphX.h"
#include "splice.h"
#include "hdb.h"
#include "dnaseq.h"
#include "dMatrix.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics_double.h>

struct altPath
/* Information about a path and associated bed probes. */
{
    struct altPath *next; /* Next in list. */
    int probeCount;        /* Number of probes on this path. */
    struct bed **beds;     /* Array of beds that are unique to this path. */
    int *bedTypes;         /* Are beds exons or splice junction probes. */
    double **expVals;      /* Expression values. */
    double **pVals;        /* Probability of expression. */
    double *avgExpVals;    /* Average value for each exp. */
    struct path *path;     /* Path that this altPath represents. */
    double score;               /* Score of being expressed overall. */
    double exonCorr;            /* Correlation of exon probe set with include junction if appropriate. */
    double exonSkipCorr;        /* Correlation of exon probe set with skip junction if appropriate. */
    double probCorr;            /* Correlation of exon probe set prob and include probs. */
    double probSkipCorr;        /* Correlation of exon probe set prob skip junction if appropriate. */
    double exonSjPercent;       /* Percentage of includes confirmed by exon probe set. */
    double sjExonPercent;       /* Percentage of exons confirmed by sj probe set. */
    int exonAgree;              /* Number of times exon agrees with matched splice junction */
    int exonDisagree;           /* Number of times exon disagrees with matched splice junction. */
    int sjAgree;                /* Number of times sj agrees with matched exon */
    int sjDisagree;             /* Number of times sj disagrees with matched exon. */
    int sjExp;                  /* Number of times sj expressed. */
    int exonExp;                /* Number of times exon expressed. */
};

struct altEvent 
/* Information about a particular alt-splicing event. Specifically a
 * collection of paths and the associated data. */
{
    struct altEvent *next;       /* Next in list. */
    struct splice *splice;       /* Splice event. */
    struct altPath *altPathList; /* Matching of paths to probe sets. */
    int altPathProbeCount;       /* Paths with associated probes. */
    int geneProbeCount;              /* Number of probes on this gene. */
    struct bed **geneBeds;           /* Array of beds that are unique to this gene. */
    double **geneExpVals;            /* Expression values. */
    double **genePVals;              /* Probability of expression. */
    double *avgExpVals;          /* Average value for each exp. */
    boolean isExpressed;         /* Is this altEvent Expressed. */
    boolean isAltExpressed;      /* Is this altEvent alt-expressed? */
};


static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"junctionProbes", OPTION_STRING},
    {"nonJunctProbes", OPTION_STRING},
    {"spliceFile", OPTION_STRING},
    {"probFile", OPTION_STRING},
    {"itensityFile", OPTION_STRING},
    {"db", OPTION_STRING},
    {"doPathRatios", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] =
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Bed format junction probe sets.",
    "Bed format non-junction (exon, etc) probe sets.",
    "Splice format file containing alternative splicing events.",
    "Data matrix with probability of transcription for each probe set.",
    "Data matrix with intensity estimates for each probe set.",
    "Database freeze of splices.",
    "Generate ratios of probe sets of splice events to each other."
};

/* Keep track of how many of each event are occuring. */
static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altOtherCount = 0;
static int alt3PrimeSoftCount = 0;
static int alt5PrimeSoftCount = 0;
static int altMutExclusiveCount = 0;
static int altControlCount = 0;

/* Keep track of how many of each event with probes */
static int alt5PrimeWProbeCount = 0;
static int alt3PrimeWProbeCount = 0;
static int altCassetteWProbeCount = 0;
static int altRetIntWProbeCount = 0;
static int altOtherWProbeCount = 0;
static int alt3PrimeSoftWProbeCount = 0;
static int alt5PrimeSoftWProbeCount = 0;
static int altMutExclusiveWProbeCount = 0;
static int altControlWProbeCount = 0;



void usage()
/** Print usage and quit. */
{
int i=0;
warn("altProbes - Match probes to splicing paths and analyze.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   ");
}

void logSpliceType(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type) 
    {
    case alt5Prime:
	alt5PrimeCount++;
	break;
    case alt3Prime: 
	alt3PrimeCount++;
	break;
    case altCassette:
	altCassetteCount++;
	break;
    case altRetInt:
	altRetIntCount++;
	break;
    case altOther:
	altOtherCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftCount++;
	break;
    case altMutExclusive:
	altMutExclusiveCount++;
	break;
    case altControl:
	altControlCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void logSpliceTypeWProbe(enum altSpliceType type)
/* Log the different types of splicing. */
{
switch (type) 
    {
    case alt5Prime:
	alt5PrimeWProbeCount++;
	break;
    case alt3Prime: 
	alt3PrimeWProbeCount++;
	break;
    case altCassette:
	altCassetteWProbeCount++;
	break;
    case altRetInt:
	altRetIntWProbeCount++;
	break;
    case altOther:
	altOtherWProbeCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftWProbeCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftWProbeCount++;
	break;
    case altMutExclusive:
	altMutExclusiveWProbeCount++;
	break;
    case altControl:
	altControlWProbeCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

boolean isJunctionBed(struct bed *bed)
/* Return TRUE if bed looks like a junction probe. Specifically
   two blocks, each 15bp. FALSE otherwise. */
{
if(bed->blockCount == 2 && bed->blockSizes[0] == 15 && bed->blockSizes[1] == 15)
    return TRUE;
return FALSE;
}

enum ggEdgeType pathEdgeType(unsigned char *vTypes, int v1, int v2)
/* Return edge type. */
{
if( (vTypes[v1] == ggHardStart || vTypes[v1] == ggSoftStart)  
    && (vTypes[v2] == ggHardEnd || vTypes[v2] == ggSoftEnd)) 
    return ggExon;
else if( (vTypes[v1] == ggHardEnd || vTypes[v1] == ggSoftEnd)  
	 && (vTypes[v2] == ggHardStart || vTypes[v2] == ggSoftStart)) 
    return ggSJ;
else
    return ggIntron;
}

boolean pathContainsIntron(struct splice *splice, struct path *path, char *chrom,
			   int chromStart, int chromEnd, char *strand)
/* Return TRUE if this path contains an intron (splice junction) that
   starts at chromStart and ends at chromEnd. */
{
int i = 0;
int *vPos = splice->vPositions;
int *verts = path->vertices;
if(differentString(splice->tName, chrom) || 
   differentString(splice->strand, strand))
    return FALSE;

/* Check the edges on the path. */
for(i = 0; i < path->vCount - 1; i++)
    {
    if(pathEdgeType(splice->vTypes, verts[i], verts[i+1]) == ggSJ)
	{
	if(chromStart == vPos[verts[i]] && chromEnd == vPos[verts[i+1]])
	    return TRUE;
	}
    }
return FALSE;
}

boolean pathContainsBlock(struct splice *splice, struct path *path, char *chrom,
			  int chromStart, int chromEnd, char *strand,
			  boolean allBases)
/* Return TRUE if this block is contained in the path, FALSE otherwise.
   If allBases then every base in chromStart-chromEnd must be covered
   by path. */
{
int i = 0;
int *vPos = splice->vPositions;
int *verts = path->vertices;
if(differentString(splice->tName, chrom) || 
   differentString(splice->strand, strand))
    return FALSE;

/* Check the first edge. */
if(path->upV != -1 && pathEdgeType(splice->vTypes, path->upV, 
				   verts[0]) == ggExon)
    {
    if(chromStart >= vPos[path->upV] && chromEnd <= vPos[verts[0]])
	return TRUE;
    else if(!allBases && rangeIntersection(chromStart, chromEnd, 
					   vPos[path->upV], vPos[verts[0]]) > 0)
	return TRUE;
    }

/* Check the last edge. */
if(path->downV != -1 && 
   pathEdgeType(splice->vTypes, verts[path->vCount - 1], path->downV) == ggExon)
    {
    if(chromStart >= vPos[verts[path->vCount - 1]] && 
       chromEnd <= vPos[path->downV])
	return TRUE;
    else if(!allBases && 
	    rangeIntersection(chromStart, chromEnd, 
			      vPos[verts[path->vCount - 1]], vPos[path->downV])  > 0)
	return TRUE;
    }

/* Check the edges on the path. */
for(i = 0; i < path->vCount - 1; i++)
    {
    if(pathEdgeType(splice->vTypes, verts[i], verts[i+1]) == ggExon)
	{
	if(chromStart >= vPos[verts[i]] && chromEnd <= vPos[verts[i+1]])
	    return TRUE;
	else if(!allBases && rangeIntersection(chromStart, chromEnd, 
					       vPos[verts[i]], vPos[verts[i+1]]) > 0)
	    return TRUE;
	}
    }
return FALSE;
}

boolean pathContainsBed(struct splice *splice, struct path *path, 
			struct bed *bed, boolean allBases, boolean intronsToo)
/* Return TRUE if this path contains this bed, FALSE otherwise. If
   allBases, the bed must be completely subsumed by the path. */
{
boolean containsBed = TRUE;
int i = 0;
assert(bed);
for(i = 0; i < bed->blockCount; i++)
    {
    int chromStart = bed->chromStart + bed->chromStarts[i];
    int chromEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];

    /* Check the exon. */
    containsBed &= pathContainsBlock(splice, path, bed->chrom, chromStart, 
				     chromEnd, bed->strand, allBases);
    

    /* Check the next intron. */
    if(intronsToo && i+1 != bed->blockCount) 
	containsBed &= pathContainsIntron(splice, path, bed->chrom, chromEnd,
					  bed->chromStarts[i+1]+chromStart, bed->strand);
    }
return containsBed;
}

void insertBedIntoPath(struct altEvent *altEvent, struct altPath *altPath,
		       struct bed *bed)
/* Add the cbed to this path. */
{
int oldCount = 0, newCount = 0;
assert(altPath);
oldCount = altPath->probeCount;
newCount = oldCount + 1;
ExpandArray(altPath->beds, oldCount, newCount);
ExpandArray(altPath->bedTypes, oldCount, newCount);
altPath->beds[oldCount] = bed;
altPath->probeCount = newCount;
}

int findProbeSetsForEvents(struct altEvent *altEvent)
/* Look for all of the beds that overlap a splice and match
   them the ones that are uniq to a path. Returns the number
   of probes that map to this splice. */
{
struct binElement *be = NULL, *beList = NULL;
struct altPath *altPath = NULL, *altMatchPath = NULL;
struct bed *bed = NULL;

assert(altEvent);
struct splice *splice = altEvent->splice;
/* Load all the beds that span this range. */
beList = chromKeeperFind(splice->tName, splice->tStart, splice->tEnd);
for(be = beList; be != NULL; be = be->next)
    {
    bed = be->val;
    if(!isJunctionBed(bed))
	continue;
    altMatchPath = NULL; /* Indicates that we haven't found a match for this bed. */
    for(altPath = altEvent->altPathList; altPath != NULL; altPath = altPath->next)
	{
	if(pathContainsBed(splice, altPath->path, bed, TRUE, TRUE))
	    {
	    if(altMatchPath == NULL)
		altMatchPath = altPath; /* So far unique match. */
	    else
		{ /* Not a unique match. */
		altMatchPath = NULL;
		break;
		}
	    }
	}

    /* If we've uniquely found a path for a bed insert it. */
    if(altMatchPath != NULL)
	insertBedIntoPath(altEvent, altMatchPath, bed);
    }

/* Count up how many paths have probes. */
for(altPath = altEvent->altPathList; altPath != NULL; altPath = altPath->next)
    if(altPath->probeCount > 0)
	altEvent->altPathProbeCount++;
}		    

void bedLoadChromKeeper()
/* Load the beds's in the file into the chromKeeper. */
{
char *db = optionVal("db", NULL);
char *bedFile = optionVal("junctionProbes", NULL);
struct bed *bed = NULL, *bedList = NULL;
if(db == NULL)
    errAbort("Must specify a database when loading beds.");
if(bedFile == NULL)
    errAbort("Must specify a file for loading beds.");
chromKeeperInit(db);
bedList = bedLoadAll(bedFile);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    chromKeeperAdd(bed->chrom, bed->chromStart, bed->chromEnd, bed);
    }
}

struct altEvent *altEventsFromSplices(struct splice *spliceList)
/* Create the altEvents and altPaths from the spliceList. */
{
struct splice *splice = NULL;
struct path *path = NULL;
struct altEvent *event = NULL, *eventList = NULL;
struct altPath *altPath = NULL, *altPathList = NULL;
for(splice = spliceList; splice != NULL; splice = splice->next)
    {
    AllocVar(event);
    event->splice = splice;
    for(path = splice->paths; path != NULL; path = path->next)
	{
	AllocVar(altPath);
	altPath->path = path;
	slAddHead(&event->altPathList, altPath);
	}
    slReverse(&event->altPathList);
    slAddHead(&eventList, event);
    }
slReverse(&eventList);
return eventList;
}

void fillInAltPathData(struct altPath *altPath, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Fill in the data for the probes that map to this
   altPath. */
{
int i = 0;
int pCount = altPath->probeCount;
AllocArray(altPath->expVals, pCount);
AllocArray(altPath->pVals, pCount);
AllocArray(altPath->avgExpVals, pCount);
for(i = 0; i < pCount; i++)
    {
    int index = hashIntValDefault(intenM->nameIndex, altPath->beds[i]->name, -1);
    if(index != -1)
	altPath->expVals[i] = intenM->matrix[index];
    index = hashIntValDefault(probM->nameIndex, altPath->beds[i]->name, -1);
    if(index != -1)
	altPath->pVals[i] = probM->matrix[index];
    }
}

void fillInEventData(struct altEvent *eventList, struct dMatrix *intenM,
		     struct dMatrix *probM)
/* Loop through the eventList and fill in the altPaths with
   data in intenM and probM. */
{
struct altEvent *event = NULL;
struct altPath *altPath = NULL;
for(event = eventList; event != NULL; event = event->next)
    {
    for(altPath = event->altPathList; altPath != NULL; altPath = altPath->next)
	{
	fillInAltPathData(altPath, intenM, probM);
	}
    }
}

void reportEventCounts()
{
fprintf(stderr, "alt 5': %d, with probes: %d\n", alt5PrimeCount, alt5PrimeWProbeCount);
fprintf(stderr, "alt 3': %d, with probes: %d\n", alt3PrimeCount, alt3PrimeWProbeCount);
fprintf(stderr, "alt Cass: %d, with probes: %d\n", altCassetteCount, altCassetteWProbeCount);
fprintf(stderr, "alt Ret. Int.: %d, with probes: %d\n", altRetIntCount, altRetIntWProbeCount);
fprintf(stderr, "alt Mutual Exclusive: %d, with probes: %d\n", altMutExclusiveCount, altMutExclusiveWProbeCount);
fprintf(stderr, "alt Txn Start: %d, with probes: %d\n", alt5PrimeSoftCount, alt5PrimeWProbeCount);
fprintf(stderr, "alt Txn End: %d, with probes: %d\n", alt3PrimeSoftCount, alt3PrimeWProbeCount);
fprintf(stderr, "alt Other: %d, with probes: %d\n", altOtherCount, altOtherWProbeCount);
fprintf(stderr, "alt Control: %d, with probes: %d\n", altControlCount, altControlWProbeCount);
}

void altProbes()
/* Top level function to map probes to paths and analyze. */
{
struct splice *spliceList = NULL;
struct altEvent *eventList = NULL, *event = NULL;
struct dMatrix *intenM = NULL, *probM = NULL;
char *intenIn = optionVal("intensityFile", NULL);
char *probIn = optionVal("probFile", NULL);
char *splicesIn = optionVal("spliceFile", NULL);
if(splicesIn == NULL)
    errAbort("Must specify a apliceFile");
warn("Loading splices from %s", splicesIn);
spliceList = spliceLoadAll(splicesIn);
eventList = altEventsFromSplices(spliceList);
warn("Loading beds.");
bedLoadChromKeeper();
dotForUserInit(max(slCount(eventList)/20, 1));
for(event = eventList; event != NULL; event = event->next)
    {
    dotForUser();
    findProbeSetsForEvents(event);
    logSpliceType(event->splice->type);
    if(event->altPathProbeCount >= 2)
	logSpliceTypeWProbe(event->splice->type);
    }
warn("");
warn("Reading Data Matrixs.");
if(probIn == NULL || intenIn == NULL)
    errAbort("Must specify intensityFile and probeFile");
intenM = dMatrixLoad(intenIn);
probM = dMatrixLoad(probIn);
fillInEventData(eventList, intenM, probM);
/* doAnalysis(eventList); */
reportEventCounts();
}

int main(int argc, char *argv[])
/* Everybody's favorite function... */
{
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
altProbes();
return 0;
}
