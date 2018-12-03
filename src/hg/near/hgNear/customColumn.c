/* customColumn - handle columns put in by users. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "portable.h"
#include "net.h"
#include "cart.h"
#include "hgNear.h"

static char *customFileName()
/* Return file name associated with custom column if any.
 * Delete cart var if it's grown stale. */
{
char *fileName = NULL;
if (cartVarExists(cart, customFileVarName))
    {
    fileName = cartString(cart, customFileVarName);
    if (!fileExists(fileName))
	{
        cartRemove(cart, customFileVarName);
	fileName = NULL;
	}
    }
return fileName;
}

static char *newCustomFileName()
/* Create new custom file name. */
{
struct tempName tn;
makeTempName(&tn, "near", ".col");
return cloneString(tn.forCgi);
}

void doCustomPage(struct sqlConnection *conn, struct column *colList)
/* Put up page to input custom columns. */
{
makeTitle("Setup Custom Columns for Gene Sorter", "hgNearHelp.html#Custom");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\">\n");
cartSaveSession(cart);

/* Put up descriptive text. */
controlPanelStart();
hPrintf(
"Add your own custom columns to the Gene Sorter using the "
"<A HREF=\"../goldenPath/help/customColumn.html\">"
"format described here</A>.");
controlPanelEnd();

/* Put up various buttons. */
controlPanelStart();
hPrintf("<TABLE><TR><TD>");
hPrintf("Custom column action: ");
hPrintf("</TD><TD>");
cgiMakeOptionalButton(customClearDoName, "clear", 
	customFileName() == NULL);
hPrintf("</TD><TD>");
cgiMakeButton(customPasteDoName, "paste in");
hPrintf("</TD><TD>");
cgiMakeButton(customUploadDoName, "from file");
hPrintf("</TD><TD>");
cgiMakeButton(customFromUrlDoName, "from URL");
hPrintf("</TD><TD>");
cgiMakeButton(confVarName, "cancel");
hPrintf("</TD></TR></TABLE>");
controlPanelEnd();

hPrintf("</FORM>");
}

static void submitCancel()
/* Put up little table with submit and cancle buttons. */
{
hPrintf("<TABLE><TR><TD>");
cgiMakeButton(customSubmitDoName, "submit");
hPrintf("</TD><TD>");
hPrintf(" ");
hPrintf("</TD><TD>");
cgiMakeButton(customPageDoName, "cancel");
hPrintf("</TD></TR></TABLE>");
}

void doCustomUpload(struct sqlConnection *conn, struct column *colList)
/* Put up page to upload custom columns. */
{
makeTitle("Upload Custom Columns", "hgNearHelp.html#Custom");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n");
cartSaveSession(cart);

controlPanelStart();
hPrintf(
"Upload a file with custom columns in the "
"<A HREF=\"../goldenPath/help/customColumn.html\">"
"format described here</A>.");
hPrintf("<BR>");
hPrintf("Column File: <INPUT TYPE=FILE NAME=%s> ", customUploadVarName);
hPrintf("<BR>");
submitCancel();
controlPanelEnd();

hPrintf("</FORM>");
}

void doCustomPaste(struct sqlConnection *conn, struct column *colList)
/* Put up page to paste custom columns. */
{
makeTitle("Paste in Custom Columns", "hgNearHelp.html#Custom");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=\"POST\">\n");
cartSaveSession(cart);

controlPanelStart();
hPrintf(
"Paste in custom columns in the "
"<A HREF=\"../goldenPath/help/customColumn.html\">"
"format described here</A>.");
hPrintf("<BR>");
cgiMakeTextArea(customPasteVarName, "", 10, 70);
submitCancel();
controlPanelEnd();

hPrintf("</FORM>");
}

void doCustomFromUrl(struct sqlConnection *conn, struct column *colList)
/* Put up page to paste in URLS with custom columns. */
{
makeTitle("Paste in Custom Columns", "hgNearHelp.html#Custom");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=\"POST\">\n");
cartSaveSession(cart);

controlPanelStart();
hPrintf(
"Paste in URL or list of URLs that refer to files in the "
"<A HREF=\"../goldenPath/help/customColumn.html\">"
"format described here</A>.");
hPrintf("<BR>");
cgiMakeTextArea(customFromUrlVarName, "", 10, 70);
submitCancel();
controlPanelEnd();

