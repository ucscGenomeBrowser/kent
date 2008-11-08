/* pcrResult -- support for internal track of hgPcr results. */

#include "common.h"
#include "hdb.h"
#include "hui.h"
#include "obscure.h"
#include "targetDb.h"
#include "pcrResult.h"

static char const rcsid[] = "$Id: pcrResult.c,v 1.10 2008/11/08 00:31:38 angie Exp $";

char *pcrResultCartVar(char *db)
/* Returns the cart variable name for PCR result track info for db. 
 * Don't free the result! */
{
static char buf[1024];
safef(buf, sizeof(buf), "%s_%s", PCR_RESULT_TRACK_NAME, db);
return buf;
}

#define setPtIfNotNull(pt, val) if (pt != NULL) *pt = val

boolean pcrResultParseCart(char *db, struct cart *cart, char **retPslFile,
			   char **retPrimerFile,
			   struct targetDb **retTarget)
/* Parse out hgPcrResult cart variable into components and make sure
 * they are valid.  If so, set *ret's and return TRUE.  Otherwise, null out 
 * *ret's and return FALSE (and clear the cart variable if it exists).  
 * ret's are ignored if NULL. */
{
char *cartVar = pcrResultCartVar(cartString(cart, "db"));
char *hgPcrResult = cartOptionalString(cart, cartVar);
if (isEmpty(hgPcrResult))
    {
    setPtIfNotNull(retPslFile, NULL);
    setPtIfNotNull(retPrimerFile, NULL);
    setPtIfNotNull(retTarget, NULL);
    return FALSE;
    }
static char buf[2048];
char *words[3];
int wordCount;
safecpy(buf, sizeof(buf), hgPcrResult);
wordCount = chopLine(buf, words);
if (wordCount < 2)
    errAbort("Badly formatted hgPcrResult variable: %s", hgPcrResult);
char *pslFile = words[0];
char *primerFile = words[1];
char *targetName = (wordCount > 2) ? words[2] : NULL;
struct targetDb *target = NULL;
if (isNotEmpty(targetName))
    target = targetDbLookup(db, targetName);

if (!fileExists(pslFile) || !fileExists(primerFile) ||
    (wordCount > 2 && target == NULL))
    {
    cartRemove(cart, cartVar);
    setPtIfNotNull(retPslFile, NULL);
    setPtIfNotNull(retPrimerFile, NULL);
    setPtIfNotNull(retTarget, NULL);
    return FALSE;
    }
setPtIfNotNull(retPslFile, cloneString(pslFile));
setPtIfNotNull(retPrimerFile, cloneString(primerFile));
setPtIfNotNull(retTarget, target);
if (retTarget == NULL)
    targetDbFreeList(&target);
return TRUE;
}

void pcrResultGetPrimers(char *fileName, char **retFPrimer, char **retRPrimer)
/* Given a file whose first line is 2 words (forward primer, reverse primer)
 * set the ret's to upper-cased primer sequences.  
 * Do not free the statically allocated ret's. */
{
static char fPrimer[1024], rPrimer[1024];;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[2];
if (! lineFileRow(lf, words))
    lineFileAbort(lf, "Couldn't read primers");
if (retFPrimer != NULL)
    {
    safecpy(fPrimer, sizeof(fPrimer), words[0]);
    touppers(fPrimer);
    *retFPrimer = fPrimer;
    }
if (retRPrimer != NULL)
    {
    safecpy(rPrimer, sizeof(rPrimer), words[1]);
    touppers(rPrimer);
    *retRPrimer = rPrimer;
    }
lineFileClose(&lf);
}

void pcrResultGetPsl(char *fileName, struct targetDb *target, char *item,
		     char *chrom, int itemStart, int itemEnd,
		     struct psl **retItemPsl, struct psl **retOtherPsls)
