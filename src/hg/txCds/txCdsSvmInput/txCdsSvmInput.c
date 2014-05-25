/* txCdsSvmInput - Create input for svm_light, a nice support vector machine classifier.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "cdsEvidence.h"
#include "cdsOrtho.h"
#include "txInfo.h"


boolean good = FALSE;
boolean bad = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsSvmInput - Create input for svm_light, a nice support vector machine classifier.\n"
  "usage:\n"
  "   txCdsSvmInput in.bed in.tce in.ortho in.info out.lst out.vector\n"
  "options:\n"
  "   -good - consider this the positive training set\n"
  "   -bad - consider this the negative training set\n"
  );
}

static struct optionSpec options[] = {
   {"good", OPTION_BOOLEAN},
   {"bad", OPTION_BOOLEAN},
   {NULL, 0},
};

int cdsEvidenceCmp(const void *va, const void *vb)
/* Compare to sort based on name,start,end. */
{
const struct cdsEvidence *a = *((struct cdsEvidence **)va);
const struct cdsEvidence *b = *((struct cdsEvidence **)vb);
int dif = strcmp(a->name, b->name);
if (dif == 0)
    {
    dif = a->start - b->start;
    if (dif == 0)
        dif = a->end - b->end;
    }
return dif;  
}

char *orthoHashKey(char *name, int start, int end)
/* Make key for orthoHash. */
{
static char buf[256];
safef(buf, sizeof(buf), "%s_%X_%X", name, start, end);
return buf;
}

boolean mostlyTrue()
/* Return TRUE 3/4 of the time. */
{
static int i;
++i;
return (i&3) != 0;
}

struct idVal
/* An id (index into feature vector) and it's value */
    {
    struct idVal *next;
    int id;	
    double val;
    };

int idValCmp(const void *va, const void *vb)
/* Compare to sort based on id. */
{
const struct idVal *a = *((struct idVal **)va);
const struct idVal *b = *((struct idVal **)vb);
int diff = a->id - b->id;
if (diff == 0)
    {
    double valDiff = b->val - a->val;
    if (valDiff < 0)
        diff = -1;
    else if (valDiff > 0)
        diff = 1;
    }
return diff;
}

void idValAdd(struct lm *lm, struct idVal **pList, int id, double val)
/* Make up new idVal and add it to head of list. */
{
struct idVal *iv;
AllocVar(iv);
iv->id = id;
iv->val = val;
slAddHead(pList, iv);
}

struct idVal *skipDupes(struct idVal *list)
/* Remove from list all but first occurence of id, assuming that
 * list is sorted by ID. */
{
struct idVal *newList = NULL, *iv, *nextIv;
int lastId = -1;
for (iv = list; iv != NULL; iv = nextIv)
    {
    nextIv = iv->next;
    if (iv->id != lastId)
        {
	slAddHead(&newList, iv);
	lastId = iv->id;
	}
    }
slReverse(&newList);
return newList;
}

void outEvidence(struct lm *lm, int featureId, struct cdsEvidence *cds, struct idVal **pList)
/* Output evidence specific to one cdsEvidence */
{
idValAdd(lm, pList, featureId, 1);
idValAdd(lm, pList, featureId+1, cds->score);
idValAdd(lm, pList, featureId+2, cds->cdsCount);
}

void outOrtho(struct lm *lm, int featureId, struct cdsOrtho *ortho, int orfSize, struct idVal **pList)
/* Output features associated with cdsOrtho. */
{
idValAdd(lm, pList, featureId, (double)ortho->missing/orfSize);
idValAdd(lm, pList, featureId+1, ortho->ratio);
idValAdd(lm, pList, featureId+2, ortho->ratio * (1.0 - (double)ortho->missing/orfSize));
idValAdd(lm, pList, featureId+3, ortho->orthoSize);
idValAdd(lm, pList, featureId+4, ortho->ratio * orfSize);
}

