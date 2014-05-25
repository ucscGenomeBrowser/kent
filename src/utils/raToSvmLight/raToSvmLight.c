/* raToSvmLight - Convert .ra file to feature vector input for svmLight.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "obscure.h"
#include "ra.h"


boolean good = FALSE;
boolean bad = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "raToSvmLight - Convert .ra file to feature vector input for svmLight.\n"
    "usage:\n"
    "    raToSvmLight in.ra keyField out.feature out.keys\n"
    "where:\n"
    "    in.ra is a ra file.  All fields except the keyField should be numeric\n"
    "    keyField is the name of the field that identifies the record, often 'name' or 'acc'\n"
    "    out.feature is the converted output, ready for svm_learn or svm_classify\n"
    "    out.keys - contains one line per ra record, with just the keyField value.\n"
    "         You want this because it is not in out.feature. The svm_learn/svm_classify\n"
    "          rely on  you keeping track externally what feature is in what line.\n"
    "options:\n"
    "    -good - Mark this as positive training set\n"
    "    -bad - Mark this as negative training set\n"
    "    -fields=fields.tab Save the field/numerical id values here\n"
  );
}

static struct optionSpec options[] = {
   {"good", OPTION_BOOLEAN},
   {"bad", OPTION_BOOLEAN},
   {"fields", OPTION_STRING},
   {NULL, 0},
};

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


void raToSvmLight(char *inFile, char *keyField, char *outFeatures, char *outKeys)
/* raToSvmLight - Convert .ra file to feature vector input for svmLight. */
{
/* Read file into a list of ra hashes.  Build up symbol table mapping */
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct hash *ra, *raList = NULL;
struct hash *symHash = hashNew(0);
int id = 0;
while ((ra = raNextRecord(lf)) != NULL)
    {
    struct hashCookie cookie = hashFirst(ra);
    struct hashEl *el;
    while ((el = hashNext(&cookie)) != NULL)
         {
	 if (!sameString(el->name, keyField))
	     if (!hashLookup(symHash, el->name))
		 hashAdd(symHash, el->name, NULL);
	 }
    slAddHead(&raList, ra);
    }
lineFileClose(&lf);
slReverse(&raList);

/* Alphabetize symbols and assign IDs.  (The alphabetization is so that
 * different files missing data in different places still end up with
 * same IDs. */
struct hashEl *el, *list = hashElListHash(symHash);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    {
    struct hashEl *realEl = hashLookup(symHash, el->name);
    realEl->val = intToPt(++id);
    }

/* For each ra, convert to feature list, sort, and output. */
FILE *f = mustOpen(outFeatures, "w");
for (ra = raList; ra != NULL; ra = ra->next)
    {
    struct lm *lm = lmInit(0);
    struct idVal *iv, *ivList = NULL;
    struct hashCookie cookie = hashFirst(ra);
    struct hashEl *el;
    while ((el = hashNext(&cookie)) != NULL)
         {
	 if (!sameString(el->name, keyField))
	     {
	     int id = hashIntVal(symHash, el->name);
	     char *valString = el->val;
	     char c = valString[0];
	     if (isdigit(c) || (c == '-' && isdigit(valString[1])))
		  idValAdd(lm, &ivList, id, atof(valString));
	     else
	          errAbort("%s has non-numeric value %s", el->name, valString);
	     }
	 }
    slSort(&ivList, idValCmp);
    if (good)
        fprintf(f, "+1");
    else if (bad)
        fprintf(f, "-1");
    else
        fprintf(f, "0");
    for (iv = ivList; iv != NULL; iv = iv->next)
        fprintf(f, " %d:%g", iv->id, iv->val);
    fprintf(f, "\n");
    }
carefulClose(&f);

/* Write out key fields in same order as feature vector lines. */
f = mustOpen(outKeys, "w");
for (ra = raList; ra != NULL; ra = ra->next)
    {
    char *key = hashMustFindVal(ra, keyField);
    fprintf(f, "%s\n", key);
    }
carefulClose(&f);

/* Optionally write out correspondence between feature field ID and ra field names. */
if (optionExists("fields"))
    {
    char *fileName = optionVal("fields", NULL);
    f = mustOpen(fileName, "w");
    struct hashEl *el, *elList = hashElListHash(symHash);
    slSort(&elList, hashElCmp);
    for (el = elList; el != NULL; el = el->next)
         {
	 fprintf(f, "%s\t%d\n", el->name, ptToInt(el->val));
	 }
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
good = optionExists("good");
bad = optionExists("bad");
if (bad && good)
    errAbort("The options good and bad don't go together.");
raToSvmLight(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
