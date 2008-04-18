/* pcrResult -- support for internal track of hgPcr results. */

#include "common.h"
#include "hdb.h"
#include "targetDb.h"
#include "pcrResult.h"

static char const rcsid[] = "$Id: pcrResult.c,v 1.1 2008/04/18 23:32:43 angie Exp $";

char *pcrResultCartVar(char *db)
/* Returns the cart variable name for PCR result track info for db. 
 * Don't free the result! */
{
static char buf[1024];
safef(buf, sizeof(buf), "hgPcrResult_%s", db);
return buf;
}

#define setPtIfNotNull(pt, val) if (pt != NULL) *pt = val

boolean pcrResultParseCart(struct cart *cart, char **retBedFile,
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
    setPtIfNotNull(retBedFile, NULL);
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
char *bedFile = words[0];
char *primerFile = words[1];
char *targetName = (wordCount > 2) ? words[2] : NULL;
struct targetDb *target = NULL;
if (isNotEmpty(targetName))
    target = targetDbLookup(hGetDb(), targetName);

if (!fileExists(bedFile) || !fileExists(primerFile) ||
    (wordCount > 2 && target == NULL))
    {
    cartRemove(cart, "hgPcrResult");
    setPtIfNotNull(retBedFile, NULL);
    setPtIfNotNull(retPrimerFile, NULL);
    setPtIfNotNull(retTarget, NULL);
    return FALSE;
    }
setPtIfNotNull(retBedFile, cloneString(bedFile));
setPtIfNotNull(retPrimerFile, cloneString(primerFile));
setPtIfNotNull(retTarget, target);
if (retTarget == NULL)
    targetDbFreeList(&target);
return TRUE;
}

