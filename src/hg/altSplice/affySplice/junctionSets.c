/* junctionSets.c Program to group together affy probes (beds) that share
   a splice site. Any group of probes that share a splice site
   but link to another splice site indicate alternative splicing. */
#include "common.h"
#include "bed.h"
#include "hash.h"
#include "options.h"
#include "chromKeeper.h"
#include "binRange.h"
#include "obscure.h"
#include "hash.h"
#include "linefile.h"

struct junctionSet 
/* A set of beds with a common start or end. */
{
    struct junctionSet *next;   /* Next in list. */
    char *chrom;                /* Chromosome. */
    int chromStart;             /* Smallest start. */
    int chromEnd;               /* Largest end. */
    char *name;                 /* Name of junction. */
    int junctCount;             /* Number of junctions in set. */
    char strand[2];             /* + or - depending on which strand. */
    int genePSetCount;          /* Number of gene probe sets. */
    char **genePSets;           /* Gene probe set name. */
    char *hName;                /* Human name for locus or affy id if not present. */
    int maxJunctCount;          /* Maximum size of bedArray. */
    struct bed **bedArray;      /* Beds in the cluster. */
    int junctDupCount;          /* Number of redundant junctions in set. */
    int maxJunctDupCount;       /* Maximum size of bedDupArray. */
    struct bed **bedDupArray;   /* Redundant beds in the cluster. */
    boolean merged;             /* Has this set been merged? */
    boolean cassette;           /* Is this junctionSet a cassette exon. */
    char *exonPsName;           /* Name of exon probe set if available for cassette. */
};

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"bedFile", OPTION_STRING},
    {"setFile", OPTION_STRING},
    {"exonFile", OPTION_STRING},
    {"genePSet", OPTION_STRING},
    {"geneMap", OPTION_STRING},
    {"altExonFile", OPTION_STRING},
    {"db", OPTION_STRING},
    {"ctFile", OPTION_STRING},
    {"ctName", OPTION_STRING},
    {"ctDesc", OPTION_STRING},
    {"ctColor", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File containing beds that represent affy junction clusters.",
    "File to output sets of probes that share a splice site to.",
    "File containing beds with exons that can't be used (i.e. transcription start or end).",
    "File containing list of gene probe sets.",
    "File containing a map of affy ids to human names for genes.",
    "Map of junctions to exon probesets that overlap, cassette exon probe sets.",
    "File database that coordinates correspond to.",
    "File that custom track will end up in.",
    "Name of custom track.",
    "Description of custom track."
    "Color of custom track in r,g,b format (i.e. 255,0,0)."
};

int setCount = 0;        /* Number of sets we have, used for naming sets. */
int setStartCount = 0;   /* Number of sets with more than one start. */
int setEndCount = 0;     /* Number of sets with more than one end. */
int setMergedCount = 0;  /* Number of sets that were merged and contain more than one probe. */
FILE *ctFile = NULL;     /* File to write beds in sets as custom track to. */
FILE *notMerged = NULL;  /* File to look at clusters that share probe starts/ends but aren't casssettes. */
boolean chromKeeperOpen = FALSE; /* Is chromkeeper initialized? */

void usage()
/** Print usage and quit. */
{
int i=0;
warn("junctionSets - Program to group affymetrix probe sets in bed form\n"
     "by their splice sites. By definition if two or more probes sets share a\n"
     "splice juction but join to different splice junctions alternative\n"
     "splicing is possible.\n"
     "options are:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n   "
	 "junctionSets -bedFile=junctionProbeSets.bed -setFile=sets.tab");
}

void initCtFile(char *fileName)
/* Initialize the custom track file handle. */
{
char *name = optionVal("ctName", "junctionSets");
char *desc = optionVal("ctDesc", "Alt-Splicing Sets of Junction Probes");
char *color = optionVal("ctColor", "255,0,0");
ctFile = mustOpen(fileName, "w");
warn("Opening file %s to write", fileName);
fprintf(ctFile, "track name=\"%s\" description=\"%s\" color=%s\n", name, desc, color);
}