/* Read in psl from file.  If a psl matches the given item position, set 
 * retItemPsl to that; otherwise add it to retOtherPsls.  Die if no psl
 * matches the given item position. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct psl *itemPsl = NULL, *otherPsls = NULL;
char *pslFields[21];
while (lineFileRow(lf, pslFields))
    {
    struct psl *psl = pslLoad(pslFields);
    boolean gotIt = FALSE;
    if (target != NULL)
	{
	if (sameString(psl->tName, item))
	    gotIt = TRUE;
	}
    else if (sameString(psl->tName, chrom) && psl->tStart == itemStart &&
	     psl->tEnd == itemEnd)
	gotIt = TRUE;
    if (gotIt)
	itemPsl = psl;
    else
	slAddHead(&otherPsls, psl);
    }
lineFileClose(&lf);
if (itemPsl == NULL)
    {
    if (target != NULL)
	errAbort("Did not find record for amplicon in %s sequence %s",
		 target->description, item);
    else
	errAbort("Did not find record for amplicon at %s:%d-%d",
		 chrom, itemStart, itemEnd);
    }
if (retItemPsl != NULL)
    *retItemPsl = itemPsl;
else
    pslFree(&itemPsl);
if (retOtherPsls != NULL)
    {
    slSort(&otherPsls, pslCmpTarget);
    *retOtherPsls = otherPsls;
    }
else
    pslFreeList(&otherPsls);
}

struct trackDb *pcrResultFakeTdb()
/* Construct a trackDb record for PCR Results track. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->tableName = cloneString(PCR_RESULT_TRACK_NAME);
tdb->shortLabel = cloneString(PCR_RESULT_TRACK_LABEL);
tdb->longLabel = cloneString(PCR_RESULT_TRACK_LONGLABEL);
tdb->grp = cloneString("map");
tdb->type = cloneString("psl .");
tdb->priority = 100.01;
tdb->canPack = TRUE;
tdb->visibility = tvPack;
tdb->html = hFileContentsOrWarning(hHelpFile(PCR_RESULT_TRACK_NAME));
trackDbPolish(tdb);
if (tdb->settingsHash == NULL)
    tdb->settingsHash = hashNew(0);
hashAdd(tdb->settingsHash, BASE_COLOR_DEFAULT, cloneString("diffBases"));
hashAdd(tdb->settingsHash, BASE_COLOR_USE_SEQUENCE,
	cloneString(PCR_RESULT_TRACK_NAME));
hashAdd(tdb->settingsHash, SHOW_DIFF_BASES_ALL_SCALES, cloneString("."));
hashAdd(tdb->settingsHash, INDEL_DOUBLE_INSERT, cloneString("on"));
hashAdd(tdb->settingsHash, INDEL_QUERY_INSERT, cloneString("on"));
hashAdd(tdb->settingsHash, INDEL_POLY_A, cloneString("on"));
hashAdd(tdb->settingsHash, "nextItemButton", cloneString("off"));
return tdb;
}

char *pcrResultItemAccName(char *acc, char *name)
/* If a display name is given in addition to the acc, concatenate them
 * into a single name that must match a non-genomic target item's name
 * in the targetDb .2bit.  Do not free the result. */
{
static char accName[256];
if (isEmpty(name))
    safecpy(accName, sizeof(accName), acc);
else
    safef(accName, sizeof(accName), "%s__%s", acc, name);
return accName;
}

char *pcrResultItemAccession(char *nameIn)
/* If nameIn contains a concatenated accession and display name, returns
 * just the accession.  Do not free the result.*/
{
char *ptr = strstr(nameIn, "__");
if (ptr != NULL)
    {
    static char nameOut[128];
    safecpy(nameOut, sizeof(nameOut), nameIn);
    nameOut[ptr-nameIn] = '\0';
    return nameOut;
    }
return nameIn;
}

char *pcrResultItemName(char *nameIn)
/* If nameIn contains a concatenated accession and display name, returns
 * just the name.  If accession only, returns NULL.  Do not free the result.*/
{
char *ptr = strstr(nameIn, "__");
if (ptr != NULL)
    return ptr+2;
return NULL;
}

