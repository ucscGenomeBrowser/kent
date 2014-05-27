/* raMerge - Merge together info in two ra files, doing a record-by-record concatenation for the most part.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "ra.h"


boolean dupeOk = FALSE, firstOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raMerge - Merge together info in two ra files, producing records that are a\n"
  "concatenation of all the input records\n"
  "usage:\n"
  "   raMerge keyField file1.ra file2.ra ... fileN.ra\n"
  "options:\n"
  "   -dupeOk - will pass through multiple instances of fields with same name.\n"
  "   -firstOnly - only keep records that are started in the first file\n"
  );
}

static struct optionSpec options[] = {
   {"dupeOk", OPTION_BOOLEAN},
   {"firstOnly", OPTION_BOOLEAN},
   {NULL, 0},
};

void raMerge(char *keyField, int raCount, char *raFiles[])
/* raMerge - Merge together info in two ra files, doing a record-by-record 
 * concatenation for the most part.. */
{
struct hash *outerHash = hashNew(20);
int i;
boolean isFirstFile = TRUE;
for (i=0; i<raCount; ++i)
    {
    struct lineFile *lf = lineFileOpen(raFiles[i], TRUE);
    struct hash *newRa;
    while ((newRa = raNextRecord(lf)) != NULL)
	{
	char *id = hashFindVal(newRa, keyField);
	if (id == NULL)
	    errAbort("Missing %s field in record ending line %d of %s", keyField,
		    lf->lineIx, lf->fileName);
	struct hash *oldRa = hashFindVal(outerHash, id);
	if (oldRa == NULL)
	    {
	    if (isFirstFile || !firstOnly)
		hashAdd(outerHash, id, newRa);
	    }
	else
	    {
	    struct hashCookie cookie = hashFirst(newRa);
	    struct hashEl *hel;
	    while ((hel = hashNext(&cookie)) != NULL)
		{
		if (!sameString(hel->name, keyField))
		    {
		    if (!dupeOk && hashLookup(oldRa, hel->name))
			errAbort("Field %s duplicated in %s", hel->name, id);
		    hashAdd(oldRa, hel->name, lmCloneString(oldRa->lm, hel->val));
		    }
		}
	    hashFree(&newRa);
	    }
	}
    lineFileClose(&lf);
    isFirstFile = FALSE;
    }

struct hashEl *outerHel, *outerList = hashElListHash(outerHash);
slSort(&outerList, hashElCmp);
FILE *f = stdout;
for (outerHel = outerList; outerHel != NULL; outerHel = outerHel->next)
    {
    struct hash *ra = outerHel->val;
    char *id = hashMustFindVal(ra, keyField);
    fprintf(f, "%s %s\n", keyField, id);
    struct hashEl *raHel, *raList = hashElListHash(ra);
    slSort(&raList, hashElCmp);
    for (raHel = raList; raHel != NULL; raHel = raHel->next)
        {
	if (!sameString(raHel->name, keyField))
	    fprintf(f, "%s %s\n", raHel->name, (char *)raHel->val);
	}
    fprintf(f, "\n");
    slFreeList(&raList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
dupeOk = optionExists("dupeOk");
firstOnly = optionExists("firstOnly");
raMerge(argv[1], argc-2, argv+2);
return 0;
}
