/* Handle variable substitutions in strings from trackDb and other
 * labels. See trackDb/README for descriptions of values that can be
 * substitute.  This code needs to do a special handle to remain compatibility
 * with behavior of the old substitution mechanism. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "trackDb.h"
#include "hdb.h"
#include "hui.h"
#include "sqlNum.h"
#include "hubConnect.h"
#include "htmshell.h"
#include "hVarSubst.h"

static boolean isVarEnd(boolean inBraces, char c)
/** does the character end a variable reference. */
{
if (inBraces)
    return (c == '}');
else
    return !((c == '_') || isalnum(c));
}

static char *parseVarName(char *desc, char *varStart, char *varName, int varNameSize)
/* parse substitution variable name out of a string, returning next position
 * after name. */
{
char *p = varStart+1;
if (*p == '\0')
    errAbort("trailing `$' while doing variable substitution in %s", desc);
boolean inBraces = (*p == '{');
if (inBraces)
    p++;
int nameIdx = 0;
while ((nameIdx < varNameSize-1) && !isVarEnd(inBraces, *p))
    varName[nameIdx++] = *p++;
if (nameIdx == 0)
    errAbort("empty variable name in %s varStart='%s' varName='%s'", desc, varStart, varName);
if (nameIdx == varNameSize)
    errAbort("variable name in desc %s exceeds maximum length of %d, starting with: \"%.*s\"",
             desc, varNameSize-1, varNameSize-1, varName);
varName[nameIdx] = '\0';
if (inBraces)
    p++;
return p;
}

static char *parseVarNameMaybe(char *varStart, char *varName, int varNameSize,
                               boolean *retInBraces)
/* Like parseVarName, but return NULL instead of aborting when what follows the `$' is not
 * a well formed variable reference.  Used in html mode, where a stray dollar sign in a
 * description page has to survive untouched.  retInBraces, if given, says whether the
 * reference was written ${name} rather than $name. */
{
char *p = varStart+1;
boolean inBraces = (*p == '{');
if (retInBraces != NULL)
    *retInBraces = inBraces;
if (inBraces)
    p++;
int nameIdx = 0;
while ((nameIdx < varNameSize-1) && (*p != '\0') && !isVarEnd(inBraces, *p))
    varName[nameIdx++] = *p++;
if ((nameIdx == 0) || (nameIdx == varNameSize-1))
    return NULL;
varName[nameIdx] = '\0';
if (inBraces)
    {
    if (*p != '}')
        return NULL;
    p++;
    }
return p;
}

static char *lookupTrackDbSubVar(char *desc, struct trackDb *tdb, char *settingName, char *varName)
/* get the specified track setting to substitute or die trying; more useful
 * message than trackDbRequiredSetting when doing substitution */
{
char *val = trackDbSettingClosestToHome(tdb, settingName);
if (val == NULL)
   errAbort("trackDb (%s) setting \"%s\" not found for variable substitution of \"$%s\" in %s",
            tdb->track, settingName, varName, desc);
return val;
}

static char *lookupOtherDb(char *desc, struct trackDb *tdb, char *varName)
/* look up the otherDb variable, which is needed for substituting varName */
{
return lookupTrackDbSubVar(desc, tdb, "otherDb", varName);
}

static void insertLinearGapHtml(struct trackDb *tdb, char *linearGap,
                             struct dyString *dest)
/* Generate HTML table from chain linearGap variable */
{
if (sameWord("medium",linearGap))
    {
dyStringPrintf(dest, "<PRE>-linearGap=%s\n\n\
tableSize    11\n\
smallSize   111\n\
position  1   2   3   11  111  2111  12111  32111   72111  152111  252111\n\
qGap    350 425 450  600  900  2900  22900  57900  117900  217900  317900\n\
tGap    350 425 450  600  900  2900  22900  57900  117900  217900  317900\n\
bothGap 750 825 850 1000 1300  3300  23300  58300  118300  218300  318300\n\
</PRE>", linearGap);
    }
else if (sameWord("loose", linearGap))
    {
dyStringPrintf(dest, "<PRE>-linearGap=%s\n\n\
tablesize    11\n\
smallSize   111\n\
position  1   2   3   11  111  2111  12111  32111  72111  152111  252111\n\
qGap    325 360 400  450  600  1100   3600   7600  15600   31600   56600\n\
tGap    325 360 400  450  600  1100   3600   7600  15600   31600   56600\n\
bothGap 625 660 700  750  900  1400   4000   8000  16000   32000   57000\n\
</PRE>", linearGap);
    }
else
    errAbort("Invalid chainLinearGap specified '%s', can only be 'medium' or 'loose'", linearGap);
}