void loadSoftExons(char *exonFile)
/* Load all of the exons into our chromKeeper structure. */
{
struct bed *bed = NULL, *bedList = NULL;
char *db = optionVal("db", NULL);
if(db == NULL)
    errAbort("Must specify a database for binKeeper sizes.");
bedList = bedLoadAll(exonFile);
warn("Placing beds in binKeeper");
chromKeeperInit(db);
for(bed = bedList; bed != NULL; bed = bed->next)
    chromKeeperAdd(bed->chrom, bed->chromStart, bed->chromEnd, bed);
chromKeeperOpen = TRUE;
}

boolean overlapSoftExon(char *chrom, int start, int end)
/* Return TRUE if region overlaps a bed in the softExonFile. */
{
struct binElement *binList = NULL, *bin = NULL;
boolean match = FALSE;
if(chromKeeperOpen == FALSE)
    return FALSE;
binList = chromKeeperFind(chrom, start, end);
if(binList != NULL)
    match = TRUE;
slFreeList(&binList);
return match;
}

int bedSpliceStart(const struct bed *bed)
/* Return the start of the bed splice site. */
{
assert(bed->blockSizes);
return bed->chromStart + bed->blockSizes[0];
}

int bedSpliceEnd(const struct bed *bed)
/* Return the end of the bed splice site. */
{
assert(bed->blockSizes);
return bed->chromStart + bed->chromStarts[1];
}

int bedCmpEnd(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart largest first. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
assert(a->blockCount > 1 && b->blockCount > 1);
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = bedSpliceEnd(a) - bedSpliceEnd(b);
return dif;
}

int bedCmpStart(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
assert(a->blockCount > 1 && b->blockCount > 1);
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = bedSpliceStart(a) - bedSpliceStart(b);
return dif;
}

void junctionSetTabOut(struct junctionSet *js, FILE *out)
/* Write out junctionSet to a file. */
{
int i;
fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\t%d\t", js->chrom, js->chromStart, js->chromEnd, 
	js->name, js->junctCount, js->strand, js->genePSetCount);

/* Print out gene set names. */
for(i = 0; i < js->genePSetCount - 1; i++)
    fprintf(out, "%s,", js->genePSets[i]);
fprintf(out, "%s\t", js->genePSets[i]);

/* Print out human name. */
fprintf(out, "%s\t", js->hName);

/* Print out beds. */
for(i = 0; i < js->junctCount - 1; i++) 
    {
    fprintf(out, "%s,", js->bedArray[i]->name);
    }
fprintf(out, "%s\t", js->bedArray[i]->name);

/* Print out the duplicates. */
fprintf(out, "%d\t",  js->junctDupCount);
for(i = 0; i < js->junctDupCount - 1; i++) 
    {
    fprintf(out, "%s,", js->bedDupArray[i]->name);
    }
if(js->junctDupCount == 0)
    fprintf(out, "NA\t");
else
    fprintf(out, "%s\t", js->bedDupArray[i]->name);
if(js->cassette)
    fprintf(out, "%d\t%s\n", js->cassette, js->exonPsName);
else
    fprintf(out, "0\tNA\n");
}