void outputOneCds(struct cdsEvidence *start, struct cdsEvidence *end, struct bed *bed, 
	struct txInfo *info, struct hashEl *orthoEl, FILE *f)
/* Assemble together all of the evidence and write it as line to svm_light input file. 
 * This line is of the format:
 *    train id:val id:val ... id:val # transcript:start-end
 * where train is +1 for positive training examples, -1 for negative training
 * examples and 0 if not training.  The id's are integers representing the type of
 * data. The val are floating point numbers. */
{
struct lm *lm = lmInit(0);
boolean outputAll = (!good || !mostlyTrue());
struct idVal *iv, *list = NULL;

/* Deal with training status. */
if (good)
    fprintf(f, "+1 ");
else if (bad)
    fprintf(f, "-1 ");
else 
    fprintf(f, "0 ");

/* Output features we always have: txSize, orfSize, start, distance from end, start/stop complete. */
int txSize = bedTotalBlockSize(bed);
int orfSize = start->end - start->start;
fprintf(f, "1:%d ", txSize);
fprintf(f, "2:%d ", orfSize);
fprintf(f, "3:%d ", start->start);
fprintf(f, "4:%d ", txSize - start->end);
fprintf(f, "5:%f ", (double)orfSize/txSize);

/* To avoid paying attention to things that are always true in training set,
 * only output cdsStart/cdsEnd sometimes. */
if (outputAll)
    {
    fprintf(f, "6:%d ", start->startComplete);
    fprintf(f, "7:%d ", start->endComplete);
    }

/* Loop through all of the cdsEvidence we have, outputting features from it. */
struct cdsEvidence *cds;
for (cds = start; cds != end; cds = cds->next)
    {
    char *source = cds->source;
    if (outputAll)
	{
	if (sameString(source, "RefSeqReviewed"))
	    outEvidence(lm, 10, cds, &list);
	else if (sameString(source, "RefPepReviewed"))
	    outEvidence(lm, 20, cds, &list);
	}
    if (sameString(source, "RefSeqValidated"))
	outEvidence(lm, 15, cds, &list);
    else if (sameString(source, "RefPepValidated"))
	outEvidence(lm, 25, cds, &list);
    else if (sameString(source, "swissProt"))
        outEvidence(lm, 30, cds, &list);
    else if (sameString(source, "RefSeqProvisional"))
        outEvidence(lm, 35, cds, &list);
    else if (sameString(source, "RefPepProvisional"))
        outEvidence(lm, 40, cds, &list);
    else if (sameString(source, "MGCfullLength"))
        outEvidence(lm, 45, cds, &list);
    else if (sameString(source, "MGCfullLengthSynthetic"))
        outEvidence(lm, 50, cds, &list);
    else if (sameString(source, "RefSeqPredicted"))
        outEvidence(lm, 55, cds, &list);
    else if (sameString(source, "RefPepPredicted"))
        outEvidence(lm, 60, cds, &list);
    else if (sameString(source, "RefSeqInferred"))
        outEvidence(lm, 65, cds, &list);
    else if (sameString(source, "RefPepInferred"))
        outEvidence(lm, 70, cds, &list);
    else if (sameString(source, "genbankCds"))
        outEvidence(lm, 75, cds, &list);
    else if (sameString(source, "trembl"))
        outEvidence(lm, 80, cds, &list);
    else if (sameString(source, "bestorf"))
        outEvidence(lm, 85, cds, &list);
    else
        warn("Unknown source %s\n", source);
#ifdef SOON
#endif /* SOON */
    }

/* Output orthology stuff. */
struct hashEl *el;
for (el = orthoEl; el != NULL; el = hashLookupNext(el))
    {
    struct cdsOrtho *ortho = el->val;
    char *db = ortho->species;
    if (startsWith("panTro", db))
        outOrtho(lm, 100, ortho, orfSize, &list);
    else if (startsWith("rheMac", db))
        outOrtho(lm, 105, ortho, orfSize, &list);
    else if (startsWith("mm", db))
        outOrtho(lm, 110, ortho, orfSize, &list);
    else if (startsWith("canFam", db))
        outOrtho(lm, 120, ortho, orfSize, &list);
    else if (startsWith("rn", db))
        outOrtho(lm, 125, ortho, orfSize, &list);
    else if (startsWith("hg", db))
        outOrtho(lm, 130, ortho, orfSize, &list);
    }

/* Output some info stuff. */
idValAdd(lm, &list, 140, info->nonsenseMediatedDecay);
idValAdd(lm, &list, 141, info->retainedIntron);
idValAdd(lm, &list, 142, info->bleedIntoIntron);
idValAdd(lm, &list, 143, info->strangeSplice);
idValAdd(lm, &list, 144, info->cdsSingleInIntron);
idValAdd(lm, &list, 145, info->cdsSingleInUtr3);

/* Sort list, because svm_learn insists on it, and output. */
slSort(&list, idValCmp);
list = skipDupes(list);
for (iv = list; iv != NULL; iv = iv->next)
    fprintf(f, "%d:%f ", iv->id, iv->val);

/* Output end bit. */
fprintf(f, "# %s:%d-%d\n", start->name, start->start, start->end);

lmCleanup(&lm);
}

