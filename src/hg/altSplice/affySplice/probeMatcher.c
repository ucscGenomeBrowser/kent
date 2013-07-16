/* probeMatcher - Program to sort out which probes belong to 
   which splicing isoform. */
#include "common.h"
#include "bed.h"
#include "altGraphX.h"
#include "hdb.h"
#include "options.h"
#include "affyProbe.h"

void usage() 
{
errAbort("probeMatcher - Program to associate probes with splicing isoforms.\n"
	 "usage:\n"
	 "   probeMatcher <splices.agx> <db> <juntionTable> <nonJunctTable> <constBeds> <output.txt>\n");
}

struct hash *constHash = NULL;

struct agProbeSets
/* Little info about the probe sets for altGraphX. */
{
    char *chrom;             /* Chromosome. */
    int chromStart;          /* Chromosome Start. */
    int chromEnd;            /* Chromosome End. */
    int type;                /* Type of splicing event 2==cassette. */
    char strand[3];          /* Genomic Strand. */
    char *name;              /* Name from altSplice clustering. */
    int maxCounts;           /* Maximum possible counts, used for
			      * memory management. */
    int contProbeCount;      /* Number of constitutive probe sets. */
    char **contProbeSets;    /* Names of constitutive probe sets. */
    int alt1ProbeCount;      /* Number of alternate1 probe sets. */
    char **alt1ProbeSets;    /* Names of alternate1 probe sets. */
    int alt2ProbeCount;      /* Number of alternate2 probe sets. */
    char **alt2ProbeSets;    /* Names of alternate2 probe sets. */
    int transcriptCount;     /* Number of altMerge transcripts involved. */
    char **transcriptNames;  /* Names of altMerge transcripts. */
};

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 100;
static int dot = 100;
if (--dot <= 0)
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

struct hash *makeBedHash(char *bedFile) 
{
struct bed *bed=NULL, *bedNext = NULL, *bedList = NULL;
struct bed *item = NULL;
struct hash *bedHash = newHash(12);
bedList = bedLoadAll(bedFile);
for(bed = bedList; bed != NULL; bed = bedNext)
    {
    bedNext = bed->next;
    if(bed->score != 6)
	continue; 
    item = hashFindVal(bedHash, bed->name);
    bed->next = NULL;
    if(item == NULL)
	{
	hashAdd(bedHash, bed->name, bed);
	}
    else
	{
	slAddTail(&item, bed);
	}
    }
return bedHash;
}
    

struct agProbeSets *newAgP(struct altGraphX *ag)
/* Allocated and fill in some initial values for agProbeSets. */
{
struct agProbeSets *agp = NULL;
AllocVar(agp);
agp->chrom = cloneString(ag->tName);
agp->chromStart = ag->tStart;
agp->chromEnd = ag->tEnd;
agp->type = ag->id;
agp->strand[0] = ag->strand[0];
agp->name = cloneString(ag->name);
agp->maxCounts = 10;
AllocArray(agp->contProbeSets, agp->maxCounts);
AllocArray(agp->alt1ProbeSets, agp->maxCounts);
AllocArray(agp->alt2ProbeSets, agp->maxCounts);
agp->transcriptCount = ag->mrnaRefCount;
agp->transcriptNames = ag->mrnaRefs;
return agp;
}

char ** expandAgp(struct agProbeSets *agp, char **probeSets )
{
char **cT=agp->contProbeSets, **a1=agp->alt1ProbeSets, **a2 = agp->alt1ProbeSets;
ExpandArray(agp->contProbeSets, agp->maxCounts, 2*agp->maxCounts);
ExpandArray(agp->alt1ProbeSets, agp->maxCounts, 2*agp->maxCounts);
ExpandArray(agp->alt2ProbeSets, agp->maxCounts, 2*agp->maxCounts);
agp->maxCounts *= 2;
if(probeSets == cT)
    return agp->contProbeSets;
else if(probeSets == a1)
    return agp->alt1ProbeSets;
else if(probeSets == a2)
    return agp->alt2ProbeSets;
errAbort("Not sure which probeSet was being used.");
return NULL;
}