static void insertMatrixHtml(struct trackDb *tdb, char *matrix,
                             char *matrixHeader, struct dyString *dest)
/* Generate HTML table from matrix setting in trackDb. matrixHeader is
 * optional. */
{
char *words[100];
char *headerWords[10];
int size;
int i, j, k;
int wordCount = 0, headerCount = 0;

wordCount = chopString(cloneString(matrix), ", \t", words, ArraySize(words));
if (matrixHeader != NULL)
    headerCount = chopString(cloneString(matrixHeader),
                    ", \t", headerWords, ArraySize(headerWords));
errno = 0;
size = sqrt(sqlDouble(words[0]));
if (errno)
    errAbort("Invalid matrix size in for track %s: %s\n", tdb->track,
             words[0]);
dyStringAppend(dest, "The following matrix was used:<P>\n");
k = 1;
dyStringAppend(dest, "<BLOCKQUOTE><TABLE class='chainTbl'>\n");
if (matrixHeader)
    {
    dyStringAppend(dest, "<TR ALIGN=right><TD>&nbsp;</TD>");
    for (i = 0; i < size && i < headerCount; i++)
        dyStringPrintf(dest, "<TD><B>%s</B></TD>", headerWords[i]);
    dyStringAppend(dest, "</TR>\n");
    }
for (i = 0; i < size; i++)
    {
    dyStringAppend(dest, "<TR ALIGN=right>");
    if (matrixHeader)
        dyStringPrintf(dest, "<TD><B>%s<B></TD>", headerWords[i]);
    for (j = 0; j < size && k < wordCount ; j++)
        dyStringPrintf(dest, "<TD>%s</TD>", words[k++]);
    dyStringAppend(dest, "</TR>\n");
    }
dyStringAppend(dest, "</TABLE></BLOCKQUOTE></P>\n");
}

static void substLinearGap(struct trackDb *tdb, struct dyString *dest)
/* Generate HTML table from matrix setting in trackDb.  Note: for
 * compatibility, substitutes and empty string if matrix setting not found in
 * trackDb. */
{
char *linearGap = trackDbSettingClosestToHome(tdb, "chainLinearGap");
if (linearGap != NULL)
    insertLinearGapHtml(tdb, linearGap, dest);
}

static void substMatrixHtml(struct trackDb *tdb, struct dyString *dest)
/* Generate HTML table from matrix setting in trackDb.  Note: for
 * compatibility, substitutes and empty string if matrix setting not found in
 * trackDb. */
{
char *matrix = trackDbSettingClosestToHome(tdb, "matrix");
if (matrix != NULL)
    insertMatrixHtml(tdb, matrix, trackDbSettingClosestToHome(tdb, "matrixHeader"), dest);
}

static boolean isAbbrevScientificName(char *name)
/* Return true if name looks like an abbreviated scientific name
* (e.g. D. yakuba). */
{
return (name != NULL && strlen(name) > 4 &&
	isalpha(name[0]) &&
	name[1] == '.' && name[2] == ' ' &&
	isalpha(name[3]));
}

static boolean isDatabaseVar(char *varBase)
/* Is this a variable that can be resolved only from the database name?
 * Specify the base name, excluding the o_ prefix. */
{
return (strcasecmp(varBase, "organism") == 0)
    || (strcasecmp(varBase, "date") == 0)
    || (strcasecmp(varBase, "linkToGatewayPage") == 0)
    || (strcasecmp(varBase, "db") == 0)
    || (strcasecmp(varBase, "hgsid") == 0);
}

static char *valOrDb(char *val, char *database)
/* return val if not-null, or a clone of database if it is null */
{
if (val == NULL)
    val = cloneString(database);
return val;
}

static void substDatabaseVar(char *database, struct cart *cart, boolean deferHgsid,
                             char *varBase, struct dyString *dest)