void txCdsSvmInput(char *inBed, char *inTce, char *inOrtho, char *inInfo, char *outList, char *outSvm)
/* txCdsSvmInput - Create input for svm_light, a nice support vector machine classifier.. */
{
/* Load up hash full of beds. */
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);
struct hash *bedHash = hashNew(18);
for (bed = bedList; bed != NULL; bed = bed->next)
    hashAdd(bedHash, bed->name, bed);

/* Load up hash full of info */
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
struct hash *infoHash = hashNew(18);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Load orthology information and put in hash. */
struct cdsOrtho *ortho, *orthoList = cdsOrthoLoadAll(inOrtho);
struct hash *orthoHash = hashNew(18);
for (ortho = orthoList; ortho != NULL; ortho = ortho->next)
    hashAdd(orthoHash, orthoHashKey(ortho->name, ortho->start, ortho->end), ortho);

/* Load in other evidence and sort. */
struct cdsEvidence *cdsList = cdsEvidenceLoadAll(inTce);
slSort(&cdsList, cdsEvidenceCmp);

/* Open output files. */
FILE *fList = mustOpen(outList, "w");
FILE *fSvm = mustOpen(outSvm, "w");

/* Stream through evidence, processing all the records on the same orf in the
 * same transcript at once. */
struct cdsEvidence *start, *end;
for (start = cdsList; start != NULL; start = end)
    {
    for (end = start->next; end != NULL; end = end->next)
	{
	if (end == NULL)
	    break;
	if (cdsEvidenceCmp(&start,&end) != 0)
	    break;
	}
    fprintf(fList, "%s\t%d\t%d\n", start->name, start->start, start->end);
    struct hashEl *orthoEl = hashLookup(orthoHash, orthoHashKey(start->name, start->start, start->end));
    if (orthoEl == NULL)
        errAbort("%s is in %s but not %s", start->name, inTce, inOrtho);
    bed = hashFindVal(bedHash, start->name);
    if (bed == NULL)
        errAbort("%s is in %s but not %s", start->name, inTce, inBed);
    info = hashFindVal(infoHash, start->name);
    if (info == NULL)
        errAbort("%s is in %s but not %s", start->name, inTce, inInfo);
    outputOneCds(start, end, bed, info, orthoEl, fSvm);
    }

/* Clean up and go home. */
carefulClose(&fList);
carefulClose(&fSvm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
bad = optionExists("bad");
good = optionExists("good");
if (good && bad)
    errAbort("The good and bad options are mutually exclusive.");
txCdsSvmInput(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);

return 0;
}
