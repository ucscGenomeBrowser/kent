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
#include "net.h"

boolean extToolsEnabled()
/* Return TRUE if we can display the external tools menu. */
{
return fileExists("extTools.ra");
}

void printExtMenuData(char *chromName) 
/* print the external tools aka "send to" menu entries as a javascript list to stdout */
{
if (!extToolsEnabled())
    return;

struct extTool *extTools = NULL;

// we allow an alternative location in /gbdb/hgFixed
// This means that we do not have to do a build patch when 
// we have to change this .ra file and can do it with a simple
// file push to the RR
if (fileExists("/gbdb/hgFixed/extTools.ra"))
    extTools = readExtToolRa("/gbdb/hgFixed/extTools.ra");
else
    extTools = readExtToolRa("extTools.ra");

struct extTool *et;
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "extTools = [\n");
for(et = extTools; et != NULL; et = et->next)
    {
    // special case for alternate chroms on hg38, skip the Ensembl links
    if (chromName!=NULL && startsWith("ensembl", et->tool) && endsWith(chromName, "_alt"))
        continue;

    if (et->dbs!=NULL)
        {
        if (!slNameInList(et->dbs, database))
            continue;
        }
    if (et->notDbs!=NULL)
        {
        if (slNameInList(et->notDbs, database))
            continue;
        }
    char* tool = jsonStringEscape(et->tool);
    char* shortLabel = jsonStringEscape(et->shortLabel);
    char* longLabel = jsonStringEscape(et->longLabel);
    dyStringPrintf(dy, "    ['%s', '%s', '%s', %d]", tool, shortLabel, longLabel, et->maxSize);
    if (et->next)
        dyStringAppend(dy, ",");
    dyStringAppend(dy, "\n");
    }
dyStringAppend(dy, "];\n");

jsInline(dy->string);
dyStringFree(&dy);

}

static char *cloneRaSetting(struct hash *hash, char *name, struct lineFile *lf, bool mustExist)
/* clone a setting out of the ra hash.  errAbort if mustExit and not found, otherwise NULL. */
{
char *str;
if ((str = hashFindVal(hash, name)) == NULL) 
    {
    if (mustExist)
        errAbort("missing required setting '%s' on line %d in file %s\n",
            name, lf->lineIx, lf->fileName);
    else
        return NULL;
    }
return cloneString(str);
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

    // pull out the required fields from the hash
    et->tool = cloneRaSetting(raHash, "tool", lf, TRUE);
    et->shortLabel = cloneRaSetting(raHash, "shortLabel", lf, TRUE);
    et->longLabel = cloneRaSetting(raHash, "longLabel", lf, TRUE);
    et->url = cloneRaSetting(raHash, "url", lf, TRUE);
    et->dbs = NULL;

    // optional fields
    et->email = cloneRaSetting(raHash, "email", lf, FALSE);
    if (hashFindVal(raHash, "onlyDbs"))
        et->dbs = commaSepToSlNames(hashFindVal(raHash, "onlyDbs"));
    if (hashFindVal(raHash, "notDbs"))
        et->notDbs = commaSepToSlNames(hashFindVal(raHash, "notDbs"));

    char *isHttpGet = hashFindVal(raHash, "isHttpGet");
    if (isHttpGet && \
     (sameString(isHttpGet, "yes") || sameString(isHttpGet, "on") || sameString(isHttpGet, "true")))
            et->isHttpGet = TRUE;

    char *maxSize = hashFindVal(raHash, "maxSize");
    if (maxSize!=NULL)
        et->maxSize = sqlUnsignedOrError(maxSize, "Error: maxSize %s for tool %s is not a number", \
                maxSize, et->tool);

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
printf("<html>\n<head>\n");
generateCspMetaHeader(stdout);
printf("</head>\n<body>\n");

if (debug)
    printf("Target URL: %s<p>", et->url);

char *chromName;
int winStart, winEnd;
char *db = cartString(cart, "db");
char *pos = cartString(cart, "position");

// Try to deal with virt chrom position used by hgTracks.
if (startsWith("virt:", cartUsualString(cart, "position", "")))
    pos = cartString(cart, "nonVirtPosition");

findGenomePos(db, pos, &chromName, &winStart, &winEnd, cart);
int len = winEnd-winStart;

char start1[255];
safef(start1, sizeof(start1), "%d", winStart+1);

char *url = replaceInUrl(et->url, "", cart, db, chromName, winStart, winEnd, NULL, TRUE);

char *method = "POST";
if (et->isHttpGet)
    method = "GET";

printf("<form id=\"redirForm\" method=\"%s\" action=\"%s\">\n", method, url);

struct slPair *slp;

if (et->maxSize!=0 && len > et->maxSize)
    {
    printf("Sorry, this tool accepts only a sequence with less than %d base pairs<p>\n"
      "Please zoom in some more.<p>\n", et->maxSize);
    return;
    }

printf("You're being redirected from the UCSC Genome Browser to the site %s<br>\n", url);
if (et->email)
    printf("Please contact %s for questions on this tool.<br>\n", et->email);

boolean submitDone = FALSE;
for (slp=et->params; slp!=NULL; slp=slp->next)
    {
    char* val = slp->val;
    if (sameWord(val, "$db"))
        val = db;
    if (sameWord(val, "$position"))
        val = pos;
    if (sameWord(val, "$start1"))
        val = start1;
    if (sameWord(val, "$returnUrl"))
        {
        // get the full URL of this hgTracks page, so external page can construct a custom track
        // and link back to us
        char* host = getenv("HTTP_HOST");
        char* reqUrl = getenv("REQUEST_URI");
        // remove everything after ? in URL
        char *e = strchr(reqUrl, '?');
        if (e) *e = 0; 

        char url[4000];
        // cannot find a way to find out if the request came in via http or https
        safef(url, sizeof(url), "http://%s%s", host, reqUrl);
        val = url;
        }
    // half the current window size
    if (stringIn("$halfLen", val))
        {
        char buf[64];
        safef(buf, sizeof(buf), "%d", len/2);
        val = replaceChars(val, "$halfLen", buf);
        }
    if (sameWord(val, "$seq") || sameWord(val, "$faSeq"))
        {
        static struct dnaSeq *seq = NULL;
        seq = hDnaFromSeq(db, chromName, winStart, winEnd, dnaLower);
        if (sameWord(val, "$seq"))
            val = seq->dna;
        else
            {
            val = catTwoStrings(">sequence\n",seq->dna);
            freez(&seq);
            }
        }
    // any remaining $-expression might be one of the general ones
    if (stringIn("$", val))
        {
        val = replaceInUrl(val, "", cart, db, chromName, winStart, winEnd, NULL, TRUE);
        }

    // output
    if (debug)
        {
        printf("%s: ", slp->name);
        printf("<input name=\"%s\" value=\"%s\"><p>\n", slp->name, val);
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
            printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", slp->name, val);
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
    {
    jsInline("document.getElementById(\"redirForm\").submit();\n");
    }
jsInlineFinish();
printf("</body></html>\n");
}
