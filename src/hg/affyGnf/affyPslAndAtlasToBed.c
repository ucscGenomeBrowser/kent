/* program to fit affy data into multiple score bed format and associated
   expRecord file from the affy file and the pslFile */
#include "common.h"
#include "affyAtlas.h"
#include "psl.h"
#include "hash.h"
#include "bed.h"
#include "dystring.h"
#include "expRecord.h"


#define DEBUG 0
FILE *scores = NULL;      /* output all scores here, used to generate a histogram of all results */
int missingExpsCount = 0; /* how many missing data points are there */
int noExpCount = 0;       /* how many points contain no experimental data */
int missingVal = -10000;  /* use this value for missing data */

void usage() 
{
errAbort("affyPslAndAtlasToBed - Takes an alignment of affy probe target\n"
	 "sequences and the human atlas results file to create an msBed file.\n"
	 "usage:\n"
	 "\taffyPslAndAtlasToBed pslFile atlasFile out.bed out.expRecord\n");
}

struct stat
/* generic structure with float val */
    {
    struct stat *next;  /* Next in singly linked list. */
    char id[33];	/* identifier */
    float val;	/* the value of interest */
    };

int statSortable(const void *e1, const void *e2)
{
const struct stat *v1 = *((struct stat**)e1);
const struct stat *v2 = *((struct stat**)e2);
float val = v1->val - v2->val;
if(val > 0) return 1;
if(val < 0) return -1;
if(val == 0) return 0;
}


void statSort(struct stat **statList) 
/* sorts a stat in increasing order */
{
slSort(statList,statSortable);
}

float statMedian(struct stat **statList)
/* finds the median of the list, will sort list to do this */
{
boolean odd = slCount(*statList)%2;
int half = slCount(*statList)/2;
struct stat *s=NULL, *s2=NULL;
float median = -1;
statSort(statList);

/* if odd the median is the middle value */
if(odd)
    {
    s = slElementFromIx(*statList,half);
    if(s != NULL)
	median = s->val;
    }
/* if even the median is the average of the two middle values */
else 
    {
    s = slElementFromIx(*statList,half);
    s2 = slElementFromIx(*statList,half-1);
    if(s != NULL && s2 != NULL)
	median = (s->val + s2->val)/2;
    }
return median;
}

void statFree(struct stat **pEl)
/* Free a single dynamically allocated stat such as created
 * with statLoad(). */
{
struct stat *el;

if ((el = *pEl) == NULL) return;
freez(pEl);
}

void statFreeList(struct stat **pList)
/* Free a list of dynamically allocated stat's */
{
struct stat *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    statFree(&el);
    }
*pList = NULL;
}

char * parseNameFromHgc(char *string)
/** parses out the name of the probe set. free returned string
with freez() */
{
char *toRet = strstr(string, "HG-U95Av2:");
/* char *toRet = strstr(string, ":"); */
char *tmp = NULL;
if(toRet == NULL)
    errAbort("Can't parse target name from %s.", string);
toRet++;
toRet = strstr(toRet, ":");
if(toRet == NULL)
    errAbort("Can't parse target name from %s.", string);
toRet++;
tmp = strstr(toRet, ";");
if(tmp == NULL)
    errAbort("Can't parse target name from %s.", string);
*tmp = '\0';
toRet = cloneString(toRet);
return toRet;
}

struct bed *createBedsFromPsls(char *pslFile, int expCount)
/** creates a list of beds from a pslfile, allocates memory for
arrays as determined by expCount */
{
struct psl *pslList = NULL, *psl = NULL;
struct bed *bedList = NULL, *bed = NULL;
pslList = pslLoadAll(pslFile);
for(psl = pslList; psl != NULL; psl = psl->next)
    {

    bed = bedFromPsl(psl);
    freez(&bed->name);
    bed->name=parseNameFromHgc(psl->qName);
    bed->score = 0;
    bed->expCount = 0;
    bed->expIds = needMem(sizeof(int)*expCount);
    bed->expScores = needMem(sizeof(float)*expCount);
    slAddHead(&bedList,bed);
    }
slReverse(&bedList);
pslFreeList(&pslList);
return bedList;
}