struct junctionSet *findEndSets(struct bed *bedList)
/* Create a list of juctionSets for all the probes that have the
   same end.*/
{
int currentEnd = 0;
char *currentChrom = NULL;
struct bed *bed = NULL;
struct junctionSet *setList = NULL, *set = NULL;
char buff[256];
char currentStrand[2];
slSort(&bedList, bedCmpEnd);

for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(currentChrom == NULL ||
       differentString(bed->chrom, currentChrom) || 
       bedSpliceEnd(bed) != currentEnd ||
       bed->strand[0] != currentStrand[0])
	{
	if(set != NULL)
	    {
	    slSafeAddHead(&setList, set);
	    if(set->junctCount > 1)
		setEndCount++;
	    }
	safef(buff, sizeof(buff), "%s.%d", bed->chrom, setCount++);
	AllocVar(set);
	set->maxJunctCount = 5;
	set->chromStart = bedSpliceStart(bed);
	set->chromEnd = bedSpliceEnd(bed);
	AllocArray(set->bedArray, set->maxJunctCount);
	set->chrom = cloneString(bed->chrom);
	set->name = cloneString(buff);
	safef(set->strand, sizeof(set->strand), "%s", bed->strand);
	set->bedArray[set->junctCount++] = bed;

	/* Set the current chrom and end. */
	currentChrom = set->chrom;
	currentEnd = set->chromEnd;
	currentStrand[0] = set->strand[0];
	}
    else if(sameString(bed->chrom, currentChrom) && 
	    bedSpliceEnd(bed) == currentEnd)
	{
	/* Only want junctions that connect to non-soft exons, i.e. not
	   transcription start and ends. */
	if(!overlapSoftExon(bed->chrom, bed->chromStart, bedSpliceStart(bed)) || bed->strand[0] == '-')
	    {
	    if(set->junctCount + 1 >= set->maxJunctCount)
		{
		ExpandArray(set->bedArray, set->maxJunctCount, 2*set->maxJunctCount);
		set->maxJunctCount = 2 * set->maxJunctCount;
		}
	    set->chromEnd = max(set->chromEnd, bedSpliceEnd(bed));
	    set->chromStart = min(set->chromStart, bedSpliceStart(bed));
	    set->bedArray[set->junctCount++] = bed;
	    }
	}
    else
	errAbort("bed %s doesn't seem to fit the mold with chrom %s and chromEnd %d",
		 bed->name, currentChrom, currentEnd);
    }
if(set != setList && set != NULL)
    slSafeAddHead(&setList, set);
slReverse(&setList);
return setList;
}

struct junctionSet *findStartSets(struct bed *bedList)
/* Create a list of juctionSets for all the probes that have the
   same start.*/
{
int currentStart = 0;
char *currentChrom = NULL;
struct bed *bed = NULL;
struct junctionSet *setList = NULL, *set = NULL;

char buff[256];
char currentStrand[2];
slSort(&bedList, bedCmpStart);

for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(currentChrom == NULL ||
       differentString(bed->chrom, currentChrom) || 
       bedSpliceStart(bed) != currentStart ||
       bed->strand[0] != currentStrand[0])
	{
	if(set != NULL)
	    {
	    slSafeAddHead(&setList, set);
	    if(set->junctCount > 1)
		setStartCount++;
	    }
	safef(buff, sizeof(buff), "%s.%d", bed->chrom, setCount++);
	AllocVar(set);
	set->maxJunctCount = 5;
	set->chromStart = bedSpliceStart(bed);
	set->chromEnd = bedSpliceEnd(bed);
	AllocArray(set->bedArray, set->maxJunctCount);
	set->chrom = cloneString(bed->chrom);
	set->name = cloneString(buff);
	safef(set->strand, sizeof(set->strand), "%s", bed->strand);
	set->bedArray[set->junctCount++] = bed;

	/* Set the current chrom and start. */
	currentChrom = set->chrom;
	currentStart = set->chromStart;
	currentStrand[0] = set->strand[0];
	}
    else if(sameString(bed->chrom, currentChrom) && 
	    bedSpliceStart(bed) == currentStart)
	{
	/* Only want junctions that connect to non-soft exons, i.e. not
	   transcription start and ends. */
	if(!overlapSoftExon(bed->chrom, bedSpliceEnd(bed), bed->chromEnd) || bed->strand[0] == '+')
	    {
	    if(set->junctCount + 1 >= set->maxJunctCount)
		{
		ExpandArray(set->bedArray, set->maxJunctCount, 2*set->maxJunctCount);
		set->maxJunctCount = 2 * set->maxJunctCount;
		}
	    set->chromEnd = max(set->chromEnd, bedSpliceEnd(bed));
	    set->chromStart = min(set->chromStart, bedSpliceStart(bed));
	    set->bedArray[set->junctCount++] = bed;
	    }
	}
    else
	errAbort("bed %s doesn't seem to fit the mold with chrom %s and chromStart %d",
		 bed->name, currentChrom, currentStart);
    }
