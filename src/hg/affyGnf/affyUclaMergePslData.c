/** affyUclaMergePslData.c - Program to merge data from Allen at UCLA
    and the alignments of the HG-U133B chip from Affymetrix. */
#include "common.h"
#include "psl.h"
#include "hash.h"
#include "linefile.h"
#include "bed.h"
#include "options.h"

static boolean doHappyDots;   /* output activity dots? */
char *prefix = ":";  /* Prefext to our affymetrix names. */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"pslFile", OPTION_STRING},
    {"affyFile", OPTION_STRING},
    {"bedOut", OPTION_STRING},
    {"expRecordOut", OPTION_STRING},
    {"expFile", OPTION_STRING},
    {"toDiffFile", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "File containing psl alignments.",
    "File from Allen at UCLA",
    "File to output beds to.",
    "File to output experiment descriptions to.",
    "File with list of experiment names, one per line.",
    "Output resulting beds in format close to Allen's file (for diff sanity check)"
};

void usage()
/** Print usage and quit. */
{
int i=0;
warn("affyUclaMergePslData - Merge data from Allen at UCLA and alignments\n"
     "   of HG-U133B Consensus sequences in pslFile.\n");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n"
	 "   affyUclaMergePslData "
	 "affyUclaMergePslData -pslFile=hg15.affyU133AB_all.lifted.pslReps.psl  \\\n"
	 "   -affyFile=/projects/compbio/data/microarray/affyUcla/data/030602_ucla_normal_human_tissue_snapshot.txt \\\n"
	 "   -bedOut=hg15.affyUcla.bed -expRecordOut=hg15.affyUcla.expRecords -expFile=expNames.txt -toDiffFile=toDiff.txt\\\n");
}


struct hash *hashPsls(char *pslFileName)
{
struct psl *pslList = NULL, *psl = NULL, *pslSubList = NULL, *pslNext = NULL;
struct hash *pslHash = newHash(15);
char *last = NULL;

char key[128];
char *tmp = NULL;
pslList = pslLoadAll(pslFileName);

/* Fix psl names */
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    tmp = strrchr(psl->qName, ';');
    *tmp = '\0';
    tmp = strstr(psl->qName,prefix);
    assert(tmp);
    /* checks if there are 2 occurrences of ":" in probe name as in full name */
    /* if probe name is shortened to fit in the seq table, there is only 1 ":"*/
    /* e.g. full: consensus:HG-U133A:212933_x_at; short:HG-U133A:212933_x_at;*/

    if (countChars(psl->qName, *prefix) == 2) 
        {
        tmp = strstr(tmp+1,prefix);
        assert(tmp);
        }
    tmp = tmp + strlen(prefix);
    safef(psl->qName, strlen(psl->qName), "%s", tmp);
    }

/* Sort based on query name. */

slSort(&pslList, pslCmpQuery);
/* For each psl, if it is has the same query name add it to the
   sublist. Otherwise store the sublist in the hash and start
   another. */
for(psl = pslList; psl != NULL; psl = pslNext)
    {
    pslNext = psl->next;
    if(last != NULL && differentWord(last, psl->qName))
	{
	hashAddUnique(pslHash, last, pslSubList);
	pslSubList = NULL;
	}
    slAddTail(&pslSubList, psl);
    last = psl->qName;
    }
/* Add the last sublist */
hashAddUnique(pslHash, last, pslSubList);
return pslHash;
}