void addProbeSets(struct altGraphX *ag, enum ggEdgeType type, int chromStart, int chromEnd,
		  struct sqlConnection *conn,
		  char *junctionTable, char *nonJunctionTable, 
		  struct agProbeSets *agp, int *count, char **probeSets)
/* Add the relevant probe sets to probeSets for edge in ag. */
{
char where[256], temp[256];
struct sqlResult *sr = NULL;
char **row = NULL;
int *vPos = ag->vPositions;
int *starts = ag->edgeStarts;
int *ends = ag->edgeEnds;
char *table = NULL;
int rowOffset = 0;
boolean unique = TRUE;
struct bed *bedList = NULL, *bed = NULL;
int i=0;
struct affyProbe *affyProbe=NULL, *affyProbeList=NULL;
/* Junctions have to be handled a bit differently. The numbers we 
   need are going to be inside the data type and difficult to select for.
   have to get everything and sort it out afterwards. */
if(type == ggSJ)
    {
    sqlSafefFrag(where, sizeof(where), " chromStart >= %d and chromEnd <= %d and blockCount = 2 ", 
	  chromStart-25, chromEnd+25);
    table = junctionTable;
    sr = hRangeQuery(conn, table, ag->tName, chromStart, chromEnd,
		     where, &rowOffset);
    while((row = sqlNextRow(sr)) != NULL)
	{
	affyProbe = affyProbeLoad(row+rowOffset);
	slAddHead(&affyProbeList, affyProbe);
	}
    sqlFreeResult(&sr);
    for(affyProbe=affyProbeList; affyProbe != NULL; affyProbe = affyProbe->next)
	{
	/* If it is a juction it must line up with the starts and stops. */
	if(type == ggSJ && 
	   ((affyProbe->chromStart+affyProbe->chromStarts[0]+affyProbe->blockSizes[0] != chromStart) ||
	    (affyProbe->chromStart+affyProbe->chromStarts[1] != chromEnd)))
	    continue;
	safef(temp, sizeof(temp), "%s:%s", affyProbe->psName, affyProbe->seq);
	for(i=0; i<*count; i++)
	    {
	    if(sameString(probeSets[i], temp))
		unique = FALSE;
	    }
	if(unique)
	    {
	    if(*count+1 > agp->maxCounts)
		probeSets = expandAgp(agp, probeSets);
	    probeSets[*count] = cloneString(temp);
	    (*count)++;
	    }
	}
    affyProbeFreeList(&affyProbe);
    }
else 
    {
    table = nonJunctionTable;
    sr = hRangeQuery(conn, table, ag->tName, chromStart, chromEnd,
		     NULL, &rowOffset);
    while((row = sqlNextRow(sr)) != NULL)
	{
	affyProbe = affyProbeLoad(row+rowOffset);
	slAddHead(&affyProbeList, affyProbe);
	}
    sqlFreeResult(&sr);
    for(affyProbe=affyProbeList; affyProbe != NULL; affyProbe = affyProbe->next)
	{
	safef(temp, sizeof(temp), "%s:%s", affyProbe->psName, affyProbe->seq);
	for(i=0; i<*count; i++)
	    {
	    if(sameString(probeSets[i], temp))
		unique = FALSE;
	    }
	if(unique)
	    {
	    if(*count+1 > agp->maxCounts)
		probeSets = expandAgp(agp, probeSets);
	    probeSets[*count] = cloneString(temp);
	    (*count)++;
	    }
	}
    affyProbeFreeList(&affyProbe);
    }
    
}
	