/* substitute a variable resolved from the database name.
 * Specify the base name, excluding the o_ prefix. If database
 * can be looked up, just substitute the database name.
 * deferHgsid asks for $hgsid to be written back out for a later pass instead of resolved;
 * it is only ever set for the html field, where there is a later pass to do it. */
{
if (sameString(varBase, "Organism"))
    {
    char *org = valOrDb(hOrganism(database), database);
    dyStringAppend(dest, org);
    freeMem(org);
    }
else if (sameString(varBase, "ORGANISM"))
    {
    char *org = hOrganism(database);
    if (org != NULL)
        touppers(org);
    else
        org = valOrDb(org, database);
    dyStringAppend(dest, org);
    freeMem(org);
    }
else if (sameString(varBase, "organism"))
    {
    char *org = hOrganism(database);
    if ((org != NULL) && !isAbbrevScientificName(org))
            tolowers(org);
    else
            org = valOrDb(org, database);
    dyStringAppend(dest, org);
    freeMem(org);
    }
else if (sameString(varBase, "date"))
    {
    char *date = valOrDb(hFreezeDateOpt(database), database);
    dyStringAppend(dest, date);
    freeMem(date);
    }
else if (sameString(varBase, "db"))
    dyStringAppend(dest, database);
else if (sameString(varBase, "hgsid"))
    {
    if (cart != NULL)
        dyStringAppend(dest, cartSessionId(cart));
    else if (deferHgsid)
        // hgTrackDb loading the html field.  A session id is per-request and cannot be baked
        // into the trackDb table, so write the reference back out and let the CGI that renders
        // the page resolve it.  Always in braces: that is the only form the render pass acts
        // on, so an escaped `$$hgsid' -- which arrives here already collapsed to `$hgsid' and
        // never reaches this branch -- is left alone by that pass (see nativeHtmlVars).
        dyStringAppend(dest, "${hgsid}");
    // Otherwise expand to nothing, which is what every caller without a cart has always done.
    // Writing the reference back out here would leak the literal text "${hgsid}" into
    // shortLabel and longLabel, and into hgNear's column html, none of which get a later pass.
    }
}

static char *parentTrackName(struct trackDb *tdb)
/* Name of the container holding tdb, in the form hgTrackUi's g= parameter needs: with the
 * hub_<id>_ prefix when this is a hub track, since that is what trackHubAddNamePrefix put
 * into tdb->track.  Views are skipped, a view has no description page of its own.  Returns
 * the track's own name when it is not in a container. */
{
struct trackDb *parent = tdb->parent;
char *viewName = NULL;
while ((parent != NULL) && tdbIsView(parent, &viewName))
    parent = parent->parent;
return (parent != NULL) ? parent->track : tdb->track;
}

/* The variables a hub's description page may use.  Deliberately a short explicit list and
 * not every trackDb setting the way native trackDb allows: a hub page written before this
 * substitution existed can easily contain something like "$track" inside a shell example,
 * and silently rewriting that would be worse than not substituting at all.
 *
 * $hgsid is deliberately not here.  A hub's description page is written by someone else and
 * is only lightly sanitized (htmlSanitize allows an <img> with an http src), so a page
 * containing <img src="https://example.com/px?s=${hgsid}"> would hand the reader's session
 * id to the hub's server, and a session id on its own is enough to read and write that
 * cart.  Nothing in a hub needs it: a link back into the browser works without one. */
static char *hubHtmlVars[] = {"db", "track", "parentTrack",
                              "organism", "Organism", "ORGANISM", "date", "downloadsServer"};

/* A native page has already been through hgTrackDb, which resolved everything it could and
 * left only $hgsid.  Substituting anything else here would be wrong as well as pointless:
 * hgTrackDb turns an escaped `$$db' into a literal `$db', and a second pass over the full
 * list would then expand it. */
static char *nativeHtmlVars[] = {"hgsid"};

static boolean isHtmlVar(struct cart *cart, struct trackDb *tdb, char *varName,
                         char **htmlVars, int htmlVarCount)
/* Is varName one of the variables this description page may use, and can this call resolve
 * it?  Asked only in html mode, to tell a variable reference from a dollar sign that
 * happens to be followed by a word. */
{
if (tdb == NULL)
    return FALSE;
if (sameString(varName, "hgsid") && (cart == NULL))
    return FALSE;
int i;
for (i = 0;  i < htmlVarCount;  i++)
    if (sameString(varName, htmlVars[i]))
        return TRUE;
return FALSE;
}

static void substTrackDbVar(char *desc, struct trackDb *tdb, char *database,
                            char *varName, struct dyString *dest)