slReverse(&setList);
return setList;
}    



boolean mergeableSets(struct junctionSet *start, struct junctionSet *end)
/* Return TRUE if there is a probe set in common between start and end. */
{
int i,j;	
if(start->merged == TRUE || end->merged == TRUE)
    return FALSE;
if(start->junctCount <=1 || end->junctCount <=1)
    return FALSE;
for(i = 0; i < start->junctCount; i++)
    {
    for(j = 0; j < end->junctCount; j++)
	{
	if(start->bedArray[i] == end->bedArray[j])
	    return TRUE;
	}
    }
return FALSE;
}

int bedStartEndCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if(dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

boolean formCassetteExon(struct junctionSet *start, struct junctionSet *end)
/* Return TRUE if the two junctions form a cassette exon. */
{
struct bed *cassette[4];
if(start->junctCount != 2 || end->junctCount != 2)
    return FALSE;

cassette[0] = start->bedArray[0];
cassette[1] = start->bedArray[1];
cassette[2] = end->bedArray[0];
cassette[3] = end->bedArray[1];
qsort(cassette, 4, sizeof(cassette[0]), bedStartEndCmp);
if(cassette[0]->chromStart == cassette[1]->chromStart &&
   cassette[1] == cassette[2] &&
   cassette[2]->chromEnd == cassette[3]->chromEnd &&
   cassette[3]->chromStart != cassette[0]->chromStart &&
   cassette[3]->chromEnd != cassette[0]->chromEnd)
    {
    return TRUE;
    }
return FALSE;
}

struct junctionSet* mergeSets(struct junctionSet *start, struct junctionSet *end)
/* Merge two sets. All juntions that are common to the both sets
   stay in the bedArray, everything else goes into the bedDupeArray. */
{
struct junctionSet *set = NULL;
int i = 0,j = 0;
if(start->junctCount > 1 && end->junctCount > 1)
    {
    fprintf(stdout, "Merging clusters %s and %s new coords %s:%d-%d\n", start->name, end->name,
	 start->chrom, min(start->chromStart,end->chromStart)-4, max(start->chromEnd, end->chromEnd)+5);
    setMergedCount++;
    }
/* Fill in basics and allocate arrays. */
AllocVar(set);
set->chrom = cloneString(start->chrom);
set->maxJunctCount = start->junctCount + end->junctCount;
set->chromStart = min(start->chromStart,end->chromStart);
set->chromEnd = max(start->chromEnd, end->chromEnd);
set->name = cloneString(start->name);
safef(set->strand, sizeof(set->strand), "%s", start->strand);
AllocArray(set->bedArray, set->maxJunctCount);
AllocArray(set->bedDupArray, set->maxJunctCount);

/* Loop through start beds looking for beds that are found 
   in just the starts or in both. */
for(i = 0; i < start->junctCount; i++) 
    {
    boolean found = FALSE;
    for(j = 0; j < end->junctCount; j++) 
	{
	if(start->bedArray[i] == end->bedArray[j])
	    {
	    set->bedArray[set->junctCount++] = start->bedArray[i];
	    found = TRUE;
	    }
	}
    if(found == FALSE) 
	set->bedDupArray[set->junctDupCount++] = start->bedArray[i];
    }

/* Look for ends for beds that are found just in the ends. */
for(i = 0; i < end->junctCount; i++) 
    {
    boolean found = FALSE;
    for(j = 0; j < start->junctCount; j++) 
	{
	if(end->bedArray[i] == start->bedArray[j])
	    {
	    found = TRUE;
	    }
	}
    if(found == FALSE) 
	set->bedDupArray[set->junctDupCount++] = end->bedArray[i];
    }

return set;
}

char *nameOfCassExonPs(struct junctionSet *set, struct hash *altExons)
/* Find the name of the appropriate cassette exon probe set or 'NA' if 
   not available. */
{
char **names = NULL;
int nameCount = 0;
int junctIx = 0, i = 0, j = 0;
int count = 0;
char *exonPs = NULL;
nameCount = set->junctDupCount;
AllocArray(names, nameCount);

/* Loop through the junctions and get the names for 
   any associated exon probe sets. */
/* for(junctIx = 0; junctIx < set->junctCount; junctIx++) */
/*     names[junctIx] = hashFindVal(altExons, set->bedArray[junctIx]->name); */
/* count = junctIx; /\* remember our offset in the names array. *\/ */
for(junctIx = 0; junctIx < set->junctDupCount; junctIx++)
    names[junctIx+count] = hashFindVal(altExons, set->bedDupArray[junctIx]->name);

/* Now make sure that the names are all the same. */
count = 0;
for(i = 0; i < nameCount; i++)
    {
    if(names[i] == NULL)
	continue;
    exonPs = names[i];
    count++;
    for(j = i+1; j < nameCount; j++)
	{
	if(names[j] != NULL && differentString(names[i], names[j]))
	    {
	    i = nameCount;
	    exonPs = NULL;
	    break;
	    }
	count++;
	}
    i = j;
    }
if(exonPs == NULL || count != 2)
    exonPs = cloneString("NA");
else
    exonPs = cloneString(exonPs);
freez(&names);
return exonPs;
}

char *confirmingExon(struct junctionSet *js, struct hash *altExons)
/* Look to see if there is a confirming exon for the shorter
   junction. */
{
int includeIx = 0, otherIx = 0;
struct bed *bed1 = NULL, *bed2 = NULL;
char *exon = NULL;
char *otherExon = NULL;
if(js->junctCount != 2)
    return NULL;
bed1 = js->bedArray[0];
bed2 = js->bedArray[1];
if(bed1->chromEnd - bed1->chromStart > bed2->chromEnd - bed2->chromStart)
    {
    includeIx = 1;
    otherIx = 0;
    }
else
    {
    includeIx = 0;
    otherIx = 1;
    }

exon = hashFindVal(altExons, js->bedArray[includeIx]->name);
otherExon = hashFindVal(altExons, js->bedArray[otherIx]->name);
/* Make sure that the two junctions don't contain the same exon. */
if(exon != NULL && otherExon != NULL && sameString(exon, otherExon))
    exon = NULL;
return exon;
}

struct junctionSet* mergeSetLists(struct junctionSet *starts, struct junctionSet *ends, struct hash *altExons)
/* Try to merge sets that have the same probes in both junctions.
   Don't want to count cassette exons twice for example (as they will
   have a set in both the starts and the ends). Also want to keep track
   of probes that were in different clusters that have now been moved.

   Algorithm: Look for clusters that have the same junction in both
   start and end. Merge these two cluster putting junctions that
   don't bridge in bedDupeArray. Once a cluter has been merged it cannot
   be merged again. That avoids transitivity in the merging process.
*/
{
struct junctionSet *mergedList = NULL, *jsStart = NULL, *jsEnd = NULL, *merged = NULL, *jsNext = NULL;
dotForUserInit(1000);
for(jsStart = starts; jsStart != NULL; jsStart = jsNext)
    {
    dotForUser();
    jsNext = jsStart->next;
    for(jsEnd = ends; jsEnd != NULL; jsEnd = jsEnd->next)
	{
	/* If we can merge them do so and add them to final list. */
	if(mergeableSets(jsStart, jsEnd))
	    {
	    if(formCassetteExon(jsStart, jsEnd))
		{
		merged = mergeSets(jsStart, jsEnd);
		merged->cassette = TRUE;
		merged->exonPsName = nameOfCassExonPs(merged, altExons);
		slAddHead(&mergedList, merged);
		jsStart->merged = TRUE;
		jsEnd->merged = TRUE;
		break;
		}
	    else 
		{
		fprintf(notMerged, "%s\t%d\t%d\t%s_%s\t%d\n", 
			jsStart->chrom,
			min(jsStart->chromStart, jsEnd->chromStart),
			max(jsStart->chromEnd, jsEnd->chromEnd),
			jsStart->name, jsEnd->name,
			jsStart->junctCount + jsEnd->junctCount);
		}
	    }
	}
    /* If this set can't be merged add it to the final list. */
    if(!jsStart->merged && jsStart->junctCount > 1)
	{
	slAddHead(&mergedList, jsStart);
	}
    }

/* Go through and add all the unmerged ends to the list. */
for(jsEnd = ends; jsEnd != NULL; jsEnd = jsNext)
    {
    jsNext = jsEnd->next;
    if(!jsEnd->merged && jsEnd->junctCount > 1)
	{
	slAddHead(&mergedList, jsEnd);
	}
    }

for(jsStart = mergedList; jsStart != NULL; jsStart = jsStart->next)
    {
    char *name = NULL;
    if(!jsStart->cassette)
	{
	name = confirmingExon(jsStart, altExons);
	if(name != NULL)
	    {
	    jsStart->cassette = TRUE;
	    jsStart->exonPsName = name;
	    }
	}
    }
warn("\nDone Merging");
return mergedList;
}

void populateGeneSetHash(struct hash *hash, char *geneSetFile)
/* Create a hash that contains gene probeSet names indexed by their
   GXXXXXX identifier. */
{
struct lineFile *lf = NULL;
char *string = NULL;
char buff[256];
char *mark = NULL;
lf = lineFileOpen(geneSetFile, TRUE);
while(lineFileNextReal(lf, &string))
    {
    struct slName *name = NULL, *nameList = NULL;
    safef(buff, sizeof(buff), "%s", string);

    /* Look to parse prefix off things like: G6848899EX_RC_a_at */
    mark = strstr(buff, "EX");
    if(mark != NULL)
	{
	name = newSlName(buff);
	*mark = '\0';
	} 
    else /* Try to parse numbers off of things like: G6848899_RC_a_at */
	{
	mark = strchr(buff, '_');
	if(mark == NULL)
	    errAbort("Can't parse gene probe set name %s\n", buff);
	name = newSlName(buff);
	*mark = '\0';
	}
    nameList = hashFindVal(hash, buff);
    /* If we have seen this gene before append to list other
       wise start a new list. */
    if(nameList != NULL)
	slAddTail(&nameList, name);
    else
	hashAddUnique(hash, buff, name);
    }
lineFileClose(&lf);
}

void populateSjAltExonMapHash(struct hash *hash, char *altExonMapFile)
/* Create a hash that maps splice juction probe sets to the
   cassette exons they overlap. */
{
struct lineFile *lf = NULL;
char *row[2]; 
lf = lineFileOpen(altExonMapFile, TRUE);
while(lineFileNextRowTab(lf, row, 2))
    {
    hashAdd(hash, row[0], cloneString(row[1]));
    }
lineFileClose(&lf);
}

void populateGeneMapHash(struct hash *hash, char *geneMapFile)
/* Create a hash that contains gene probeMap names indexed by their
   GXXXXXX identifier. */
{
struct lineFile *lf = NULL;
char *row[2]; 
lf = lineFileOpen(geneMapFile, TRUE);
while(lineFileNextRowTab(lf, row, 2))
    {
    hashAdd(hash, row[1], cloneString(row[0]));
    }
lineFileClose(&lf);
}

char *geneNameForId(struct hash *hash, char *id)
/* Return the human name for probe set id. */
{
char *mark = NULL;
char buff[256];
char *name = hashFindVal(hash, id);
if(name == NULL)
    {
    safef(buff, sizeof(buff), "%s", id);
    mark = strstr(buff, "EX");
    if(mark == NULL)
	mark = strchr(buff, '_');
    assert(mark);
    *mark = '\0';
    name = cloneString(buff);
    }
return name;
}
	

void findGeneSets(struct junctionSet *js, struct hash *hash, char ***pCharArray, int *pCount)
/* Find the corresponding gene probe set for this junction set. */
{
char buff[256];
char *mark = NULL;
struct slName *name = NULL, *nameList = NULL;
safef(buff, sizeof(buff), "%s", js->bedArray[0]->name);
mark = strchr(buff, '@');
if(mark == NULL)
    errAbort("Can't parse gene name from %s", buff);
*mark = '\0';
nameList = hashFindVal(hash, buff);
if(nameList != NULL)
    {
    AllocArray(*pCharArray, slCount(nameList));
    }
*pCount = 0;
for(name = nameList; name != NULL; name = name->next)
    {
    (*pCharArray)[(*pCount)++] = name->name;
    }
}

void junctionSets(char *bedFile, char *setFile, char *geneSetFile, char *geneMapFile) 
/* Bin up the beds into sets that have the same splice sites. */
{
struct bed *bed = NULL, *bedList = NULL;
struct junctionSet *starts = NULL, *ends = NULL, *merged = NULL, *js = NULL;
FILE *out = mustOpen(setFile, "w");
char *exonFile = optionVal("exonFile", NULL);
char *altExonMapFile = optionVal("altExonFile", NULL);
int totalCount = 0;
struct hash *geneSetHash = newHash(12);
struct hash *geneMapHash = newHash(12);
struct hash *sjToAltExHash = newHash(12);
warn("Reading beds.");
bedList = bedLoadAll(bedFile);
populateGeneSetHash(geneSetHash, geneSetFile);
populateGeneMapHash(geneMapHash, geneMapFile);
if(altExonMapFile != NULL)
    populateSjAltExonMapHash(sjToAltExHash, altExonMapFile);
if(exonFile != NULL)
    loadSoftExons(exonFile);
warn("Clustering starts.");
starts = findStartSets(bedList);
warn("Clustering ends.");
ends = findEndSets(bedList); 
warn("Merging clusters.");
merged = mergeSetLists(starts, ends, sjToAltExHash); 
for(js = merged; js != NULL; js = js->next)
    {
    int i;
    if((js->junctCount + js->junctDupCount) > 1)
	{
	findGeneSets(js, geneSetHash, &js->genePSets, &js->genePSetCount);
	/* The only ones we should be missing are the pesky CXXXX ones.
	   about 30 genes, don't know where they come from. */
	if(js->genePSetCount != 0)
	    {
	    js->hName = geneNameForId(geneMapHash, js->genePSets[0]);
	    if(ctFile != NULL)
		{
		for(i = 0; i < js->junctCount; i++)
		    bedTabOutN(js->bedArray[i], 12, ctFile);
		for(i = 0; i < js->junctDupCount; i++)
		    bedTabOutN(js->bedDupArray[i], 12, ctFile);
		}
	    junctionSetTabOut(js,out);
	    totalCount++;
	    }
	}
    }
warn("%d total alt sets. %d alt at start, %d alt at end, %d merged.",
     totalCount, setStartCount, setEndCount, setMergedCount);
bedFreeList(&bedList);
warn("Done.");
}

int main(int argc, char *argv[])
{
char *bedFile = NULL;
char *setFile = NULL;
char *ctFileName = NULL;
char *genePName = NULL;
char *geneMap = NULL;
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
bedFile = optionVal("bedFile", NULL);
setFile = optionVal("setFile", NULL);
ctFileName = optionVal("ctFile", NULL);
genePName = optionVal("genePSet", NULL);
geneMap = optionVal("geneMap", NULL);
if(bedFile == NULL || setFile == NULL || genePName == NULL || geneMap == NULL)
    errAbort("Must specify bedFile, setFile, geneMap and genePSet files, use -help for usage.");
if(ctFileName != NULL)
    initCtFile(ctFileName);
if(bedFile == NULL || setFile == NULL)
    {
    warn("Error: Must specify both a bedFile and a setFile.");
    usage();
    }
notMerged = mustOpen("notMerged.tab","w");
junctionSets(bedFile, setFile, genePName, geneMap);
carefulClose(&notMerged);
carefulClose(&ctFile);
return 0;
}