hPrintf("</FORM>");
}

/* ---- Parse custom columns. ---- */

static void addIfMissing(struct hash *settings, char *var, char *val)
/* Add var to hash if it's not there already. */
{
if (!hashLookup(settings, var))
    hashAdd(settings, var, cloneString(val));
}

static void outputSetting(FILE *f, char *name, char *val)
/* Output name=val pair,  adding quotes if val has spaces. */
{
fprintf(f, "%s=", name);
if (hasWhiteSpace(val))
    {
    if (strchr(val, '"') != NULL)
	{
	char *s = makeEscapedString(val, '"');
	fprintf(f, "\"%s\"", s);
	freez(&s);
	}
    else
	{
	fprintf(f, "\"%s\"", val);
	}
    }
else
    fprintf(f, "%s", val);
}

static void outputSettings(FILE *f, struct hash *settings)
/* Output settings hash as a var=val set. */
{
struct hashEl *list, *el;
list = hashElListHash(settings);
for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, " ");
    outputSetting(f, el->name, el->val);
    }
hashElFreeList(&list);
}

struct column *parseColumnLine(struct lineFile *lf, char *line)
/* Parse out column header line and return a column based on it. */
{
struct hash *settings;
struct column *col;
AllocVar(col);
settings = col->settings = hashVarLine(line, lf->lineIx);
addIfMissing(settings, "name", "custom");
addIfMissing(settings, "shortLabel", "User Column");
addIfMissing(settings, "longLabel", "User custom column");
addIfMissing(settings, "visibility", "on");
addIfMissing(settings, "priority", "2.01");
addIfMissing(settings, "org", genome);
addIfMissing(settings, "db", database);
hashAdd(settings, "type", cloneString("custom"));
columnVarsFromSettings(col, lf->fileName);
return col;
}

struct column *verifyCopyColumn(struct sqlConnection *conn, 
	struct lineFile *lf, FILE *output, char *line,
	struct column *builtInList, struct hash *uniqHash)
/* Parse out column from line file.  If output is non-NULL
 * then also save it to output. */
{
char *word, *name;
struct column *col = parseColumnLine(lf, line);
struct hash *lookupHash = NULL, *gpHash = newHash(16);
char *lookup = NULL;
int hitCount = 0, missCount = 0, badGeneCount = 0;
struct genePos *gpList, *gp;
char *oneMiss = NULL, *oneBadGene = NULL;

/* Build up hash & list of all known genes. */
gpList = knownPosAll(conn);
for (gp = gpList; gp != NULL; gp = gp->next)
    hashAdd(gpHash, gp->name, gp);

/* Deal with non-uniq names. */
if (hashLookup(uniqHash, col->name))
    {
    int ix = 1;
    char buf[256];
    for (;;)
	{
	safef(buf, sizeof(buf), "%s%d", col->name, ++ix);
	if (!hashLookup(uniqHash, buf))
	    {
	    col->name = cloneString(buf);
	    break;
	    }
	}
    hashRemove(col->settings, "name");
    hashAdd(col->settings, "name", col->name);
    }
hashAdd(uniqHash, col->name, col);

/* If there's a idLookup setting then make lookup hash. */
if ((lookup = hashFindVal(col->settings, "idLookup")) != NULL)
    {
    struct column *lookupCol = findNamedColumn(lookup);
    if (lookupCol == NULL)
        errAbort("Couldn't find column %s for idLookup line %d of %s",
		lookup, lf->lineIx, lf->fileName);
    lookupHash = hashNew(16);
    for (gp = gpList; gp != NULL; gp = gp->next)
        {
	char *cell = lookupCol->cellVal(lookupCol, gp, conn);
	if (cell != NULL)
	    {
	    hashAdd(lookupHash, cell, gp->name);
	    freez(&cell);
	    }
	}
    }

/* Allocate structure and fill in from header. */
fprintf(output, "column");
outputSettings(output, col->settings);
fprintf(output, "\n");

/* Make hash of lines. */
while (lineFileNext(lf, &line, NULL))
    {
    line = trimSpaces(line);
    if (line[0] == '#')
        continue;
    if (line[0] == 0)
        break;
    if (startsWith("column ", line))
        {
	lineFileReuse(lf);
	break;
	}
    name = word = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (line == NULL || line[0] == 0)
        {
	errAbort("Expecting at least two words in line %d of %s",
	    lf->lineIx, lf->fileName);
	}
    if (lookupHash != NULL)
	{
	name = hashFindVal(lookupHash, word);
	if (name == NULL)
	    {
	    ++missCount;
	    if (oneMiss == NULL)
	        oneMiss = cloneString(word);
	    continue;
	    }
	}
    else
        {
	if (!hashLookup(gpHash, name))
	    {
	    ++badGeneCount;
	    if (oneBadGene == NULL)
	        {
		oneBadGene = cloneString(name);
		continue;
		}
	    }
	}
    ++hitCount;
    fprintf(output, "%s %s\n", name, line);
    }
fprintf(output, "\n");
if (missCount > 0)
   {
   warn("%d items including '%s' not found (and %d found) in %s", 
   	missCount, oneMiss, hitCount, lookup);
   }
if (badGeneCount > 0)
   {
   warn("%d gene ID's including '%s' not found (and %d found)", 
   	badGeneCount, oneBadGene, hitCount);
   }
hashFree(&lookupHash);
return col;
}