struct hash *createBedHash(struct bed *bedList)
/** takes a list of beds and puts them in a hash with duplicates
    numbered as name_1, name_2 */
{
struct hash *bedHash = newHash(5);
struct bed *bed = NULL;
struct dyString *ds = newDyString(1024);
char *name = NULL;
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    int count = 0;
    char *targetName = NULL;
    struct bed *tmp = NULL;

    dyStringClear(ds);
    dyStringPrintf(ds,"%s_%d", bed->name, count);
    /* since we may have duplications, look for an empty slot in the hash */
    while(TRUE && (count < 1000))
	{
	tmp = hashFindVal(bedHash, ds->string);
	if(tmp == NULL)
	    {
	    hashAddUnique(bedHash, ds->string, bed);
	    break;
	    }
	else 
	    {     
	    dyStringClear(ds);
	    dyStringPrintf(ds, "%s_%d", bed->name, ++count);
	    }
	}
    }
return bedHash;
}

struct expRecord *expRecordFromAffyAtlas(struct affyAtlas *aa)
/** constructs a simple experiment record from information in an affyAtlas record */
{
static int id = 0;
struct expRecord *er = NULL;
char name[256];
AllocVar(er);
sprintf(name, "%s_%s", aa->annName, aa->tissue);
er->id = id++;
er->name = cloneString(name);
er->description = cloneString(aa->tissue);
er->url = cloneString("https://www.netaffx.com/index2.jsp");
er->ref = cloneString("http://www.gnf.org/");
er->credit = cloneString("http://www.gnf.org/");
er->numExtras = 3;
er->extras = needMem(sizeof(char*) * er->numExtras);
er->extras[0] = cloneString("HG-U95");
er->extras[1] = cloneString(aa->annName);
er->extras[2] = cloneString(aa->tissue);
return er;
}

boolean differentExperiment(struct expRecord *er, struct affyAtlas *aa) 
/* return TRUE if this expRecord was derived from an affyAtlas record similar
   to this one FALSE otherwise */
{
if(er == NULL && aa == NULL)
    return TRUE;
if(er == NULL)
    return FALSE;
if(aa == NULL)
    return FALSE;
if(strstr(er->name, aa->annName) && strstr(er->name, aa->tissue))
    return FALSE;
else
    return TRUE;
}

boolean differentAA(struct affyAtlas *aa1,struct affyAtlas *aa2)
/** returns TRUE if from different chips FALSE otherwise */
{
return differentString(aa1->annName, aa2->annName);
}

int countExperiments(struct affyAtlas *aaList)
/** takes an affy atlas file and counts the different experiments in it */
{
struct affyAtlas *aa=NULL, *aaMark = NULL;
int count =1;
aaMark = aaList;
for(aa = aaList; aa != NULL; aa = aa->next)
    {
    if(differentAA(aaMark, aa))
	{
	count++;
	aaMark = aa;
	}
    }
return count;
}

void addExperimentToBed(struct bed *bed, struct affyAtlas *aa, struct expRecord *er)
/** add one affy atlas record to a ms bed record */
{
bed->expIds[bed->expCount] = er->id;
bed->score += aa->signal;
bed->expScores[bed->expCount] = aa->signal;
bed->expCount++;
}

void appendNewExperiment(struct hash *bedHash, struct affyAtlas *aa, struct expRecord *er)
/** finds the correct beds from the beds hash and adds the data in the affyAtlas
    record to them. */
{
char buff[256];
int count =0;
struct bed *bed = NULL;
snprintf(buff, sizeof(buff), "%s_%d", aa->probeSet, count);
while(TRUE)
    {
    bed = hashFindVal(bedHash, buff);
    if(bed == NULL)
	break;
    else
	{
	addExperimentToBed(bed, aa, er);
	snprintf(buff, sizeof(buff), "%s_%d", aa->probeSet, ++count);
	}
    }
}

void appendExperiments(struct hash *bedHash, struct affyAtlas *aaList, struct expRecord **erList)
/** loop through the altas file creating new experiments and appending them
   to the beds in the bed hash */
{
struct affyAtlas *aa=NULL;
struct expRecord *er = NULL;

assert(aaList);
er = expRecordFromAffyAtlas(aaList);

for(aa = aaList; aa != NULL; aa = aa->next)
    {
    if(differentExperiment(er, aa))
	{
	slAddHead(erList, er);
	er = expRecordFromAffyAtlas(aa);
	}
    appendNewExperiment(bedHash, aa, er);
    }
/* add last experiment */
slAddHead(erList,er);
}