/* substitute a variable value obtained from trackDb */
{
if (sameString(varName, "matrix"))
    substMatrixHtml(tdb, dest);
else if (sameString(varName, "chainLinearGap"))
    substLinearGap(tdb, dest);
else if (sameString(varName, "downloadsServer"))
    dyStringAppend(dest, hDownloadsServer());
else if (sameString(varName, "track"))
    dyStringAppend(dest, tdb->track);
else if (sameString(varName, "parentTrack"))
    dyStringAppend(dest, parentTrackName(tdb));
else
    dyStringAppend(dest, lookupTrackDbSubVar(desc, tdb, varName, varName));
}

static void substVar(char *desc, struct cart *cart, boolean deferHgsid, struct trackDb *tdb,
                     char *database, char *varName, struct dyString *dest)
/* look up varName and insert value in output string.  Error if variable
 * can't be found */
{
if (isDatabaseVar(varName))
    substDatabaseVar(database, cart, deferHgsid, varName, dest);
else if (tdb == NULL)
    errAbort("invalid variable \"%s\" to substitute in %s",
             varName, desc);
else if (startsWith("o_", varName) && isDatabaseVar(varName+2))
    substDatabaseVar(lookupOtherDb(desc, tdb, varName), cart, deferHgsid, varName+2, dest);
else
    substTrackDbVar(desc, tdb, database, varName, dest);
}

static char *hVarSubstExt(char *desc, struct cart *cart, struct trackDb *tdb, char *database,
                          char *src, char **htmlVars, int htmlVarCount, boolean bracesOnly,
                          boolean deferHgsid)
/* Parse a string and substitute variable references.  Return NULL if
 * no variable references were found.  Error on missing variables (except
 * $matrix).  desc is a brief description to print on an error to help with
 * debugging. tdb maybe NULL to only do substitutions based on database
 * and organism.  cart may be NULL. See trackDb/README for more information.
 * When htmlVars is given nothing is an error and only those variables are recognized:
 * every other `$' is copied through unchanged.  Pass NULL for the strict behaviour that
 * shortLabel, longLabel and native trackDb html need.
 * bracesOnly additionally requires the ${name} form, for a pass that runs over text an
 * earlier pass has already been through.  deferHgsid writes $hgsid back out instead of
 * resolving it; see substDatabaseVar. */
{
struct dyString *dest = NULL;
char *start = src;  // start of current static string in src
char *next = src;   // cursor
char varName[65];

while ((next = strchr(next, '$')) != NULL)
    {
    if (dest == NULL)
        dest = dyStringNew(strlen(src));
    dyStringAppendN(dest, start, next-start);
    if (*(next+1) == '$')
        {
        // $$ is a literal $
        dyStringAppendC(dest, '$');
        start = next = next + 2;
        }
    else if (htmlVars != NULL)
        {
        // variable reference, or just a dollar sign in the text
        boolean inBraces = FALSE;
        char *after = parseVarNameMaybe(next, varName, sizeof(varName), &inBraces);
        if ((after != NULL) && (inBraces || !bracesOnly)
            && isHtmlVar(cart, tdb, varName, htmlVars, htmlVarCount))
            {
            /* Escape the value before it goes into the page.  A hub's description html was
             * sanitized once, when the hub was read (trackHub.c, htmlSanitize); this pass
             * runs at render time, long after, so anything it inserted raw would be markup
             * that nothing had ever looked at.  Two of the variables are exactly that:
             * $organism and $date come straight out of a hub's genomes.txt with no
             * validation.  None of the variables in either list is meant to carry markup,
             * so escaping them all costs nothing and leaves no gap to keep track of. */
            struct dyString *raw = dyStringNew(64);
            substVar(desc, cart, deferHgsid, tdb, database, varName, raw);
            char *escaped = htmlEncode(raw->string);
            dyStringAppend(dest, escaped);
            freeMem(escaped);
            dyStringFree(&raw);
            start = next = after;
            }
        else
            {
            dyStringAppendC(dest, '$');
            start = next = next + 1;
            }
        }
    else
        {
        // variable reference
        start = next = parseVarName(desc, next, varName, sizeof(varName));
        substVar(desc, cart, deferHgsid, tdb, database, varName, dest);
        }
    }
if (dest != NULL)
    {
    dyStringAppend(dest, start);
    return dyStringCannibalize(&dest);
    }
else
    return NULL; // no substitutions
}

char *hVarSubst(char *desc, struct trackDb *tdb, char *database, char *src)
/* Parse a string and substitute variable references.  Return NULL if
 * no variable references were found.  Error on missing variables (except
 * $matrix).  desc is a brief description to print on error to help with
 * debugging. tdb maybe NULL to only do substitutions based on database
 * and organism. See trackDb/README for more information.*/
{
return hVarSubstExt(desc, NULL, tdb, database, src, NULL, 0, FALSE, FALSE);
}