static struct hash *hashColumns(struct column *colList)
/* Return a hash of columns keyed by name. */
{
struct column *col;
struct hash *hash = hashNew(9);
for (col = colList; col != NULL; col = col->next)
    hashAdd(hash, col->name, col);
return hash;
}

static struct column *verifyCopyColumns(struct sqlConnection *conn,
	struct lineFile *lf, FILE *output, 
	struct column *builtInList, struct hash *uniqHash)
/* Read in custom columns.  Verify that they are ok.  Copy 
 * version after lookup if any to output. */
{
char *line, *word;
struct column *colList = NULL;
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    if (sameString("column", word))
        {
	struct column *col = verifyCopyColumn(conn, lf, output, line, builtInList, uniqHash);
	if (col != NULL)
	    {
	    slAddHead(&colList, col);
	    }
	}
    else
        {
	errAbort("Unrecognized word '%s' line %d of %s", word, 
		lf->lineIx, lf->fileName);
	break;
	}
    }
return colList;
}

static void removeOldCustomColumns(struct column **pColList)
/* Remove any existing custom columns. */
{
struct column *list = NULL, *col, *next;
for (col = *pColList; col != NULL; col = next)
    {
    next = col->next;
    if (!startsWith("custom", col->type))
        {
	slAddHead(&list, col);
	}
    }
slReverse(&list);
*pColList = list;
}

static void customFromText(struct sqlConnection *conn, char *text, 
	struct column **pColList)
/* Convert custom column text to lineFile, and call custom column
 * updater. */
{
struct lineFile *lf = lineFileOnString("custom column", TRUE, text);
char *outFileName = newCustomFileName();
struct column *newList;
FILE *f;
struct hash *uniqHash = NULL;

/* Remove file name from cart for better error recovery. */
cartRemove(cart, customFileVarName);

/* Copy and verify output. */
f = mustOpen(outFileName, "w");
removeOldCustomColumns(pColList);
uniqHash = hashColumns(*pColList);
newList = verifyCopyColumns(conn, lf, f, *pColList, uniqHash);
*pColList = slCat(*pColList, newList);
lf->buf = NULL;		/* Don't let lineFileClose free this */
lineFileClose(&lf);
carefulClose(&f);

/* Put back file name in cart , looks like we must have succeeded. */
cartSetString(cart, customFileVarName, outFileName);
freez(&outFileName);
freeHash(&uniqHash);
}

static void customFromUrls(struct sqlConnection *conn, char *urlList, 
	struct column **pColList)