void agpTabOut(struct agProbeSets *agp, FILE *out)
/*Write out tab delimited. */
{
int i=0;
fprintf(out, "%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t", agp->chrom, agp->chromStart, agp->chromEnd,
	agp->type, agp->strand, agp->name, agp->maxCounts, agp->contProbeCount);
for(i=0; i<agp->contProbeCount; i++)
    fprintf(out, "%s,", agp->contProbeSets[i]);
fprintf(out, "\t");
fprintf(out, "%d\t", agp->alt1ProbeCount);
for(i=0; i<agp->alt1ProbeCount; i++)
    fprintf(out, "%s,", agp->alt1ProbeSets[i]);
fprintf(out, "\t");
fprintf(out, "%d\t", agp->alt2ProbeCount);
for(i=0; i<agp->alt2ProbeCount; i++)
    fprintf(out, "%s,", agp->alt2ProbeSets[i]);
fprintf(out, "\t");
fprintf(out, "%d\t", agp->transcriptCount);
for(i=0; i<agp->transcriptCount; i++)
    fprintf(out, "%s,", agp->transcriptNames[i]);
fprintf(out, "\n");
}



void findProbesForAg(struct altGraphX *ag, struct sqlConnection *conn,
		     char *junctionTable, char *nonJunctionTable, FILE *out)
/* Dump out the altGraphX's constitutive, alternative and alternative2 probe sets. */
{
struct agProbeSets *agp = newAgP(ag);
int i;
struct bed *bed=NULL, *bedList = NULL;
int *starts = ag->edgeStarts;
int *ends = ag->edgeEnds;
int *vPos = ag->vPositions;
for(i=0; i<ag->edgeCount; i++)
    {
    enum ggEdgeType type = getSpliceEdgeType(ag, i);
    switch (ag->edgeTypes[i])
	{
	case 0 :
	    addProbeSets(ag, type, vPos[starts[i]], vPos[ends[i]],
			 conn, junctionTable, nonJunctionTable, 
			 agp, &agp->contProbeCount, agp->contProbeSets);
	    break;
	case 1 :
	    addProbeSets(ag, type, vPos[starts[i]], vPos[ends[i]],
			 conn, junctionTable, nonJunctionTable, 
			 agp, &agp->alt1ProbeCount, agp->alt1ProbeSets);
	    break;
	case 2 :
	    addProbeSets(ag, type, vPos[starts[i]], vPos[ends[i]],
			 conn, junctionTable, nonJunctionTable, 
			 agp, &agp->alt2ProbeCount, agp->alt2ProbeSets);
	    break;
	default :
	    errAbort("probeMatcher::findProbesForAg() - Don't recognize type %d", ag->id);
	}
    }
bedList = hashFindVal(constHash, ag->name);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    addProbeSets(ag, ggExon, bed->chromStart, bed->chromEnd, conn,
		 junctionTable, nonJunctionTable, agp, 
		 &agp->contProbeCount, agp->contProbeSets);
    }
agpTabOut(agp, out);
}

void matchProbes(struct altGraphX *agList, char *junctionTable, char *nonJunctionTable, FILE *out)
/* Run through the agList and find the probes for the constitutive 
   and alternative edges. */
{
struct sqlConnection *conn = hAllocConn();
struct altGraphX *ag = NULL;
for(ag=agList; ag != NULL; ag = ag->next)
    {
    occassionalDot();
    findProbesForAg(ag, conn, junctionTable, nonJunctionTable, out);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct altGraphX *agList = NULL;
FILE *out = NULL;
char *bedName = NULL;
optionHash(&argc, argv);
if(argc != 6)
    usage();
hSetDb(argv[2]);
bedName = optionVal("bedName", NULL);
if(bedName != NULL)
    {
    warn("Creating bed hash.");
    constHash = makeBedHash(bedName);
    warn("");
    }
else
    constHash = newHash(2);
warn("Loading altGraphX records.");
agList = altGraphXLoadAll(argv[1]);
out = mustOpen(argv[5], "w");
warn("Looking for probe sets.");
matchProbes(agList, argv[3], argv[4], out);
carefulClose(&out);
warn("\nDone");
return 0;
}
	 