void pslFreeListWrapper(void *val)
{
struct psl *pslList = val;
pslFreeList(&pslList);
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 1000;
static int dot = 1000;
if (doHappyDots && (--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

void fillInExpHash(char *expFileName, struct hash **expHash, struct slName **expNames, int *expCount)
/** Read all of the names from the expFileName and store them in a hash. */
{
struct lineFile *lf = lineFileOpen(expFileName, TRUE);
char *line = NULL;
int lineSize = 0;
struct slName *name = NULL;
*expCount = 0;
*expHash = newHash(5);
while(lineFileNextReal(lf, &line))
    {
    hashAddInt(*expHash, line, (*expCount)++);
    name = newSlName(line);
    slAddHead(expNames, name);
    }
slReverse(expNames);
}

void initBedScores(struct bed *bed, int expCount)
{
int i;
for(i=0; i<expCount; i++)
    {
    bed->expIds[i] = i;
    bed->expScores[i] = -10000;
    }
}

void outputToDiffRecord(struct bed *bed, struct slName *expNames, FILE *out)
/** Write out a bed in the original format, good for diffing 
    to make sure things are getting matched. */
{
static struct hash *unique = NULL;
char key[256];
char *tmp = "placeHolder";
int i;
struct slName *name = NULL;
if(unique == NULL)
    unique = newHash(15);
safef(key, sizeof(key), "%s", bed->name);
if(hashFindVal(unique, key) != NULL)
    return;
for(i=0; i<bed->expCount; i++)
    {
    if(bed->expScores[i] == -10000)
	continue;
    name = slElementFromIx(expNames,bed->expIds[i]);
    fprintf(out, "%s\t%s\t%f\n", bed->name, name->name, bed->expScores[i]);
    }
hashAdd(unique, key, tmp);
}

void outputBedsFromPsls(struct hash *pslHash,char *bedOutName, char *expRecordOutName, 
			char *affyFileName, char *expFileName)
/** For each set of entries in affyFile find matching psl and create a bed. */
{
struct bed *bed = NULL, *b=NULL;
struct psl *pslList = NULL, *psl = NULL;
struct hash *expHash = NULL;
int numExps = 0;
int expCount = 0;
int i =0;
char *probeSet = NULL;
char *row[4];
char key[128];
struct slName *expNames = NULL, *name = NULL;
FILE *bedOut = NULL;
FILE *expRecordOut = NULL;
char *toDiffFileName = optionVal("toDiffFile", NULL);
FILE *toDiffOut = NULL;
struct lineFile *lf = NULL;
fillInExpHash(expFileName, &expHash, &expNames, &expCount);
lf = lineFileOpen(affyFileName, TRUE);
bedOut = mustOpen(bedOutName, "w");
if(toDiffFileName != NULL)
    toDiffOut = mustOpen(toDiffFileName, "w");

/* Loop through either adding experiments to beds or if new
   probeset create bed from psl and start over. */
while(lineFileChopNextTab(lf, row, sizeof(row)))
    {
    /* Do we have to make a new bed? */
    if(probeSet == NULL || differentWord(probeSet, row[0]))
	{
	occassionalDot();
	numExps = 0;
	/* If we have probeset print out the current beds. */
	if(probeSet != NULL)
	    {
	    for(b = bed; b != NULL; b = b->next)
		{
		int avgCount = 0;
		for(i = 0; i < b->expCount; i++)
		    if(b->expScores[i] != -10000)
			avgCount++;
		if(avgCount != 0 && b->score > 0)
		    b->score = log(b->score / avgCount) * 100;
		else
		    b->score = 0;
		bedTabOutN(b, 15, bedOut);
		if(toDiffOut != NULL)
		    outputToDiffRecord(b, expNames, toDiffOut);
		}
	    }
	bedFreeList(&bed);
	/* Lookup key in pslHash to find list of psl. */
	safef(key, sizeof(key), "%s", row[0]);
	pslList = hashFindVal(pslHash, key);
	/* Can have multiple psls. */
	for(psl = pslList; psl != NULL; psl = psl->next)
	    {
	    b = bedFromPsl(psl);
	    AllocArray(b->expIds, expCount );
	    AllocArray(b->expScores, expCount);
	    b->expCount = expCount;
	    initBedScores(b, expCount);
	    slAddHead(&bed, b);
	    }
	}
    if(bed != NULL)
	{
	/* Allocate larger arrays if necessary. */
	if(numExps > expCount)
	    {
	    errAbort("Supposed to be %d experiments but probeset %s has at least %d",
		     expCount, bed->name, numExps);
	    }
	for(b = bed; b != NULL; b = b->next)
	    {
	    int exp = hashIntVal(expHash, row[1]);
	    if(differentWord(row[3], "NaN"))
	       b->expScores[exp] = atof(row[3]);
	    if(differentWord(row[2], "NaN"))
	       b->score += atof(row[2]);
	    }
	numExps++;
	}
    freez(&probeSet);
    probeSet = cloneString(row[0]);
    }
expRecordOut = mustOpen(expRecordOutName, "w");
i = 0;
for(name = expNames; name != NULL; name = name->next)
    {
    subChar(name->name, ',', '_');	    
    subChar(name->name, ' ', '_');
    fprintf(expRecordOut, "%d\t%s\tuclaExp\tuclaExp\tuclaExp\tuclaExp\t1\t%s,\n", i++, name->name, name->name);
    }
hashFree(&expHash);
slFreeList(&expNames);
carefulClose(&expRecordOut);
carefulClose(&bedOut);
lineFileClose(&lf);
}
	
void mergeDataAndAlignments()
/** Load up the psls, hash them and transform into beds. */
{
char *pslFileName = NULL;
char *bedOutName = NULL;
char *affyFileName = NULL;
char *expRecordOutName = NULL;
char *expFileName = NULL;
struct hash *pslHash = NULL;
struct bed *bed = NULL;
/* Parse some arguments and make sure they exist. */
pslFileName = optionVal("pslFile", NULL);
if(pslFileName == NULL)
    errAbort("Must specify -pslFile flag. Use -help for usage.");
bedOutName = optionVal("bedOut", NULL);
if(bedOutName == NULL)
    errAbort("Must specify -bedOut flag. Use -help for usage.");
affyFileName = optionVal("affyFile", NULL);
if(affyFileName == NULL)
    errAbort("Must specify -affyFile flag. Use -help for usage.");
expRecordOutName = optionVal("expRecordOut", NULL);
if(expRecordOutName == NULL)
    errAbort("Must specify -expRecordOut flag. Use -help for usage.");
expFileName = optionVal("expFile", NULL);
if(expFileName == NULL)
    errAbort("Must specify -expFile flag. Use -help for usage.");
/* Hash psls according to their name. */
warn("Reading psls from: %s", pslFileName);
pslHash = hashPsls(pslFileName);
warn("Outputing beds:");
outputBedsFromPsls(pslHash, bedOutName, expRecordOutName, affyFileName, expFileName);
warn("\nFreeing Memory.");
hashTraverseVals(pslHash, pslFreeListWrapper);
hashFree(&pslHash);
warn("Done.");
}

int main(int argc, char *argv[])
{
if(argc == 1)
    usage();
doHappyDots = isatty(1);  /* stdout */
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
mergeDataAndAlignments();
return 0;
}
