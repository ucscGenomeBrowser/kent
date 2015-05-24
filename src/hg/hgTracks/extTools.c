// Code to parse list of external tools and redirect to it via a hidden form */
//
// The link from hgTracks has the form ?hgt.redirectTool=<name>
// Note that you can add the parameter &debug=1 to the URL to see and modify the hidden form
// cgi-bin/extTools.ra defines the parameters and labels of the tools

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hCommon.h"
#include "htmshell.h"
#include "hash.h"
#include "web.h"
#include "ra.h"
#include "hgTracks.h"
#include "extTools.h"
#include "hgFind.h"
#include "obscure.h"

static char *getRequiredRaSetting(struct hash *hash, char *name, struct lineFile *lf)
/* Grab a group setting out of the ra hash.  errAbort if not found. */
{
char *str;
if ((str = hashFindVal(hash, name)) == NULL) 
    errAbort("missing required setting '%s' on line %d in file %s\n",
	name, lf->lineIx, lf->fileName);
return str;
}

struct extTool *readExtToolRa(char *raFileName) 
/* parse the extTools.ra file. Inspired by trackHub.c:readGroupRa() */
{
struct lineFile *lf = udcWrapShortLineFile(raFileName, NULL, 1*1024*1024);
struct extTool *list = NULL;
//struct hash *ra;
struct slPair *raList;
while ((raList = raNextStanzAsPairs(lf)) != NULL)
    {
    struct extTool *et;
    AllocVar(et);

    struct slPair *paramList = NULL;

    struct hash *raHash;
    raHash = hashNew(0);

    // the "param" field is there more than once, so cannot use a hash for it
    struct slPair *raPair;
    for (raPair = raList; raPair != NULL; raPair = raPair->next)
        {
        if (sameWord(raPair->name, "param"))
            {
            // split the val into parameter name and value
            char *paramArr[2];
            int fCount = chopByWhite(raPair->val, paramArr, 2);
            // hacky way to allow spaces in parameter names and values
            char *name = replaceChars(paramArr[0], "%20", " ");
            char *val;
            if (fCount==2)
                val = cloneString(paramArr[1]);
            else
                val = cloneString("");
            val = replaceChars(val, "%20", " ");
            slPairAdd(&paramList, name, val);
            }
        else
            // add all other name/val pairs to the hash
            hashAdd(raHash, raPair->name, cloneString(raPair->val));
        }

    // pull out the normal fields from the hash
    et->tool = cloneString(getRequiredRaSetting(raHash, "tool", lf));
    et->shortLabel = cloneString(getRequiredRaSetting(raHash, "shortLabel", lf));
    char *maxSize = hashFindVal(raHash, "maxSize");
    if (maxSize!=NULL)
        et->maxSize = sqlUnsignedOrError(maxSize, "Error: maxSize %s for tool %s is not a number", \
                maxSize, et->tool);
    et->longLabel = cloneString(getRequiredRaSetting(raHash, "longLabel", lf));
    et->url = cloneString(getRequiredRaSetting(raHash, "url", lf));
    et->dbs = NULL;
    if (hashFindVal(raHash, "dbs"))
        et->dbs = commaSepToSlNames(hashFindVal(raHash, "dbs"));

    slReverse(&paramList);
    et->params = paramList;

    slAddHead(&list, et);
    freeHashAndVals(&raHash);
    slPairFree(&raList);
    }

if (list)
    slReverse(&list);
lineFileClose(&lf);
return list;
}

void extToolRedirect(struct cart *cart, char *tool)
/* redirect to an external tool, sending the data specified in the ext tools config file */
{
bool debug = cgiVarExists("debug");

struct extTool *extTools = readExtToolRa("extTools.ra");
struct extTool *et;
for (et = extTools; et != NULL; et = et->next)
    if (sameWord(et->tool, tool))
        break;
if (et==NULL)
    errAbort("No configuration found for tool %s", tool);

// construct an invisible CGI form with the given parameters
printf("<html><body>\n");

if (debug)
    printf("Target URL: %s<p>", et->url);

printf("<form id=\"redirForm\" method=\"POST\" action=\"%s\">\n", et->url);

struct slPair *slp;
char *db = cartString(cart, "db");
char *pos = cartString(cart, "position");

char *chromName;
int winStart, winEnd;
findGenomePos(db, pos, &chromName, &winStart, &winEnd, cart);
int len = winEnd-winStart;

if (et->maxSize!=0 && len > et->maxSize)
    {
    printf("Sorry, this tool accepts only a sequence with less than %d base pairs<p>\n"
      "Try to zoom in some more.<p>\n", et->maxSize);
    return;
    }

boolean submitDone = FALSE;
for (slp=et->params; slp!=NULL; slp=slp->next)
    {
    char* val = slp->val;
    if (sameWord(val, "$db"))
        val = db;
    if (sameWord(val, "$position"))
        val = pos;
    if (stringIn("$halfLen", val))
        {
        char buf[64];
        safef(buf, sizeof(buf), "%d", len/2);
        val = replaceChars(val, "$halfLen", buf);
        }
    if (sameWord(val, "$seq"))
        {
        static struct dnaSeq *seq = NULL;
        seq = hDnaFromSeq(db, chromName, winStart, winEnd, dnaLower);
        val = seq->dna;
        }
    // any remaining $-expression might be one of the general ones
    if (stringIn("$", val))
        val = replaceInUrl(val, "", cart, db, chromName, winStart, winEnd, NULL, TRUE);

    // output
    if (debug)
        {
        printf("%s: ", slp->name);
        printf("<input name=\"%s\" value=\"%s\"><p>", slp->name, val);
        }
    else
        {
        // parameter named submit is a special case
        if (sameWord(slp->name, "submit"))
            {
            submitDone = TRUE;
            printf("<input type=\"submit\" value=\"%s\" style=\"position: absolute; left: -9999px; "
                "width: 1px; height: 1px;\">\n", val);
            }
        else
            printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">", slp->name, val);
        }
    }

// a hidden submit button, see
// http://stackoverflow.com/questions/477691/submitting-a-form-by-pressing-enter-without-a-submit-button
if (debug)
    printf("<input type=\"submit\">\n");
else if (!submitDone)
    printf("<input type=\"submit\" style=\"position: absolute; left: -9999px; width: 1px; height: 1px;\">\n");
printf("</form>\n");
// a little javascript that clicks the submit button
if (!debug)
    printf("<script>document.getElementById(\"redirForm\").submit();</script>\n");
printf("</body></html>\n");
}