/* Grab custom columns from a list of URLs. */
{
char *outFileName = newCustomFileName();
struct column *customCols = NULL;
FILE *f;
char *url;
struct hash *uniqHash = NULL;

/* Remove file name from cart for better error recovery. */
cartRemove(cart, customFileVarName);

/* Copy and verify output. */
f = mustOpen(outFileName, "w");
removeOldCustomColumns(pColList);
uniqHash = hashColumns(*pColList);

/* Loop through and add columns from each URL. */
while ((url = nextWord(&urlList)) != NULL)
    {
    if (!(startsWith("http://" , url)
       || startsWith("https://", url)
       || startsWith("ftp://"  , url)))
	errAbort("Invalid url [%s]. URLs must start with http://, https://, or ftp://", url);
    struct lineFile *lf = netLineFileOpen(url);
    struct column *newList = verifyCopyColumns(conn, lf, f, *pColList, uniqHash);
    customCols = slCat(newList, customCols);
    lineFileClose(&lf);
    }

*pColList = slCat(customCols, *pColList);
carefulClose(&f);

/* Put back file name in cart , looks like we must have succeeded. */
cartSetString(cart, customFileVarName, outFileName);
freez(&outFileName);
hashFree(&uniqHash);
}

void doCustomSubmit(struct sqlConnection *conn, struct column *colList)
/* Put up page to submit custom columns. */
{
if (cartVarExists(cart, customUploadVarName))
    {
    customFromText(conn, cartString(cart, customUploadVarName), &colList);
    cartRemove(cart, customUploadVarName);
    }
else if (cartVarExists(cart, customPasteVarName))
    {
    customFromText(conn, cartString(cart, customPasteVarName), &colList);
    cartRemove(cart, customPasteVarName);
    }
else if (cartVarExists(cart, customFromUrlVarName))
    {
    customFromUrls(conn, cartString(cart, customFromUrlVarName), &colList);
    cartRemove(cart, customFromUrlVarName);
    }
else
    {
    warn("doCustomSubmit without recognized custom variable");
    }
slSort(&colList, columnCmpPriority);
doConfigure(conn, colList, NULL);
}

void doCustomClear(struct sqlConnection *conn, struct column *colList)
/* Put up page to clear custom columns. */
{
removeOldCustomColumns(&colList);
cartRemove(cart, customFileVarName);
doConfigure(conn, colList, NULL);
}

struct column *quickParseColumn(struct lineFile *lf, char *colLine)
/* Given column line, parse column until blank line with
 * minimal error checking. */
{
struct column *col = parseColumnLine(lf, colLine);
struct hash *idHash;
char *line, *word;
if (col == NULL)
    return NULL;
col->customIdHash = idHash = hashNew(16);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == 0)
        break;
    word = nextWord(&line);
    line = skipLeadingSpaces(line);
    hashAdd(idHash, word, lmCloneString(idHash->lm, line));
    }
return col;
}

struct column *quickParseColumns(char *fileName)
/* Parse column that's been validated and put into temp file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct column *colList = NULL;
char *line, *word;
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    if (sameString("column", word))
	{
	struct column *col = quickParseColumn(lf, line);
	if (col != NULL)
	    {
	    slAddHead(&colList, col);
	    }
	}
    else
	{
	warn("Unrecognized word '%s' line %d of %s", word, 
		lf->lineIx, lf->fileName);
	break;
	}
    }
lineFileClose(&lf);
return colList;
}

struct column *customColumnsRead(struct sqlConnection *conn, char *org, char *db)
/* Read in data for custom columns. */
{
char *fileName = customFileName();
struct column *colList = NULL;

if (fileName != NULL)
    colList = quickParseColumns(fileName);
return colList;
}

/* ---- Implement column methods for custom types ---- */

static boolean colSettingOn(struct column *col, char *name)
/* Return TRUE if setting exists and has value "on" */
{
char *val = hashFindVal(col->settings, name);
return val != NULL && sameString(val, "on");
}

static boolean customExists(struct column *col, struct sqlConnection *conn)
/* Custom column exists if we are in right database. */
{
char *db = hashFindVal(col->settings, "db");
return db == NULL || sameString(db, database);
}

char *customCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Return value of custom field. */
{
struct dyString *dy = NULL;
char *retVal = NULL;
struct hashEl *hel;
for (hel = hashLookup(col->customIdHash, gp->name); hel != NULL; hel = hashLookupNext(hel))
    {
    if (dy == NULL)
        dy = dyStringNew(256);
    else
        dyStringAppendC(dy, ',');
    dyStringAppend(dy, hel->val);
    }
if (dy != NULL)
    retVal = dyStringCannibalize(&dy);
return retVal;
}

