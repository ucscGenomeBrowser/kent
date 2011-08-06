/* Handle variable substitutions in strings from trackDb and other
 * labels. See trackDb/README for descriptions of values that can be
 * substitute.  This code needs to do a special handle to remain compatibility
 * with behavior of the old substitution mechanism. */
#include "common.h"
#include "trackDb.h"
#include "hdb.h"
#include "hui.h"
#include "sqlNum.h"
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
    || (strcasecmp(varBase, "db") == 0);
}

static char *valOrDb(char *val, char *database)
/* return val if not-null, or a clone of database if it is null */
{
if (val == NULL)
    val = cloneString(database);
return val;
}

static void substDatabaseVar(char *database, char *varBase,
                             struct dyString *dest)
/* substitute a variable resolved from the database name.
 * Specify the base name, excluding the o_ prefix. If database
 * can be looked up, just substitute the database name. */
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
else
    dyStringAppend(dest, lookupTrackDbSubVar(desc, tdb, varName, varName));
}

static void substVar(char *desc, struct trackDb *tdb, char *database,
                     char *varName, struct dyString *dest)
/* look up varName and insert value in output string.  Error if variable
 * can't be found */
{
if (isDatabaseVar(varName))
    substDatabaseVar(database, varName, dest);
else if (tdb == NULL)
    errAbort("invalid variable \"%s\" to substitute in %s",
             varName, desc);
else if (startsWith("o_", varName) && isDatabaseVar(varName+2))
    substDatabaseVar(lookupOtherDb(desc, tdb, varName), varName+2, dest);
else
    substTrackDbVar(desc, tdb, database, varName, dest);
}

char *hVarSubst(char *desc, struct trackDb *tdb, char *database, char *src)
/* Parse a string and substitute variable references.  Return NULL if
 * no variable references were found.  Error on missing variables (except
 * $matrix).  desc is a brief description to print on an error to help with
 * debugging. tdb maybe NULL to only do substitutions based on database
 * and organism. See trackDb/README for more information.*/
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
    else
        {
        // variable reference
        start = next = parseVarName(desc, next, varName, sizeof(varName));
        substVar(desc, tdb, database, varName, dest);
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

void hVarSubstInVar(char *desc, struct trackDb *tdb, char *database, char **varPtr)
/* hVarSubst on a dynamically allocated string, replacing string in substitutions
 * occur, freeing the old memory if necessary.  See hVarSubst for details.
 */
{
char *dest = hVarSubst(desc, tdb, database, *varPtr);
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
hVarSubstInVar(tdb->track, tdb, database, &tdb->html);
}