void checkAllBeds(struct bed **bedList, int expCount)
/** check to make sure that all the beds have the same
    number of experiments associated with them */
{
struct bed *bed = NULL;
for(bed = *bedList; bed != NULL; )
    {
    if(bed->expCount != expCount)
	{
	struct bed *tmp = NULL;
	if(bed->expCount != 0)
	    {
	    warn("Bed %s at %d has only %d exps, mark is %d", bed->name, slIxFromElement(*bedList, bed), bed->expCount, expCount);
	    missingExpsCount++;
	    }
	else
	    noExpCount++;
	tmp = bed->next;
	slRemoveEl(bedList, bed);
	bed = tmp;
	}
    else
	bed = bed->next;
    }
}


float safeLog2(float num)
/* returns log base 2 of num if num > 0, else
   returns 0 */
{
if(num > 0)
    return logBase2(num);
else 
    return 0;
}

void convertIntensitiesToRatios(struct bed *bedList)
/* for each bed calculate the median intensity not including missing 
data and calcute scores as ratio of each intensity to median 
intensity. */
{
FILE *mediansOut = NULL, *valuesOut = NULL;
float median = 0;
struct stat *statList = NULL, *stat = NULL;
struct bed *bed = NULL;
int i = 0;
#ifdef DEBUG
valuesOut = mustOpen("values.debug", "w");
mediansOut = mustOpen("medians.debug", "w");
#endif
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    float ratio = 0;
    float logRatio = 0;
#ifdef DEBUG
    fprintf(mediansOut, "%s", bed->name);
    fprintf(valuesOut, "%s", bed->name);
#endif
    /* find the median value by sorting */
    for(i=0; i<bed->expCount; i++)
	{
	if(bed->expScores[i] != missingVal)
	    {
	    AllocVar(stat);
	    stat->val = bed->expScores[i];
	    slAddHead(&statList, stat);

	    }
	}
    median = statMedian(&statList);
#ifdef DEBUG
    for(stat = statList; stat != NULL; stat = stat->next)
	{
	fprintf(valuesOut, "\t%f", stat->val);
	}
    fprintf(valuesOut, "\n");
    fprintf(mediansOut, "\t%f\n", median);
#endif
    statFreeList(&statList);
    for(i=0; i<bed->expCount; i++)
	{
	if(bed->expScores[i] != missingVal)
	    {
	    ratio = bed->expScores[i] / median;
	    logRatio = safeLog2(ratio);
	    bed->expScores[i] = logRatio;
	    fprintf(scores, "%f\n", logRatio);
	    }
	}
    }
#ifdef DEBUG
carefulClose(&mediansOut);
carefulClose(&valuesOut);
#endif
}

void calculateAverages(struct bed *bedList)
/** divide the sum of the intensities stored in the score by
    number of experiments */
{
struct bed *bed = NULL;
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    float tmp;
    assert(bed->expCount);
    tmp =  bed->score / bed->expCount;
    if(tmp <= 0)
	bed->score = missingVal;
    else 
	{
	tmp = 10 * tmp;
	bed->score = (int)tmp;
	}
    }
}

void affyPslAndAtlasToBed(char *pslFile, char *atlasFile, char *bedOut, char *expRecOut)
/** Main function that does all the work */
{
struct hash *bedHash = NULL;
struct affyAtlas *aaList=NULL, *aa=NULL;
struct expRecord *erList=NULL, *er=NULL;
struct bed *bedList=NULL, *bed=NULL;
int expCount = 0;
FILE *erOut = NULL, *bOut=NULL;
warn("loading atlas file");
aaList = affyAtlasLoadAll(atlasFile);
expCount = countExperiments(aaList);
warn("creating list of beds from alignments");
bedList = createBedsFromPsls(pslFile, expCount);
warn("creating hash from list of beds");
bedHash = createBedHash(bedList);
warn("appending experiments to beds in hash");
appendExperiments(bedHash, aaList, &erList);
warn("Running sanity Checks");
checkAllBeds(&bedList, expCount);
warn("%d beds were missing experiments." , missingExpsCount);
warn("%d beds had no experiments.", noExpCount);
warn("Calculating average intensities");
convertIntensitiesToRatios(bedList);
calculateAverages(bedList);

warn("writing expRecords out");
erOut = mustOpen(expRecOut, "w");
for(er = erList; er != NULL; er = er->next)
    expRecordTabOut(er, erOut);
carefulClose(&erOut);

warn("writing beds out");
bOut = mustOpen(bedOut, "w");
for(bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 15, bOut);
carefulClose(&bOut);

warn("cleaning up..");
freeHash(&bedHash);
bedFreeList(&bedList);

warn("Done.");
}


int main(int argc, char *argv[])
{
if(argc != 5)
    usage();
scores = mustOpen("scores.tab", "w");
affyPslAndAtlasToBed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