void hVarSubstInVar(char *desc, struct trackDb *tdb, char *database, char **varPtr)
/* hVarSubst on a dynamically allocated string, replacing string in substitutions
 * occur, freeing the old memory if necessary.  See hVarSubst for details.
 */
{
char *dest = hVarSubstExt(desc, NULL, tdb, database, *varPtr, NULL, 0, FALSE, FALSE);
if (dest != NULL)
    {
    freez(varPtr);
    *varPtr = dest;
    }
}

void hVarSubstTrackDb(struct trackDb *tdb, char *database)
/* Substitute variables in trackDb shortLabel, longLabel, and html fields. */
{
hVarSubstInVar(tdb->track, tdb, database, &tdb->shortLabel);
hVarSubstInVar(tdb->track, tdb, database, &tdb->longLabel);
/* The html field alone defers $hgsid, because it alone gets a second pass at render time
 * (hVarSubstTrackDbHtml).  The labels do not, so a deferred reference in one of them would
 * reach the user as the literal text "${hgsid}". */
char *dest = hVarSubstExt(tdb->track, NULL, tdb, database, tdb->html, NULL, 0, FALSE, TRUE);
if (dest != NULL)
    {
    freez(&tdb->html);
    tdb->html = dest;
    }
}

void hVarSubstWithCart(char *desc, struct cart *cart, struct trackDb *tdb, char *database,
                       char **varPtr)
/* Like hVarSubstInVar, but if cart is non-NULL, $hgsid will be substituted. */
{
char *dest = hVarSubstExt(desc, cart, tdb, database, *varPtr, NULL, 0, FALSE, FALSE);
if (dest != NULL)
    {
    freez(varPtr);
    *varPtr = dest;
    }
}

void hVarSubstTrackDbHtml(struct cart *cart, struct trackDb *tdb, char *database)
/* Substitute variables in a track's description page, at render time, where there is a
 * cart and where $db, $hgsid and $parentTrack resolve to the hub_<id>_ names the CGIs
 * actually use.  How much is left to do depends on where the page came from.
 *
 * A hub's html comes straight off the hub's web server and has never been through
 * substitution, so the whole of hubHtmlVars is resolved here.  A native page was already
 * substituted by hgTrackDb when it loaded trackDb, and all that is left is $hgsid, which
 * hgTrackDb had to defer because a session id is per-request.
 *
 * Nothing is an error either way, so a dollar sign in a description page that was not
 * written with this in mind stays a dollar sign. */
{
if ((tdb == NULL) || isEmpty(tdb->html))
    return;
char **htmlVars = nativeHtmlVars;
int htmlVarCount = ArraySize(nativeHtmlVars);
/* A native page has been through hgTrackDb already, so only the ${hgsid} that pass deferred
 * may be acted on here.  Requiring the braces is what keeps an escaped `$$hgsid' escaped:
 * hgTrackDb collapses it to a bare `$hgsid', which this pass then leaves alone. */
boolean bracesOnly = TRUE;
if (isHubTrack(tdb->track))
    {
    /* A hub page has never been substituted, so the full list applies and both $hgsid and
     * ${hgsid} are meant to work.  One pass, so `$$hgsid' escapes normally. */
    htmlVars = hubHtmlVars;
    htmlVarCount = ArraySize(hubHtmlVars);
    bracesOnly = FALSE;
    }
char *dest = hVarSubstExt(tdb->track, cart, tdb, database, tdb->html, htmlVars, htmlVarCount,
                          bracesOnly, FALSE);
/* Assign without freeing, and only when something actually changed.  hVarSubstExt allocates
 * its buffer at the first `$' whether or not it substitutes anything, so dest is non-NULL for
 * any page that merely contains a dollar sign.  For a native track tdb->html can point into
 * the trackDb cache, which is localmem carved out of an mmap'd file (trackDbCache.c): that
 * pointer never came from malloc, and freeing it aborts the CGI.  The mapping is MAP_PRIVATE,
 * so storing a new pointer is fine.  The old string is left alone; it is either the cache's,
 * which is not ours to free, or one string per request in a CGI that is about to exit. */
if ((dest != NULL) && !sameString(dest, tdb->html))
    tdb->html = dest;
else
    freeMem(dest);
}