static void customCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Return value of custom field. */
{
boolean gotAny = FALSE;
struct hashEl *hel;
hPrintf("<TD>");
for (hel = hashLookup(col->customIdHash, gp->name); hel != NULL; hel = hashLookupNext(hel))
    {
    if (gotAny)
	hPrintf(",&nbsp;");
    else
	gotAny = TRUE;
    if (col->itemUrl)
	{
	char *encoded = cgiEncode(hel->val);
        hPrintf("<A HREF=\"");
	hPrintf(col->itemUrl, encoded);
	hPrintf("\">");
	hPrintEncodedNonBreak(hel->val);
	hPrintf("</A>");
	freeMem(encoded);
	}
    else
	hPrintEncodedNonBreak(hel->val);
    }
if (!gotAny)
    {
    hPrintf("n/a");
    }
hPrintf("</TD>");
}

static void addSearchResult(struct searchResult **pList, struct hashEl *hel)
/* Add given name to search result. */
{
struct searchResult *res;
AllocVar(res);
res->gp.name = cloneString(hel->name);
res->matchingId = cloneString(hel->val);
slAddHead(pList, res);
}

struct searchResult *customSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Search lookup type column. */
{
struct searchResult *resList = NULL;
char *type = columnSetting(col, "search", "exact");
struct hashEl *helList, *hel;

helList = hashElListHash(col->customIdHash);
if (sameString(type, "prefix"))
    {
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	if (startsWith(search, hel->val))
	    addSearchResult(&resList, hel);
	}
    }
else if (sameString(type, "fuzzy"))
    {
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	if (stringIn(search, hel->val) != NULL)
	    addSearchResult(&resList, hel);
	}
    }
else 
    {
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	if (sameString(search, hel->val))
	    addSearchResult(&resList, hel);
	}
    }
hashElFreeList(&helList);
return resList;
}

static void customFilterControls(struct column *col, 
	struct sqlConnection *conn)
/* Print out controls for filter. */
{
if (colSettingOn(col, "isNumber"))
    minMaxAdvFilterControls(col, conn);
else
    lookupAdvFilterControls(col, conn);
}

static struct genePos *customFilterText(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced text filter on custom track.  This works much like 
 * lookup custom filter. */
{
char *wild = advFilterVal(col, "wild");
struct hash *keyHash = keyFileHash(col);
if (wild != NULL || keyHash != NULL)
    {
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct hash *hash = newHash(17);
    struct hashEl *hel, *helList = hashElListHash(col->customIdHash);
    struct slName *wildList = stringToSlNames(wild);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	if (keyHash == NULL || hashLookupUpperCase(keyHash, hel->val))
	    {
	    if (wildList == NULL || wildMatchList(hel->val, wildList, orLogic))
		hashAdd(hash, hel->name, NULL);
	    }
	}
    list = weedUnlessInHash(list, hash);
    hashElFreeList(&helList);
    hashFree(&hash);
    }
hashFree(&keyHash);
return list;
}

static struct genePos *customFilterNum(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced numerical filter on custom track.  This 
 * works much like floatAdvFilter. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
char *valString;
if (minString != NULL || maxString != NULL)
    {
    struct genePos *gp, *next, *newList = NULL;
    double minVal = 0, maxVal = 0, val;
    if (minString != NULL)
        minVal = atof(minString);
    if (maxString != NULL)
        maxVal = atof(maxString);
    for (gp = list; gp != NULL; gp = next)
        {
	next = gp->next;
	valString = hashFindVal(col->customIdHash, gp->name);
	if (valString != NULL)
	    {
	    val = atof(valString);
	    if (minString == NULL || val >= minVal)
	       if (maxString == NULL || val <= maxVal)
	           {
		   slAddHead(&newList, gp);
		   }
	    }
	}
    slReverse(&newList);
    list = newList;
    }
return list;
}

static struct genePos *customFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
{
if (colSettingOn(col, "isNumber"))
    return customFilterNum(col, conn, list); 
else
    return customFilterText(col, conn, list);
}

void setupColumnCustom(struct column *col, char *parameters)
/* Set up custom column. */
{
col->exists = customExists;
col->cellVal = customCellVal;
col->cellPrint = customCellPrint;
if (columnSetting(col, "search", NULL))
    col->simpleSearch = customSearch;
col->filterControls = customFilterControls;
col->advFilter = customFilter;
}
