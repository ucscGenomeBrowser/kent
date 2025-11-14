/* hgFind.c - Find things in human genome annotations. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "regexHelper.h"
#include "obscure.h"
#include "hCommon.h"
#include "portable.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "jksql.h"
#include "hdb.h"
#include "hui.h"
#include "psl.h"
#include "genePred.h"
#include "genePredReader.h"
#include "bed.h"
#include "cytoBand.h"
#include "cart.h"
#include "errCatch.h"
#include "hgFind.h"
#include "hgFindSpec.h"
#include "hgHgvs.h"
#include "snp.h"
#include "refLink.h"
#include "kgAlias.h"
#include "kgProtAlias.h"
#include "findKGAlias.h"
#include "findKGProtAlias.h"
#include "tigrCmrGene.h"
#include "minGeneInfo.h"
#include "pipeline.h"
#include "hgConfig.h"
#include "trix.h"
#include "trackHub.h"
#include "udc.h"
#include "hubConnect.h"
#include "bigBedFind.h"
#include "genbank.h"
#include "chromAlias.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "jsonParse.h"

// Exhaustive searches can lead to timeouts on CGIs (#11626).
// However, hgGetAnn requires exhaustive searches (#11665).
#define NONEXHAUSTIVE_SEARCH_LIMIT 500
#define EXHAUSTIVE_SEARCH_REQUIRED  -1
#define SNIPPET_LIMIT 100

char *hgAppName = "";

/* Caches used by hgFind.c */
struct hash *hgFindTrackHash = NULL;
struct trackDb *hgFindTdbList = NULL;

/* alignment tables to check when looking for mrna alignments */
static char *estTables[] = { "intronEst", "all_est", "xenoEst", NULL };
static char *estLabels[] = { "Spliced ESTs", "ESTs", "Other ESTs", NULL };
static char *mrnaTables[] = { "all_mrna", "xenoMrna", NULL };
static char *mrnaLabels[] = { "mRNAs", "Other mRNAs", NULL };
static struct dyString *hgpMatchNames = NULL;

void hgPosFree(struct hgPos **pEl)
/* Free up hgPos. */
{
struct hgPos *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    freeMem(el->description);
    freeMem(el->browserName);
    freez(pEl);
    }
}

static void hgPosFreeList(struct hgPos **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPos *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPosFree(&el);
    }
*pList = NULL;
}

static void hgPosTableFree(struct hgPosTable **pEl)
/* Free up hgPosTable. */
{
struct hgPosTable *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    hgPosFreeList(&el->posList);
    freez(pEl);
    }
}

static void hgPosTableFreeList(struct hgPosTable **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPosTable *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPosTableFree(&el);
    }
*pList = NULL;
}

void searchCategoryFree(struct searchCategory **el)
{
struct searchCategory *pEl = *el;
if (pEl != NULL)
    {
    freeMem(pEl->id);
    freeMem(pEl->name);
    freeMem(pEl->searchString);
    freeMem(pEl->label);
    freeMem(pEl->description);
    freeMem(pEl->groupName);
    trixClose(&pEl->trix);
    slNameFreeList(pEl->parents);
    slNameFreeList(pEl->errors);
    }
}


#define HGPOSRANGESIZE 64
static char *hgPosBrowserRange(struct hgPos *pos, char range[HGPOSRANGESIZE])
/* Convert pos to chrN:123-456 format.  If range parameter is NULL it returns
 * static buffer, otherwise writes and returns range. */
{
static char buf[HGPOSRANGESIZE];

if (range == NULL)
    range = buf;
safef(range, HGPOSRANGESIZE, "%s:%d-%d",
      pos->chrom, pos->chromStart+1, pos->chromEnd);
return range;
}


#if 0 /* not used */
static char *getGrepIndexFile(struct hgFindSpec *hfs)
/* Return grepIndex setting (may be relative to hg.conf grepIndex.default),
 * or NULL if the file doesn't exist. */
{
char *indexFile = hgFindSpecSetting(hfs, "grepIndex");
if (indexFile == NULL)
    return NULL;
else if (fileExists(indexFile))
    return cloneString(indexFile);
else if (! startsWith("/", indexFile))
    {
    char *grepIndexRoot = cfgOption("grepIndex.default");
    if (grepIndexRoot != NULL)
	{
	char absPath[1024];
	safef(absPath, sizeof(absPath), "%s/%s/%s",
	      grepIndexRoot, hGetDb(), indexFile);
	if (fileExists(absPath))
	    return cloneString(absPath);
	}
    }
return NULL;
}
#endif

#define HGFIND_MAX_KEYWORDS 16
#define HGFIND_MAX_CMDWORDS 6

static void makeCmds(char **cmds[HGFIND_MAX_KEYWORDS+1],
		     char **keyWords, int keyCount, char *extraOptions)
/* Fill in cmds, an array of command word arrays. */
{
int i;
for (i=0;  i < keyCount;  i++)
    {
    char **cmd = NULL;
    int j = 0;
    AllocArray(cmd, HGFIND_MAX_CMDWORDS);
    cmd[j++] = "fgrep";
    cmd[j++] = "-i";
    if (isNotEmpty(extraOptions))
	cmd[j++] = extraOptions;
    cmd[j++] = keyWords[i];
    cmd[j++] = NULL;
    if (j > HGFIND_MAX_CMDWORDS)
	errAbort("overflow error -- increase HGFIND_MAX_CMDWORDS.");
    cmds[i] = cmd;
    }
cmds[i] = NULL;
}

static void freeCmds(char **cmds[], int keyCount)
/* Free each element of cmds. */
{
int i;
for (i=0;  i < keyCount;  i++)
    {
    freez(&(cmds[i]));
    }
}

static boolean keyIsPrefix(char *key, char *text)
/* Return TRUE only if key is at the start of some word in text. 
 * For short keys (2 or less) it must be whole word. */
{
char *s = text;
int keyLen = strlen(key);
while ((s = stringIn(key, s)) != NULL)
    {
    if (s == text || !isalnum(s[-1]))
	{
	if (keyLen > 2 || !isalnum(s[keyLen]))
	    return TRUE;
	}
    s += 1;
    }
return FALSE;
}

static boolean keyIsPrefixIgnoreCase(char *key, char *text)
/* Case insensitive keyIsPrefix */
{
boolean isPrefix;
key = cloneString(key);
touppers(key);
text = cloneString(text);
touppers(text);
isPrefix = keyIsPrefix(key, text);
freeMem(key);
freeMem(text);
return isPrefix;
}

static boolean allKeysPrefix(char **keys, int keyCount, char *text)
/* Make sure that all keys in text are proper prefixes of a word. */
/* NOTE: this is case sensitive.  To ignore case, caller must ensure that 
 * all keys and text have been forced to the same case. */
{
int i;
for (i=0; i<keyCount; ++i)
    {
    if (!keyIsPrefix(keys[i], text))
        return FALSE;
    }
return TRUE;
}

static char *commonDescriptionWords[] = {
/* List of words that are too common in mRNA descriptions to be useful.
 * Every year or two we might want to update this. */
  "LIKE",
  "HUMAN",
  "FACTOR",
  "ISOFORM",
  "GENE",
  "FROM",
  "HYPOTHETICAL",
  "FAMILY",
  "UNKNOWN",
  "PA",
  "MEMBER",
  "SIMILAR",
  "TO",
  "REGION",
  "MUS",
  "MUSCULUS",
  "MELANOGASTER",
  "CAENORHABDITIS",
  "CHAIN",
  "ELEGANS",
  "DROSOPHILA",
  "COT",
  "NORMALIZED",
  "TRANSCRIPT",
  "FOR",
  "VARIANT",
  "FIS",
  "THALIANA",
  "ARABIDOPSIS",
  "PARTIAL",
  "LENGTH",
  "HUMAN",
  "FULL",
  "IMAGE",
  "PROTEIN",
  "OF",
  "COMPLETE",
  "CDNA",
  "CLONE",
  "CDS",
  "HOMO",
  "SAPIENS",
  "MRNA",
  "5",
  "4",
  "3",
  "2",
  "1",
};

static char *commonProductNameWords[] = {
/* List of words that are too common in productNames to be useful.
 * Every year or two we might want to update this. */
  "MEMBER",
  "FAMILY",
  "UNKNOWN",
  "FOR",
  "PUTATIVE",
  "LIKE",
  "5",
  "4",
  "3",
  "2",
  "1",
  "HYPOTHETICAl",
  "ISOFORM",
 "PROTEIN",
};

static boolean anyStartWith(char *prefix, char *words[], int wordCount)
/* Return TRUE if any words start with prefix. */
{
int i;
for (i=0; i<wordCount; ++i)
    {
    if (startsWith(prefix, words[i]))
        return TRUE;
    }
return FALSE;
}

static boolean isTooCommon(char *table, char *key)
/* Return TRUE if key is too common to be useful in table */
{
if (sameString(table, "description"))
    return anyStartWith(key, commonDescriptionWords, 
    	ArraySize(commonDescriptionWords));
else if (sameString(table, "productName"))
    return anyStartWith(key, commonProductNameWords, 
    	ArraySize(commonProductNameWords));
else if (sameString(table, "author"))
    return sameWord(key, "and");
else
    return FALSE;
}

static int removeTooCommon(char *table, char **keys, int keyCount)
/* Remove keys that are too common to be meaningful. */
{
int readIx, writeIx=0;
for (readIx=0; readIx < keyCount; ++readIx)
    {
    if (!isTooCommon(table, keys[readIx]))
        {
	keys[writeIx] = keys[readIx];
	writeIx += 1;
	}
    }
return writeIx;
}


static struct slName *doGrepQuery(char *indexFile, char *table, char *key,
				  char *extraOptions)
/* grep -i key indexFile, return a list of ids (first word of each line). */
{
struct pipeline *pl = NULL;
struct slName *idList = NULL;
struct lineFile *lf = NULL;
char *id, *rest, *line;
char *keyWords[HGFIND_MAX_KEYWORDS];
char **cmds[HGFIND_MAX_KEYWORDS+1];
/* escape special chars here */
char *escapedKey = sqlEscapeString(key); /* presumably this is the right way escape it? -Galt*/ 
int keyCount;

touppers(escapedKey);
keyCount = chopLine(escapedKey, keyWords);
keyCount = removeTooCommon(table, keyWords, keyCount);
if (keyCount > 0)
    {
    if (extraOptions == NULL)
	extraOptions = "";
    makeCmds(cmds, keyWords, keyCount, extraOptions);

    pl = pipelineOpen(cmds, pipelineRead | pipelineNoAbort, indexFile, NULL, 0);
    lf = pipelineLineFile(pl);
    verbose(3, "\n***Running this fgrep command with pipeline from %s:\n*** %s\n\n",
	    indexFile, pipelineDesc(pl));
    while (lineFileNextReal(lf, &line))
	{
	id = nextWord(&line);
	rest = skipLeadingSpaces(line);
	touppers(rest);
	if (allKeysPrefix(keyWords, keyCount, rest))
	    {
	    struct slName *idEl = slNameNew(id);
	    slAddHead(&idList, idEl);
	    }
	}
    pipelineClose(&pl);  /* Takes care of lf too. */
    freeCmds(cmds, keyCount);
    if (verboseLevel() >= 3)
	{
	int count = slCount(idList);
	verbose(3, "*** Got %d results from %s\n\n", count, indexFile);
	}
    }
freeMem(escapedKey);
return idList;
}

static struct hgPosTable *findTable(struct hgPosTable *list, char *name)
/* Find first table in list that matches name */
{
struct hgPosTable *ret = NULL;
for (ret = list; ret != NULL; ret = ret->next)
    {
    if (sameString(ret->name, name))
        return ret;
    }
return NULL;
}

static struct hgPosTable *addKnownGeneTable(char *db, struct hgPositions *hgp, char *name)
/* Create new table for known genes matches, add it to hgp, and return it. */
{
// we may be coming here a second time, after already hitting knownGeneFast
// add non duplicate results to the end of our hgp
struct hgPosTable *table;
table = findTable(hgp->tableList, name);
if (table == NULL)
    {
    AllocVar(table);
    table->searchTime = -1;
    if (differentString(name, "knownGene"))
        {
        char *masterGeneTrack = hdbGetMasterGeneTrack(name);

        table->description = cloneString(masterGeneTrack);
        table->name = cloneString(masterGeneTrack);
        }
    else
        {
        if (hTableExists(db, "knownAttrs"))
            table->description = cloneString("Gencode Genes");
        else if (hTableExists(db, "kgProtMap2"))
            table->description = cloneString("UCSC Genes");
        else
            table->description = cloneString("Known Genes");
        table->name = cloneString("knownGene");
        }
    slAddHead(&hgp->tableList, table);
    }
return table;
}

static char *makeIndexPath(char *db, char *name)
{
/* create the pathname with the knowngene index for a db, result needs to be freed */
char *path = needMem(PATH_LEN);
safef(path, PATH_LEN, "/gbdb/%s/%s.ix", db, name);
char *newPath = hReplaceGbdb(path);
freez(&path);
return newPath;
}

static boolean gotFullText(char *db, char *indexPath)
/* Return TRUE if we have full text index. */
{
boolean result = FALSE;

if (udcExists(indexPath))
    result = TRUE;
else
    {
    warn("%s doesn't exist", indexPath);
    result = FALSE;
    }

return result;
}

struct tsrPos
/* Little helper structure tying together search result
 * and pos, used by addKnownGeneItems */
    {
    struct tsrPos *next;	/* Next in list. */
    struct trixSearchResult *tsr;	/* Basically a gene symbol */
    struct hgPos *posList;		/* Associated list of positions. */
    };

static int hgPosCmpCanonical(const void *vhg1, const void *vhg2)
// Compares two hgPos structs and returns an integer
{
const struct hgPos *hg1 = *((struct hgPos**)vhg1);
const struct hgPos *hg2 = *((struct hgPos**)vhg2);
int diff = trixSearchResultCmp(&hg1->tp->tsr, &hg2->tp->tsr);
if (diff == 0)
    {
    diff = (hg2->canonical - hg1->canonical);
    if (diff == 0)
        {
	// Prioritize things on main chromosomes
	diff = chrNameCmpWithAltRandom(hg1->chrom, hg2->chrom);
	}
    }
return diff;
}

static void addKnownGeneItems(struct hgPosTable *table,
	struct trixSearchResult *tsrList, struct sqlConnection *conn, char *name, struct trix *trix, struct hgFindSpec *hfs)
/* Convert tsrList to posList, and hang posList off of table. */
{
struct dyString *dy = dyStringNew(0);
struct trixSearchResult *tsr;
struct hash *hash = hashNew(16);
struct hgPos *pos, *posList = NULL;
struct tsrPos *tpList = NULL, *tp;
struct sqlResult *sr;
char **row;
int maxToReturn = NONEXHAUSTIVE_SEARCH_LIMIT;
char *db = sqlGetDatabase(conn);
char *dbName;

if (sameString(name, "knownGene"))
    dbName = db;
else
    dbName = name;

if (slCount(tsrList) > maxToReturn)
    {
    //warn("Search terms are not very specific, only showing first %d matching UCSC Genes.",
   // 	maxToReturn);
    tsr = slElementFromIx(tsrList, maxToReturn-1);
    tsr->next = NULL;
    }
// allow supporting snippet file to not exist, if there are no
// snippets then the below code will use the description from
// kgXref
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *context = hgFindSpecSetting(hfs, "searchTrixContext");
    if (context && sameString(context, "on"))
        addSnippetsToSearchResults(tsrList, trix);
    }
errCatchEnd(errCatch);

/* Make hash of all search results - one for each known gene ID. */
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    lmAllocVar(hash->lm, tp);
    tp->tsr = tsr;
    slAddHead(&tpList, tp);
    hashAdd(hash, tsr->itemId, tp);
    }

/* Stream through knownGenes table and make up a pos
 * for each mapping of each gene matching search. */
sqlDyStringPrintf(dy,
	"select kg.name,kg.chrom,kg.txStart,kg.txEnd,geneSymbol,description,kc.transcript from %s.knownGene kg "
        "join %s.kgXref on kg.name = %s.kgXref.kgID "
        "left join %s.knownCanonical kc on "
            "kc.transcript = kg.name and kc.chrom=kg.chrom and kc.chromStart = kg.txStart "
        "where name in (", dbName, dbName, dbName, dbName);
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    sqlDyStringPrintf(dy, "'%s'", tsr->itemId);
    if (tsr->next != NULL)
        sqlDyStringPrintf(dy, ",");
    }
sqlDyStringPrintf(dy, ")");

sr = sqlGetResult(conn, dy->string);

while ((row = sqlNextRow(sr)) != NULL)
    {
    tp = hashFindVal(hash, row[0]);
    char nameBuf[256];
    if (tp == NULL)
        internalErr();
    else
        {
        AllocVar(pos);
        pos->chrom = cloneString(row[1]);
        pos->chromStart = sqlUnsigned(row[2]);
        pos->chromEnd = sqlUnsigned(row[3]);
        pos->tp = tp;
        slAddHead(&tp->posList, pos);
        safef(nameBuf, sizeof(nameBuf), "%s (%s)", row[4], row[0]);
        pos->name = cloneString(nameBuf);
        pos->browserName = cloneString(row[0]);
        if (tp->tsr->snippet)
            if (!sameString(hgAppName, "hgSearch") && !sameString(hgAppName, "cartJson"))
                pos->description = htmlTextStripTags(tp->tsr->snippet);
            else
                pos->description = tp->tsr->snippet;
        else
            pos->description = cloneString(row[5]);
        pos->canonical = row[6] != NULL;
       }
    }
sqlFreeResult(&sr);

/* Hang all pos onto table. */
for (tp = tpList; tp != NULL; tp = tp->next)
    {
    struct hgPos *next;
    for (pos = tp->posList; pos != NULL; pos = next)
        {
	next = pos->next;
	slAddHead(&posList, pos);
	}
    }

slSort(&posList, hgPosCmpCanonical);
// we may have already been here (ex: queried knownGeneFast first), if so,
// we need to put the new list of results behind the old list, since the
// old results had a higher priority. We can now rank results to knownGene
// by putting what we want users to find first in different search specs
if (table->posList == NULL)
    table->posList = posList;
else
    {
    struct hash *prevHash = hashNew(0);
    struct hgPos *newPosList = NULL, *next;
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
        hashAdd(prevHash, pos->name, pos);
        }
    for (pos = posList; pos != NULL; pos = next)
        {
        next = pos->next;
        if (!hashLookup(prevHash, pos->name))
            slAddHead(&newPosList, pos);
        }
    slReverse(&newPosList);
    table->posList = slCat(table->posList, newPosList);
    }

hashFree(&hash);
dyStringFree(&dy);
}

static boolean findKnownGeneFullText(char *db, char *term,struct hgPositions *hgp, char *name, char *path, struct hgFindSpec *hfs, boolean measureTiming)
/* Look for position in full text. */
{
long startTime = clock1000();
boolean gotIt = FALSE;
struct trix *trix;
struct trixSearchResult *tsrList;
char *lowered = cloneString(term);
char *keyWords[HGFIND_MAX_KEYWORDS];
int keyCount;
struct hgPosTable *table = NULL;

trix = trixOpen(path);
tolowers(lowered);
keyCount = chopLine(lowered, keyWords);
tsrList = trixSearch(trix, keyCount, keyWords, tsmExpand);
if (tsrList != NULL)
    {
    table = addKnownGeneTable(db, hgp, name);
    struct sqlConnection *conn = hAllocConn(db);
    addKnownGeneItems(table, tsrList, conn, name, trix, hfs);
    hFreeConn(&conn);
    gotIt = TRUE;
    }
freez(&lowered);
trixSearchResultFreeList(&tsrList);
trixClose(&trix);
// This is hacky but rely on knownGene table being at head of list
// for timing. TODO: make this more robust
if (measureTiming && table != NULL)
    {
    if (table->searchTime == -1)
        table->searchTime = clock1000() - startTime;
    else
        table->searchTime += clock1000() - startTime;
    }
return gotIt;
}

static char *getUiUrl(struct cart *cart)
/* Get rest of UI from browser. */
{
static struct dyString *dy = NULL;
static char *s = NULL;
if (dy == NULL)
    {
    dy = dyStringNew(64);
    if (cart != NULL && cart->sessionId != NULL)
	dyStringPrintf(dy, "%s=%s", cartSessionVarName(), cartSessionId(cart));
    s = dy->string;
    }
return s;
}


static void singlePos(struct hgPositions *hgp, char *tableDescription,
                      char *posDescription, char *tableName, char *posName,
                      char *browserName, char *chrom, int start, int end)
/* Fill in pos for simple case single position. */
{
struct hgPosTable *table;
struct hgPos *pos;

AllocVar(table);
AllocVar(pos);

slAddHead(&hgp->tableList, table);
table->posList = pos;
table->description = cloneString(tableDescription);
table->name = cloneString(tableName);
pos->chrom = chrom;
pos->chromStart = start;
pos->chromEnd = end;
pos->name = cloneString(posName);
pos->description = cloneString(posDescription);
pos->browserName = cloneString(browserName);
}

static void fixSinglePos(struct hgPositions *hgp)
/* Fill in posCount and if proper singlePos fields of hgp
 * by going through tables... */
{
int posCount = 0;
struct hgPosTable *table;
struct hgPos *pos;

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
	++posCount;
	if (pos->chrom != NULL)
	    hgp->singlePos = pos;
	}
    }
if (posCount != 1)
   hgp->singlePos = NULL;
hgp->posCount = posCount;
}

INLINE boolean setStartEndFromQuery(struct sqlConnection *conn, char *query,
                                    int *retStart, int *retEnd)
/* Run query (which must have start and end as first two output columns)
 * and collect min start and max end from resulting rows.  Return FALSE if no rows. */
{
boolean foundIt = FALSE;
int minStart = BIGNUM;
int maxEnd = 0;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int start = sqlUnsigned(row[0]);
    int end = sqlUnsigned(row[1]);
    if (start < minStart)
        minStart = start;
    if (end > maxEnd)
        maxEnd = end;
    foundIt = TRUE;
    }
sqlFreeResult(&sr);
if (foundIt)
    {
    if (retStart != NULL)
        *retStart = minStart;
    if (retEnd != NULL)
        *retEnd = maxEnd;
    }
return foundIt;
}


static boolean hgFindChromBand(char *db, char *chrom, char *band, int *retStart, int *retEnd)
/* Return start/end of band in chromosome. */
{
struct sqlConnection *conn = hAllocConn(db);
struct dyString *query = sqlDyStringCreate("select chromStart, chromEnd from cytoBand "
                                           "where chrom = '%s' and name = '%s'",
                                           chrom, band);
boolean foundIt = setStartEndFromQuery(conn, query->string, retStart, retEnd);
if (! foundIt)
    {
    // No exact match -- if band has a '.', chop at the '.' in case we only have more coarse data.
    // Otherwise try prefix search.
    dyStringClear(query);
    int len = strlen(band);
    char truncBand[len+1];
    safecpy(truncBand, sizeof(truncBand), band);
    char *dot = strchr(truncBand, '.');
    if (dot)
        {
        *dot = 0;
        sqlDyStringPrintf(query, "select chromStart, chromEnd from cytoBand "
                       "where chrom = '%s' and name = '%s'",
                       chrom, truncBand);
        }
    else
        {
        sqlDyStringPrintf(query, "select chromStart, chromEnd from cytoBand "
                       "where chrom = '%s' and name like '%s%%'", chrom, band);
        }
    foundIt = setStartEndFromQuery(conn, query->string, retStart, retEnd);
    }
hFreeConn(&conn);
dyStringFree(&query);
return foundIt;
}

boolean hgParseCytoBandName(char *db, char *spec, char **retChromName, char **retBandName)
/* Return TRUE if spec is a cytological band name including chromosome short 
 * name. Returns chromosome chrN name and band (with chromosome stripped off) */
{
regmatch_t substrArr[5];
// See if spec looks like a "chr"-less chromosome followed by a p or q, then a number,
// and possibly a '.' and another number.
// Mouse bands may have a letter A-H before the number, and may have no number.
// Horse bands may have "pq".
if (regexMatchSubstrNoCase(spec, "^(X|Y|[0-9]+)([pq]+[A-H]?([0-9]+(\\.[0-9]+)?)?)$",
                           substrArr, ArraySize(substrArr)))
    {
    char chrSpec[PATH_LEN];
    safencpy(chrSpec, sizeof(chrSpec), "chr", 3);
    safencpy(chrSpec+3, sizeof(chrSpec)-3, spec, substrArr[1].rm_eo);
    char *chromName = hgOfficialChromName(db, chrSpec);
    if (chromName)
        {
        if (retChromName)
            *retChromName = chromName;
        if (retBandName)
            *retBandName = cloneString(spec + substrArr[2].rm_so);
        return TRUE;
        }
    }
return FALSE;
}

boolean hgFindCytoBand(char *db, char *spec, char **retChromName, int *retWinStart,
		       int *retWinEnd)
/* Return position associated with cytological band if spec looks to be 
 * in that form. */
{
char *bandName;

if (!hgParseCytoBandName(db, spec, retChromName, &bandName))
     return FALSE;
return hgFindChromBand(db, *retChromName, bandName, retWinStart, retWinEnd);
}

boolean findChromContigPos(char *db, char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Look in all chroms.
 * Don't alter return variables unless found. */
/* NOTE: could probably speed this up by using the chromInfo hashtable */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row;
char query[256];
boolean foundIt = FALSE;

/* In case this is a scaffold-based assembly, check for unsplit table first: */
if (sqlTableExists(conn, "gold"))
    {
    sqlSafef(query, sizeof(query),
	  "select chrom,chromStart,chromEnd from gold where frag = '%s'",
	  name);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	*retChromName = cloneString(row[0]);
	*retWinStart = atoi(row[1]);
	*retWinEnd = atoi(row[2]);
	foundIt = TRUE;
	}
    sqlFreeResult(&sr);
    }
else
    {
    struct slName *allChroms = hAllChromNames(db);
    struct slName *chromPtr;

    for (chromPtr=allChroms;  chromPtr != NULL;  chromPtr=chromPtr->next)
	{
	char tableName[256];
	safef(tableName, sizeof(tableName), "%s_gold", chromPtr->name);
	if (! sqlTableExists(conn, tableName))
	    continue;
	sqlSafef(query, sizeof(query), 
	      "select chromStart,chromEnd from %s where frag = '%s'",
	      tableName, name);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    *retChromName = cloneString(chromPtr->name);
	    *retWinStart = atoi(row[0]);
	    *retWinEnd = atoi(row[1]);
	    foundIt = TRUE;
	    }
	sqlFreeResult(&sr);
	if (foundIt)
	    break;
	}
    slNameFreeList(&allChroms);
    }
hFreeConn(&conn);
return foundIt;
}

#if 0 /* not used */
static boolean isAccForm(char *s)
/* Returns TRUE if s is of format to be a genbank accession. */
{
int len = strlen(s);
if (len < 6 || len > 10)
    return FALSE;
if (!isalpha(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}
#endif

static boolean mrnaInfo(char *acc, struct sqlConnection *conn, 
                                char **mrnaType)
/* Sets *mrnaType to mrna/est type for the accession */
/* Ignores returned values if parameters are NULL */
/* Return TRUE if search succeeded, else FALSE */
/* NOTE: caller must free mrnaType */
{
char query[256];
struct sqlResult *sr;
char **row;
int ret;

sqlSafef(query, sizeof(query),
      "select type from %s where acc = '%s'", gbCdnaInfoTable, acc);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (mrnaType != NULL)
        *mrnaType = cloneString(row[0]);
    ret = TRUE;
    }
else
    ret = FALSE;
sqlFreeResult(&sr);
return ret;
}

boolean isRefSeqAcc(char *acc)
/* Return TRUE if acc looks like a RefSeq acc. */
{
return regexMatchNoCase(acc, "^(N|X)M_[0-9]{6}[0-9]*$");
}

static char *mrnaType(char *db, char *acc)
/* Return "mrna" or "est" if acc is mRNA, otherwise NULL.  Returns
 * NULL for refseq mRNAs */
/* for compat with older databases, just look at the seqId to
 * determine if it's a refseq, don't use table */
/* NOTE: caller must free returned type */
{
struct sqlConnection *conn;
char *type = NULL;
char *ret = NULL;

if (isRefSeqAcc(acc))
    return NULL;
conn = hAllocConn(db);
if (mrnaInfo(acc, conn, &type))
   ret = type;
else
   ret = NULL;
hFreeConn(&conn);
return ret;
}

static void mrnaHtmlStart(struct hgPosTable *table, FILE *f)
/* Print preamble to mrna alignment positions. */
{
fprintf(f, "<H2>%s</H2>", table->description);
fprintf(f, "This aligns in multiple positions.  Click on a hyperlink to ");
fprintf(f, "go to tracks display at a particular alignment.<BR>");

fprintf(f, "<PRE>");
fprintf(f, " SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
fprintf(f, "------------------------------------------------------------------------------\n");
}

static void mrnaHtmlEnd(struct hgPosTable *table, FILE *f)
/* Print end to mrna alignment positions. */
{
fprintf(f, "</PRE>");
}

static void mrnaHtmlOnePos(struct hgPosTable *table, struct hgPos *pos, FILE *f)
/* Print one mrna alignment position. */
{
fprintf(f, "%s", pos->description);
}

char *hCarefulTrackOpenVisCart(struct cart *cart, char *db, char *trackName)
/* If track is already in full mode, return full; otherwise, return
 * hTrackOpenVis. */
{
char *vis = cart ? cartOptionalString(cart, trackName) : NULL;
if (vis && sameString(vis, "full"))
    return "full";
else
    return hTrackOpenVis(db, trackName);
}

static struct psl *getPslFromTable(struct sqlConnection *conn, char *db, char *table, char *acc)
/* If table exists, return PSL for each row with qName = acc. */
{
struct psl *pslList = NULL;
if (sqlTableExists(conn, table))
    {
    int rowOffset = hOffsetPastBin(db, NULL, table);
    char query[256];
    sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'", table, acc);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct psl *psl = pslLoad(row+rowOffset);
	slAddHead(&pslList, psl);
	}
    slReverse(&pslList);
    sqlFreeResult(&sr);
    }
return pslList;
}

static void addPslResultToHgp(struct cart *cart, struct hgPositions *hgp, char *db, char *tableName,
			      char *shortLabel, char *acc, struct psl *pslList, boolean measureTiming)

/* Create an hgPosTable for the given psl search results, and add it to hgp->tableList. */
{
if (pslList == NULL)
    return;
long startTime = clock1000();
struct hgPosTable *table;
struct dyString *dy = dyStringNew(1024);
struct psl *psl;
char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';
char *ui = getUiUrl(cart);
AllocVar(table);
table->searchTime = -1;
table->htmlStart = mrnaHtmlStart;
table->htmlEnd = mrnaHtmlEnd;
table->htmlOnePos = mrnaHtmlOnePos;
slAddHead(&hgp->tableList, table);
dyStringPrintf(dy, "%s Alignments in %s", acc, shortLabel);
table->description = cloneString(dy->string);
table->name = cloneString(tableName);
char *trackName = hGetTrackForTable(db, table->name);
slSort(&pslList, pslCmpScore);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    struct hgPos *pos;
    dyStringClear(dy);
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(db, psl->tName);
    pos->chromStart = psl->tStart;
    pos->chromEnd = psl->tEnd;
    pos->name = cloneString(psl->qName);
    pos->browserName = cloneString(psl->qName);
    dyStringPrintf(dy, "<A HREF=\"%s%cposition=%s&%s=%s",
		   hgAppName, hgAppCombiner, hgPosBrowserRange(pos, NULL),
		   trackName, hCarefulTrackOpenVisCart(cart, db, trackName));
    if (ui != NULL)
	dyStringPrintf(dy, "&%s", ui);
    dyStringPrintf(dy, "%s\">", hgp->extraCgi);
    dyStringPrintf(dy, "%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
		   psl->match + psl->misMatch + psl->repMatch + psl->nCount,
		   100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		   skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
		   psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
    dyStringPrintf(dy, "\n");
    pos->description = cloneString(dy->string);
    slAddHead(&table->posList, pos);
    }
slReverse(&table->posList);
if (measureTiming)
    table->searchTime = clock1000() - startTime;
dyStringFree(&dy);
}

static boolean findMrnaPos(struct cart *cart, char *db, char *acc,  struct hgPositions *hgp, boolean measureTiming)
/* Find MRNA or EST position(s) from accession number.
 * Look to see if it's an mRNA or EST.  Fill in hgp and return
 * TRUE if it is, otherwise return FALSE. */
/* NOTE: this excludes RefSeq mrna's, as they are currently
 * handled in findRefGenes(), which is called later in the main function */
{
struct sqlConnection *conn = hAllocConn(db);
if (!sqlTableExists(conn, gbCdnaInfoTable))
    {
    hFreeConn(&conn);
    return FALSE;
    }
char *type = mrnaType(db, acc); 
if (isEmpty(type))
    {
    hFreeConn(&conn);
    /* this excludes refseq mrna's, and accessions with
     * invalid column type in mrna table (refseq's and ests) */
    return FALSE;
    }
char lowerType[16];
char **tables, **labels, *tableName;
boolean gotResults = FALSE;

safecpy(lowerType, sizeof(lowerType), type);
tolowers(lowerType);
if (sameWord(lowerType, "mrna"))
    {
    tables = mrnaTables;
    labels = mrnaLabels;
    }
else if (sameWord(lowerType, "est"))
    {
    tables = estTables;
    labels = estLabels;
    }
else
    {
    hFreeConn(&conn);
    return FALSE;
    }

while ((tableName = *tables++) != NULL)
    {
    char *label = *labels++;
    struct psl *pslList = NULL;
    if (sameString(tableName, "intronEst") && !sqlTableExists(conn, tableName))
	{
	struct slName *c, *chromList = hChromList(db);
	char splitTable[HDB_MAX_TABLE_STRING];
	for (c = chromList;  c != NULL;  c = c->next)
	    {
	    safef(splitTable, sizeof(splitTable), "%s_%s", c->name, tableName);
	    struct psl *chrPslList = getPslFromTable(conn, db, splitTable, acc);
	    if (pslList == NULL)
		pslList = chrPslList;
	    else
		slCat(pslList, chrPslList);
	    }
	}
    else
	pslList = getPslFromTable(conn, db, tableName, acc);
    if (pslList == NULL)
	continue;
    gotResults = TRUE;
    addPslResultToHgp(cart, hgp, db, tableName, label, acc, pslList, measureTiming);
    if (!sameString(tableName, "intronEst"))
	/* for speed -- found proper table, so don't need to look farther */
	break;
    }
hFreeConn(&conn);
return gotResults;
}

static char *getGenbankGrepIndex(char *db, struct hgFindSpec *hfs, char *table,
				 char *suffix)
/* If hg.conf has a grepIndex.genbank setting, hfs has a (placeholder)
 * grepIndex setting, and we can access the index file for table, then
 * return the filename; else return NULL. */
/* Special case for genbank: Mark completely specifies the root in
 * hg.conf, so hfs's grepIndex setting value is ignored -- it is used
 * only to enable grep indexing.  So we have multiple ways to turn this 
 * off if necessary: remove hg.conf setting (takes out all dbs), 
 * remove hgFindSpec setting (takes out one db at a time), or remove 
 * a file (takes out one table at a time). */
{
char *grepIndexRoot = cfgOption("grepIndex.genbank");
char *hfsSetting = hgFindSpecSetting(hfs, "grepIndex");

if (grepIndexRoot != NULL && hfsSetting != NULL)
    {
    char buf[1024];
    char *dot;
    // check to see if table name has database in it
    if ((dot = strchr(table, '.')) != NULL)
        {
        *dot = 0;
        db = table;
        table = dot + 1;
        }
    safef(buf, sizeof(buf), "%s/%s/%s.%s",
	  grepIndexRoot, db, table, suffix);
    if (dot)
        *dot = '.';
    if (fileExists(buf))
	return cloneString(buf);
    }
return NULL;
}


static struct slName *genbankGrepQuery(char *indexFile, char *table, char *key)
/* grep -i key indexFile, return a list of ids (first word of each line). */
{
char *extraOptions = "";
if (sameString(table, "author"))
    extraOptions = "-w";
return doGrepQuery(indexFile, table, key, extraOptions);
}

static struct slName *genbankSqlFuzzyQuery(struct sqlConnection *conn,
					   char *table, char *key, int limitResults)
/* Perform a fuzzy sql search for %key% in table.name; return list of 
 * corresponding table.id's.  */
{
struct slName *idList = NULL, *idEl = NULL;
if (!isTooCommon(table, key))
    {
    struct sqlResult *sr;
    char **row;
    char query[256];
    if (limitResults == EXHAUSTIVE_SEARCH_REQUIRED)
        sqlSafef(query, sizeof(query),
              "select id,name from %s where name like '%%%s%%'", table, key);
    else // limit results to avoid CGI timeouts (#11626).
        sqlSafef(query, sizeof(query),
              "select id,name from %s where name like '%%%s%%' limit %d", table, key, limitResults);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	touppers(row[1]);
	if (keyIsPrefix(key, row[1]))
	    {
	    idEl = newSlName(row[0]);
	    slAddHead(&idList, idEl);
	    }
	}
    sqlFreeResult(&sr);
    }
return idList;
}

static char *skipDb(char *tableName)
/* retun a pointer past the datbase part of the table name (if any) */
{
char *dot = tableName;

if ((dot = strchr(tableName, '.')) == NULL)
    return tableName;

return dot + 1;
}

static boolean gotAllGenbankGrepIndexFiles(char *db, struct hgFindSpec *hfs,
					   char *tables[], int tableCount)
/* Return TRUE if all tables have a readable genbank index file. */
{
int i;
for (i=0;  i < tableCount;  i++)
    if (! getGenbankGrepIndex(db, hfs, tables[i], "idName"))
	return FALSE;
return TRUE;;
}

static void findHitsToTables(char *db, struct hgFindSpec *hfs,
			     char *key, int limitResults, char *tables[], int tableCount,
			     struct hash **retHash, struct slName **retList)
/* Return all unique accessions that match any table. */
// Modified to return only the first 500 hits because of CGI timeouts
{
struct slName *list = NULL, *el;
struct hash *hash = newHash(0);
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;
char *field;
int i;

int rowCount = 0; // Excessively broad searches were leading to CGI timeouts (#11626).
for (i = 0;
     i<tableCount && (limitResults == EXHAUSTIVE_SEARCH_REQUIRED || rowCount < limitResults);
     ++i)
    {
    struct slName *idList = NULL;
    char *grepIndexFile = NULL;
    
    /* I'm doing this query in two steps in C rather than
     * in one step in SQL just because it somehow is much
     * faster this way (like 100x faster) when using mySQL. */
    field = tables[i];
    if (!sqlTableExists(conn, field))
	continue;
    if ((grepIndexFile = getGenbankGrepIndex(db, hfs, field, "idName")) != NULL)
	idList = genbankGrepQuery(grepIndexFile, field, key);
    else
        idList = genbankSqlFuzzyQuery(conn, field, key, limitResults);
    /* don't check srcDb to exclude refseq for compat with older tables */
    if (idList)
        {
        struct dyString *query = sqlDyStringCreate("select acc, organism from %s where %s in (",
                gbCdnaInfoTable, skipDb(field));
        sqlDyStringPrintValuesList(query, idList);
        sqlDyStringPrintf(query, ") and type = 'mRNA'");
        // limit results to avoid CGI timeouts (#11626).
        if (limitResults != EXHAUSTIVE_SEARCH_REQUIRED)
            sqlDyStringPrintf(query, " limit %d", limitResults);
        sr = sqlGetResult(conn, dyStringContents(query));
        dyStringFree(&query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char *acc = row[0];
            /* will use this later to distinguish xeno mrna */
            int organismID = sqlUnsigned(row[1]);
            if (!isRefSeqAcc(acc) && !hashLookup(hash, acc))
                {
                el = newSlName(acc);
                slAddHead(&list, el);
                hashAddInt(hash, acc, organismID);
                // limit results to avoid CGI timeouts (#11626).
                if (rowCount++ > limitResults && limitResults != EXHAUSTIVE_SEARCH_REQUIRED)
                    break;
                }
            }
        sqlFreeResult(&sr);
        }
    slFreeList(&idList);
    }
hFreeConn(&conn);
slReverse(&list);
*retList = list;
*retHash = hash;
}


static void andHits(struct hash *aHash, struct slName *aList, 
	struct hash *bHash, struct slName *bList,
	struct hash **retHash, struct slName **retList)
/* Return hash/list that is intersection of lists a and b. */
{
struct slName *list = NULL, *el, *newEl;
struct hash *hash = newHash(0);
for (el = aList; el != NULL; el = el->next)
    {
    char *name = el->name;
    int organismID = hashIntValDefault(bHash, name, -1);
    if (organismID >= 0 && !hashLookup(hash, name))
        {
	newEl = newSlName(name);
	slAddHead(&list, newEl);
	hashAddInt(hash, name, organismID);
	}
    }
*retHash = hash;
*retList = list;
}

static void mrnaKeysHtmlOnePos(struct hgPosTable *table, struct hgPos *pos,
			       FILE *f)
{
fprintf(f, "%s", pos->description);
}

static boolean mrnaAligns(struct sqlConnection *conn, char *table, char *acc)
/* Return TRUE if accession is in the designated alignment table (for speed,
 * this assumes that we've already checked that the table exists) */
{
char query[256];
sqlSafef(query, sizeof(query), 
      "select count(*) from %s where qName = '%s'", table, acc);
return (sqlQuickNum(conn, query) > 0);
}

static int addMrnaPositionTable(char *db, struct hgPositions *hgp, 
                                struct slName **pAccList,
				struct hash *accOrgHash, struct cart *cart,
                                struct sqlConnection *conn, char *hgAppName,
                                boolean aligns, boolean isXeno, boolean measureTiming)
/* Generate table of positions that match criteria.
 * Add to hgp if any found. Return number found */
{
struct hgPosTable *table = NULL;
struct slName *el = NULL;
struct slName *elToFree = NULL;
long startTime = clock1000();
struct dyString *dy = dyStringNew(256);
char *ui = getUiUrl(cart);
int organismID = hOrganismID(hgp->database);   /* id from mrna organism table */
int alignCount = 0;
char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';
char *mrnaTable = isXeno ? "xenoMrna" : "all_mrna";
boolean mrnaTableExists = hTableExists(hgp->database, mrnaTable);

AllocVar(table);
table->searchTime = -1;

/* Examine all accessions to see if they fit criteria for
 * this table. Add all matching to the position list, and
 * remove from the accession list */
for (el = *pAccList; el != NULL; el = el->next)
    {
    freez(&elToFree);
    char *acc = el->name;

    /* check if item matches xeno criterion */
    int itemOrganismID = hashIntVal(accOrgHash, acc);
    if (isXeno == (itemOrganismID == organismID))
        continue;

    /* check if item matches alignment criterion */
    if (aligns != (mrnaTableExists && mrnaAligns(conn, mrnaTable, acc)))
	continue;

    /* item fits criteria, so enter in table */
    struct hgPos *pos = NULL;
    AllocVar(pos);
    slAddHead(&table->posList, pos);
    pos->name = cloneString(acc);
    pos->browserName = cloneString(acc);
    dyStringClear(dy);
    
    if (aligns)
        {
        dyStringPrintf(dy, "<A HREF=\"%s%cposition=%s",
		       hgAppName, hgAppCombiner, acc);
        }
    else
        {
        /* display mRNA details page -- need to add dummy CGI variables*/
        dyStringPrintf(dy, "<A HREF=\"%s%cg=%s&i=%s&c=0&o=0&l=0&r=0",
		       hgcName(), hgAppCombiner, mrnaTable, acc);
        }
    if (ui != NULL)
        dyStringPrintf(dy, "&%s", ui);
    dyStringPrintf(dy, "%s\">", 
               hgp->extraCgi);
    dyStringPrintf(dy, "%s</A>", acc);

    /* print description for item, or lacking that, the product name */
    char description[1028];
    safef(description, sizeof(description), "%s", "n/a"); 
    char query[512];
    sqlSafef(query, sizeof(query), 
        "select d.name from %s g,%s d"
        " where g.acc = '%s' and g.description = d.id", gbCdnaInfoTable, descriptionTable, acc);
    sqlQuickQuery(conn, query, description, sizeof(description));
    if (sameString(description, "n/a"))
        {
        /* look for product name */
        sqlSafef(query, sizeof(query), 
            "select p.name from %s g,%s p"
            " where g.acc = '%s' and g.productName = p.id",
                 gbCdnaInfoTable, productNameTable, acc);
        char product[256];
        sqlQuickQuery(conn, query, product, sizeof(product));
        if (!sameString(product, "n/a"))
            {
            /* get organism name */
            sqlSafef(query, sizeof(query), 
                "select o.name from %s g,%s o"
                " where g.acc = '%s' and g.organism = o.id", gbCdnaInfoTable, organismTable, acc);
            char organism[128];
            *organism = 0;
            sqlQuickQuery(conn, query, organism, sizeof(organism));
            safef(description, sizeof(description), "%s%s%s",
                    *organism ? organism : "",
                    *organism ? ", " : "",
                    product);
            }
        }
    if (!sameString(description, "n/a"))
        /* print description if it has been loaded */
        dyStringPrintf(dy, " - %s", description);
    dyStringPrintf(dy, "\n");
    pos->description = cloneString(dy->string);

    /* remove processed element from accession list */
    slRemoveEl(pAccList, el);
    elToFree = el;
    }

/* fill in table and add to hgp only if it contains results */
alignCount = slCount(table->posList);
if (alignCount > 0)
    {
    char *organism = hOrganism(hgp->database);      /* dbDb organism column */
    if (alignCount == 1)
        {
        // So far we have not bothered to look up the coordinates because there are almost always
        // multiple matches among which the user will have to choose.  However, it is possible
        // for there to be a unique match (hgwdev 19-02-15, hg38, "elmer" --> U01022).  In that
        // case we should look up the coordinates so the user doesn't have to click through a page
        // with one match leading to another search.
        char shortLabel[256];
        safef(shortLabel, sizeof shortLabel, "%s%s %sligned mRNAs",
              isXeno ? "Non-" : "", organism,
              aligns ?  "A" : "Una");
        char *acc = table->posList->name;
        struct psl *pslList = getPslFromTable(conn, hgp->database, mrnaTable, acc);
        addPslResultToHgp(cart, hgp, hgp->database, mrnaTable, shortLabel, acc, pslList, measureTiming);
        if (hgp->tableList)
            alignCount = slCount(hgp->tableList->posList);
        else 
            alignCount = 0;
        }
    else
        {
        char title[256];
        slReverse(&table->posList);
        safef(title, sizeof(title), "%s%s %sligned mRNA Search Results",
              isXeno ? "Non-" : "", organism,
              aligns ?  "A" : "Una");
        table->description = cloneString(title);
        table->name = cloneString(mrnaTable);
        table->htmlOnePos = mrnaKeysHtmlOnePos;
        slAddHead(&hgp->tableList, table);
        }
    freeMem(organism);
    }
if (measureTiming)
    table->searchTime = clock1000() - startTime;
dyStringFree(&dy);
return alignCount;
}

static boolean findMrnaKeys(struct cart *cart, char *db, struct hgFindSpec *hfs,
			    char *keys, int limitResults, struct hgPositions *hgp, boolean measureTiming)
/* Find mRNA that has keyword in one of its fields. */
{
int alignCount;
char *tables[] = {
	productNameTable, geneNameTable,
	authorTable, tissueTable, cellTable, descriptionTable, developmentTable, 
	};
struct hash *allKeysHash = NULL;
struct slName *allKeysList = NULL;
struct sqlConnection *conn = hAllocConn(db);
boolean found = FALSE;

/* If we can use grep to search all tables, then use piped grep to 
 * implement implicit "AND" of multiple keys. */
if (gotAllGenbankGrepIndexFiles(db, hfs, tables, ArraySize(tables)))
    {
    findHitsToTables(db, hfs, keys, limitResults, tables, ArraySize(tables),
		     &allKeysHash, &allKeysList);
    }
else
    {
    struct hash *oneKeyHash = NULL;
    struct slName *oneKeyList = NULL;
    struct hash *andedHash = NULL;
    struct slName *andedList = NULL;
    char *words[32];
    char buf[512];
    int wordCount;
    int i;
    safef(buf, sizeof(buf), "%s", keys);
    wordCount = chopLine(buf, words);
    if (wordCount == 0)
	return FALSE;
    found = TRUE;
    for (i=0; i<wordCount; ++i)
	{
	findHitsToTables(db, hfs, words[i], limitResults, tables, ArraySize(tables),
			 &oneKeyHash, &oneKeyList);
	if (allKeysHash == NULL)
	    {
	    allKeysHash = oneKeyHash;
	    oneKeyHash = NULL;
	    allKeysList = oneKeyList;
	    oneKeyList = NULL;
	    }
	else
	    {
	    andHits(oneKeyHash, oneKeyList, allKeysHash, allKeysList,
		    &andedHash, &andedList);
	    freeHash(&oneKeyHash);
	    slFreeList(&oneKeyList);
	    freeHash(&allKeysHash);
	    slFreeList(&allKeysList);
	    allKeysHash = andedHash;
	    andedHash = NULL;
	    allKeysList = andedList;
	    andedList = NULL;
	    }
	}
    }
if (allKeysList == NULL)
    return FALSE;

/* generate position lists and add to hgp */
/* organism aligning */
alignCount = addMrnaPositionTable(db, hgp, &allKeysList, allKeysHash, cart, conn,
				  hgAppName, TRUE, FALSE, measureTiming);
/* organism non-aligning */
addMrnaPositionTable(db, hgp, &allKeysList, allKeysHash, cart, conn,
		     hgAppName, FALSE, FALSE, measureTiming);
/* xeno aligning */
/* NOTE: to suppress display of xeno mrna's in non-model organisms
 * (RT 801 and 687), uncommented the following...
 * add to display list only if there is a scarcity of items
 * already listed as aligning (low number of own mRna's for this organism) */
if (alignCount < 20)
    addMrnaPositionTable(db, hgp, &allKeysList, allKeysHash, cart, conn,
			 hgAppName, TRUE, TRUE, measureTiming);
hashFree(&allKeysHash);
hFreeConn(&conn);
return(found);
}

static boolean isUnsignedInt(char *s)
/* Return TRUE if s is in format to be an unsigned int. */
{
int size=0;
char c;
while ((c = *s++) != 0)
    {
    if (++size > 10 || !isdigit(c))
        return FALSE;
    }
if (size==0)
    return FALSE;
return TRUE;
}

static void addRefLinks(struct sqlConnection *conn, struct dyString *query,
	struct refLink **pList)
/* Query database and add returned refLinks to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct refLink *rl = refLinkLoad(row);
    slAddHead(pList, rl);
    }
sqlFreeResult(&sr);
}

static void addRefLinkAccs(struct sqlConnection *conn, struct slName *accList,
			   struct refLink **pList)
/* Query database and add returned refLinks to head of list. */
{
struct slName *accEl = NULL;
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];

for (accEl = accList;  accEl != NULL;  accEl = accEl->next)
    {
    sqlSafef(query, sizeof(query), "select * from %s where mrnaAcc = '%s'",
	  refLinkTable, accEl->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct refLink *rl = refLinkLoad(row);
	slAddHead(pList, rl);
	}
    sqlFreeResult(&sr);
    }
}

static boolean findRefGenes(char *db, struct hgFindSpec *hfs, char *spec,
			    struct hgPositions *hgp, boolean measureTiming)
/* Look up refSeq genes in table. */
{
long startTime = clock1000();
struct sqlConnection *conn = hAllocConn(db);
struct dyString *ds = dyStringNew(256);
struct refLink *rlList = NULL, *rl;
boolean gotRefLink = sqlTableExists(conn, refLinkTable);
boolean found = FALSE;
char *specNoVersion = cloneString(spec);
// chop off the version number, e.g. "NM_000454.4 ", 
//  but if spec starts with "." like ".stuff" then specNoVersion is entirely empty.
(void) chopPrefix(specNoVersion);  
if (gotRefLink && isNotEmpty(specNoVersion))
    {
    if (startsWith("NM_", specNoVersion) || startsWith("NR_", specNoVersion) || startsWith("XM_", specNoVersion))
	{
	sqlDyStringPrintf(ds, "select * from %s where mrnaAcc = '%s'", refLinkTable, specNoVersion);
	addRefLinks(conn, ds, &rlList);
	}
    else if (startsWith("NP_", specNoVersion) || startsWith("XP_", specNoVersion))
        {
	sqlDyStringPrintf(ds, "select * from %s where protAcc = '%s'", refLinkTable, specNoVersion);
	addRefLinks(conn, ds, &rlList);
	}
    else if (isUnsignedInt(specNoVersion))
        {
	sqlDyStringPrintf(ds, "select * from %s where locusLinkId = '%s'",
		       refLinkTable, specNoVersion);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	sqlDyStringPrintf(ds, "select * from %s where omimId = '%s'", refLinkTable,specNoVersion);
	addRefLinks(conn, ds, &rlList);
	}
    else 
	{
	char *indexFile = getGenbankGrepIndex(db, hfs, refLinkTable, "mrnaAccProduct");
	sqlDyStringPrintf(ds, "select * from %s where name like '%s%%' limit %d",
		       refLinkTable, specNoVersion, NONEXHAUSTIVE_SEARCH_LIMIT);
	addRefLinks(conn, ds, &rlList);
	if (indexFile != NULL)
	    {
	    struct slName *accList = doGrepQuery(indexFile, refLinkTable, specNoVersion,
						 NULL);
	    addRefLinkAccs(conn, accList, &rlList);
	    }
	else
	    {
	    dyStringClear(ds);
	    sqlDyStringPrintf(ds, "select * from %s where product like '%%%s%%' limit %d",
			   refLinkTable, specNoVersion, NONEXHAUSTIVE_SEARCH_LIMIT);
	    addRefLinks(conn, ds, &rlList);
	    }
	}
    }
if (rlList != NULL)
    {
    struct hgPosTable *table = NULL;
    struct hash *hash = newHash(8);
    struct genePredReader *gpr;
    struct genePred *gp;
    struct slName *values = NULL;
    struct dyString *where = sqlDyStringCreate("name in (");
    // convert the rlList to slNames for sql comma sep strings
    for (rl = rlList; rl != NULL; rl = rl->next)
        {
        /* Don't return duplicate mrna accessions */
        if (hashFindVal(hash, rl->mrnaAcc))
            {            
            continue;
            }
        hashAdd(hash, rl->mrnaAcc, rl);
        slAddHead(&values, slNameNew(rl->mrnaAcc));
        }
    slReverse(&values);
    sqlDyStringPrintValuesList(where, values);
    sqlDyStringPrintf(where, ") order by find_in_set(name, '%s')", slNameListToString(values,','));
    gpr = genePredReaderQuery(conn, hfs->searchTable, dyStringCannibalize(&where));
    while ((gp = genePredReaderNext(gpr)) != NULL)
        {
        struct hgPos *pos = NULL;
        AllocVar(pos);
        if (table == NULL)
            {
            char desc[256];
            AllocVar(table);
            table->searchTime = -1;
            table->name = cloneString(hfs->searchTable);
            if (startsWith("xeno", hfs->searchTable))
                safef(desc, sizeof(desc), "Non-%s RefSeq Genes", hOrganism(db));
            else
                safef(desc, sizeof(desc), "RefSeq Genes");
            table->description = cloneString(desc);
            slAddHead(&hgp->tableList, table);
            }
        struct refLink *rl = (struct refLink *)hashFindVal(hash, gp->name);
        slAddHead(&table->posList, pos);
        pos->name = cloneString(rl->name);
        pos->browserName = cloneString(rl->mrnaAcc);
        dyStringClear(ds);
        dyStringPrintf(ds, "(%s) %s", rl->mrnaAcc, rl->product);
        pos->description = cloneString(ds->string);
        pos->chrom = hgOfficialChromName(db, gp->chrom);
        pos->chromStart = gp->txStart;
        pos->chromEnd = gp->txEnd;
        genePredFree(&gp);
        found = TRUE;
        }
    genePredReaderFree(&gpr);
    if (table != NULL && measureTiming)
        table->searchTime = clock1000() - startTime;
    refLinkFreeList(&rlList);
    freeHash(&hash);
    }
dyStringFree(&ds);
hFreeConn(&conn);
return(found);
}

/* Lowe lab additions */

static void addTigrCmrGenes(struct sqlConnection *conn, struct dyString *query,
	struct tigrCmrGene **pList)
/* Query database and add returned tigrCmrGenes to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct tigrCmrGene *rl = tigrCmrGeneLoad(row);
    slAddHead(pList, rl);
    }
sqlFreeResult(&sr);
}

static void findTigrGenes(char *db, char *spec, struct hgPositions *hgp)
/* Look up TIGR and Genbank genes from keyword */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
struct dyString *ds = dyStringNew(256);
char **row;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct bed *bed;
struct tigrCmrGene *tigrList = NULL, *tigr;
/* struct minGeneInfo *gbList = NULL, *gb; */
boolean gotTIGRkeys = sqlTableExists(conn, "tigrCmrORFsInfo");

if (gotTIGRkeys)
    {
    sqlDyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrCommon like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    sqlDyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrMainRole like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    sqlDyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrSubRole like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    }
if (tigrList != NULL)
    {
    struct hash *hash = newHash(8);
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString("TIGR CMR Genes");
    table->name = cloneString("tigrORFsCmr");
    for (tigr = tigrList; tigr != NULL; tigr = tigr->next)
        {
        /* Don't return duplicate TIGR CMR accessions */
        if (hashFindVal(hash, tigr->name))
            {
            hashAdd(hash, tigr->name, tigr);
            continue;
            }
        hashAdd(hash, tigr->name, tigr);
	dyStringClear(ds);
	sqlDyStringPrintf(ds, "select * from tigrCmrORFs where name = '%s'", tigr->name);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    bed = bedLoadN(row+1,6);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(tigr->name);
	    pos->browserName = cloneString(tigr->name);
	    dyStringClear(ds);
	    dyStringPrintf(ds, "%s; %s; %s", tigr->tigrCommon, tigr->tigrMainRole, tigr->tigrSubRole);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(db, bed->chrom);
	    pos->chromStart = bed->chromStart;
	    pos->chromEnd = bed->chromEnd;
	    bedFree(&bed);
	    }
	sqlFreeResult(&sr);
	}
    tigrCmrGeneFreeList(&tigrList);
    freeHash(&hash);
    }
dyStringFree(&ds);
hFreeConn(&conn);
}

/* End of Lowe Lab stuff */

static boolean findGenePredPattern(char *db, char *pattern, struct hgPositions *hgp,
				   char *tableName, struct hgPosTable *table)
/* Look for position pattern in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
struct hgPos *pos = NULL;

if (!hTableExists(db, tableName))
    return FALSE;
conn = hAllocConn(db);
query = dyStringNew(256);
sqlDyStringPrintf(query,
	      "SELECT chrom, txStart, txEnd, name FROM %s WHERE name LIKE '%s'",
	      tableName, pattern);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	if (table == NULL)
	    {
	    AllocVar(table);
	    struct dyString *desc = dyStringNew(256);
	    dyStringPrintf(desc, "%s Gene Predictions", tableName);
	    table->description = dyStringCannibalize(&desc);
	    table->name = cloneString(tableName);
	    slAddHead(&hgp->tableList, table);
	    }
	}
    
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(db, row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    pos->browserName = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
dyStringFree(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static void addUniqYeastGene(char *db, struct hash *uniqHash, 
	struct sqlConnection *conn, char *query, 
	struct hgPositions *hgp, char *geneTable,
	struct hgPosTable **pTable)
/* Execute query which returns a single row, and add genes. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct hgPosTable *table = *pTable;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    if (!hashLookup(uniqHash, id))
	{
	hashAdd(uniqHash, id, NULL);
	if (table == NULL)
	    {
	    AllocVar(table);
	    table->name = geneTable;
	    table->description = "Genes from Sacchromyces Genome Database";
	    slAddHead(&hgp->tableList, table);
	    *pTable = table;
	    }
	findGenePredPattern(db, id, hgp, geneTable, table);
	}
    }
sqlFreeResult(&sr);
}

static boolean findYeastGenes(char *db, char *pattern, struct hgPositions *hgp)
/* Scan yeast-specific tables. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row, query[256];
struct hgPosTable *table = NULL;
boolean found = FALSE;

if (hTableExists(db, "sgdGene"))
    {
    struct hash *uniqHash = newHash(0);
    boolean gotNames = FALSE, gotDescriptions = FALSE;
    sqlSafef(query, sizeof(query), 
        "select name from sgdGene where name = '%s'", pattern);
    addUniqYeastGene(db, uniqHash, conn, query, hgp, "sgdGene", &table);
    if (hTableExists(db, "sgdToName"))
	{
	gotNames = TRUE;
	sqlSafef(query, sizeof(query), 
	    "select name from sgdToName where value like '%s%%'", pattern);
	addUniqYeastGene(db, uniqHash, conn, query, hgp, "sgdGene", &table);
	}
    if (hTableExists(db, "sgdDescription"))
        {
	gotDescriptions = TRUE;
	sqlSafef(query, sizeof(query), 
	    "select name from sgdDescription where description like '%%%s%%'", 
	    pattern);
	addUniqYeastGene(db, uniqHash, conn, query, hgp, "sgdGene", &table);
	}
    hashFree(&uniqHash);

    /* Add descriptions to table. */
    if (table != NULL)
        {
	struct hgPos *pos;
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    struct dyString *dy = dyStringNew(1024);
	    if (gotNames)
		{
		sqlSafef(query, sizeof(query),
		   "select value from sgdToName where name = '%s'", pos->name);
	        sr = sqlGetResult(conn, query);
		while ((row = sqlNextRow(sr)) != NULL)
		    dyStringPrintf(dy, "(%s) ", row[0]);
		sqlFreeResult(&sr);
		}
	    if (gotDescriptions)
		{
		sqlSafef(query, sizeof(query),
		   "select description from sgdDescription where name = '%s'", 
		   pos->name);
	        sr = sqlGetResult(conn, query);
		if ((row = sqlNextRow(sr)) != NULL)
		    dyStringPrintf(dy, "%s", row[0]);
		sqlFreeResult(&sr);
		}
	    if (dy->stringSize > 0)
		pos->description = cloneString(dy->string);
	    dyStringFree(&dy);
	    }
	found = TRUE;
	}
    }
hFreeConn(&conn);
return(found);
}

void hgPositionsHtml(char *db, struct hgPositions *hgp, char *hgAppName, struct cart *cart)
/* Write multiple search results as HTML. */
{
struct hgPosTable *table;
struct hgPos *pos;
char *desc;
char range[HGPOSRANGESIZE];
char *ui = getUiUrl(cart);
char *extraCgi = hgp->extraCgi;
char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';
boolean containerDivPrinted = FALSE;
struct trackDb *tdbList = NULL;
// This used to be an argument, but only stdout was used:
FILE *f = stdout;

if (hgp->posCount == 0)
    {
    fprintf(f, "<div id='hgFindResults'>\n");
    fprintf(f, "<p>No additional items found</p>");
    fprintf(f, "</div>\n");
    return;
    }
for (table = hgp->tableList; table != NULL; table = table->next)
    {
    if (table->posList != NULL)
	{
    char *trackName = table->name, *tableName = table->name;
    struct trackDb *tdb = NULL;
    // clear the tdb cache if this track is a hub track
    if ((sameString("trackDb", tableName) || sameString("helpDocs", tableName) ||
            sameString("publicHubs", tableName)))
        // not relevant for hgTables/hgVai/hgIntegrator
        continue;
    else
        {
        if (isHubTrack(tableName))
            tdbList = NULL;
        tdb = tdbForTrack(db, tableName, &tdbList);
        if (!tdb && startsWith("all_", tableName))
            tdb = tdbForTrack(db, tableName+strlen("all_"), &tdbList);
        if (!tdb && startsWith("xeno", tableName))
            {
            // due to genbank track changes over the years, sometimes tables
            // get left on different servers when their trackDb entry was removed
            // long ago. In that case skip those hits
            continue;
            }
        if (!tdb)
            errAbort("no track for table \"%s\" found via a findSpec", tableName);
        trackName = tdb->track;
        }
    char *vis = hCarefulTrackOpenVisCart(cart, db, trackName);
    boolean excludeTable = FALSE;
    if(!containerDivPrinted)
        {
        fprintf(f, "<div id='hgFindResults'>\n");
        if (hgp->singlePos == NULL) // we might be called with only one result
            fprintf(f, "<p>Your search resulted in multiple matches.  "
                    "Please select a position:</p>\n");
        containerDivPrinted = TRUE;
        }
    if (table->htmlStart) 
        table->htmlStart(table, f);
    else
        fprintf(f, "<H2>%s</H2><PRE>\n", table->description);
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
        if (table->htmlOnePos)
            table->htmlOnePos(table, pos, f);
        else
        {
        char *matches = excludeTable ? "" : pos->browserName;
        char *encMatches = cgiEncode(matches);
        hgPosBrowserRange(pos, range);
        fprintf(f, "<A HREF=\"%s%cposition=%s",
            hgAppName, hgAppCombiner, range);
        if (ui != NULL)
            fprintf(f, "&%s", ui);
        fprintf(f, "%s&", extraCgi);
        fprintf(f, "%s=%s&", trackName, vis);
        // this is magic to tell the browser to make the 
        // composite and this subTrack visible
        if (tdb->parent)
            {
            if (tdbIsSuperTrackChild(tdb))
            fprintf(f, "%s=show&", tdb->parent->track);
            else
            {
            // tdb is a subtrack of a composite or a view
            fprintf(f, "%s_sel=1&", trackName);
            fprintf(f, "%s_sel=1&", tdb->parent->track);
            }
            }
                if (isNotEmpty(pos->highlight))
                    {
                    char *encHighlight = cgiEncode(pos->highlight);
                    fprintf(f, "addHighlight=%s&", encHighlight);
                    freeMem(encHighlight);
                    }
        fprintf(f, "hgFind.matches=%s,\">", encMatches);
        // Bold canonical genes. 
        if(pos->canonical) {
            fprintf(f, "<B>");
            }
        htmTextOut(f, pos->name);
        if(pos->canonical) {
            fprintf(f, "</B>");
            }
        fprintf(f, " at %s</A>", range);
        desc = pos->description;
        if (desc)
            {
            fprintf(f, " - ");
            htmTextOut(f, desc);
            }
        fprintf(f, "\n");
        freeMem(encMatches);
        }
        }
    if (table->htmlEnd) 
        table->htmlEnd(table, f);
    else
        fprintf(f, "</PRE>\n");
	}
    }

if(containerDivPrinted)
    {
    if (hgp->shortCircuited)
        {
        char *queryString = getenv("QUERY_STRING");
        char *addString = "&noShort=1";
        if (isEmpty(queryString))
            addString = "noShort=1";
        fprintf(f, "<A HREF=\"%s?%s%s\"> More results...</A>", hgAppName, queryString, addString);
        }
    fprintf(f, "</div>\n");
    }
}

static struct hgPositions *hgPositionsSearch(char *db, char *spec,
                                             char **retChromName, int *retWinStart, int *retWinEnd,
                                             boolean *retIsMultiTerm, struct cart *cart,
                                             char *hgAppName, char **retMultiChrom,
                                             struct dyString *dyWarn, struct searchCategory *categories)
/* Search for positions that match spec (possibly ;-separated in which case *retIsMultiTerm is set).
 * Return a container of tracks and positions (if any) that match term.  If different components
 * of a multi-term search land on different chromosomes then *retMultiChrom will be set. */
{
struct hgPositions *hgp = NULL;
char *chrom = NULL;
int start = INT_MAX;
int end = 0;
char *terms[16];
int termCount = chopByChar(cloneString(spec), ';', terms, ArraySize(terms));
boolean multiTerm = (termCount > 1);
boolean measureTiming = cartUsualBoolean(cart, "measureTiming", FALSE);
if (retIsMultiTerm)
    *retIsMultiTerm = multiTerm;
if (retMultiChrom)
    *retMultiChrom = NULL;
int i;
for (i = 0;  i < termCount;  i++)
    {
    trimSpaces(terms[i]);
    if (isEmpty(terms[i]))
	continue;
    // Append warning messages to dyWarn, but allow errAborts to continue
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        hgp = hgPositionsFind(db, terms[i], "", hgAppName, cart, multiTerm, measureTiming, categories);
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        errAbort("%s", errCatch->message->string);
    else if (isNotEmpty(errCatch->message->string))
        dyStringAppend(dyWarn, errCatch->message->string);
    errCatchFree(&errCatch);
    if (hgp->singlePos != NULL)
	{
	if (retMultiChrom && chrom != NULL && differentString(chrom, hgp->singlePos->chrom))
            *retMultiChrom = cloneString(chrom);
	chrom = hgp->singlePos->chrom;
	if (hgp->singlePos->chromStart < start)
	    start = hgp->singlePos->chromStart;
	if (hgp->singlePos->chromEnd > end)
	    end = hgp->singlePos->chromEnd;
	}
    else if (hgp->posCount == 0 || (multiTerm && hgp->posCount > 1))
        break;
    }
if (retChromName)
    *retChromName = (chrom == NULL) ? chrom : hgOfficialChromName(db, chrom);
if (retWinStart)
    *retWinStart  = start;
if (retWinEnd)
    *retWinEnd    = end;
return hgp;
}

static struct hgPositions *revertPosition(struct cart *cart, char **pPosition,
                                          char **retChrom, int *retStart, int *retEnd,
                                          char *hgAppName, struct dyString *dyWarn)
/* Revert *pPosition to lastPosition (or default position).  Return a new hgp for the
 * resolved position.  Append warnings to dyWarn, errAbort if defaultPos doesn't work.  */
{
struct hgPositions *hgp = NULL;
boolean isMultiTerm = FALSE;
char *multiDiffChrom = NULL;
char *db = cartString(cart, "db");
char *lastPosition = cartOptionalString(cart, "lastPosition");
if (isNotEmpty(lastPosition) && !IS_CART_VAR_EMPTY(lastPosition))
    {
    if (startsWith(MULTI_REGION_CHROM, lastPosition) || 
        startsWith(OLD_MULTI_REGION_CHROM, lastPosition))
        {
        lastPosition = cartUsualString(cart, "nonVirtPosition", hDefaultPos(db));
        }
    hgp = hgPositionsSearch(db, lastPosition, retChrom, retStart, retEnd, &isMultiTerm,
                            cart, hgAppName, &multiDiffChrom, dyWarn, NULL);
    if (hgp->singlePos && !(isMultiTerm && isNotEmpty(multiDiffChrom)))
        {
        freez(pPosition);
        *pPosition = cloneString(lastPosition);
        return hgp;
        }
    else
        dyStringPrintf(dyWarn, "  Unable to resolve lastPosition '%s'; "
                       "reverting to default position.", lastPosition);
    }
char *defaultPosition = hDefaultPos(db);
hgp = hgPositionsSearch(db, defaultPosition, retChrom, retStart, retEnd, &isMultiTerm,
                        cart, hgAppName, &multiDiffChrom, dyWarn, NULL);
if (hgp->singlePos && !(isMultiTerm && isNotEmpty(multiDiffChrom)))
    {
    freez(pPosition);
    *pPosition = cloneString(defaultPosition);
    }
else
    errAbort("Unable to resolve default position '%s' for database '%s'.",
             defaultPosition, db);
return hgp;
}

static boolean posIsObsolete(char *pos)
/* Return TRUE if pos is genome (or other obsolete keyword).  Once upon a time position=genome
 * was used to indicate genome-wide search, but now we have an independent option. */
{
pos = trimSpaces(pos);
return(sameWord(pos, "genome") || sameWord(pos, "hgBatch"));
}

struct hgPositions *hgFindSearch(struct cart *cart, char **pPosition,
                                 char **retChrom, int *retStart, int *retEnd,
                                 char *hgAppName, struct dyString *dyWarn, struct searchCategory *categories)
/* If *pPosition is a search term, then try to resolve it to genomic position(s).
 * If unable to find a unique position then revert pPosition to lastPosition (or default position).
 * Return a container of matching tables and positions.  Warnings/errors are appended to dyWarn. */
{
struct hgPositions *hgp = NULL;
if (posIsObsolete(*pPosition))
    {
    hgp = revertPosition(cart, pPosition, retChrom, retStart, retEnd, hgAppName, dyWarn);
    }
else
    {
    boolean isMultiTerm = FALSE;
    char *multiDiffChrom = NULL;
    char *db = cartString(cart, "db");
    hgp = hgPositionsSearch(db, *pPosition, retChrom, retStart, retEnd,
                            &isMultiTerm, cart, hgAppName, &multiDiffChrom, dyWarn, categories);
    if (isMultiTerm && isNotEmpty(multiDiffChrom))
        {
        dyStringPrintf(dyWarn, "Sites occur on different chromosomes: %s, %s.",
                       multiDiffChrom, hgp->singlePos->chrom);
        hgp = revertPosition(cart, pPosition, retChrom, retStart, retEnd, hgAppName, dyWarn);
        }
    else if (hgp->posCount > 1 ||
             // In weird cases it's possible to get a single result that does not have coords, but
             // leads to another search a la multiple results!  That happened with genbank keyword
             // search ("elmer" in hg19, hg38 Feb. '19).  I fixed it but there could be other cases.
             (hgp->posCount == 1 && !hgp->singlePos))
        {
        if (isMultiTerm)
            dyStringPrintf(dyWarn, "%s not uniquely determined (%d locations) -- "
                           "can't do multi-position search.",
                           hgp->query, hgp->posCount);
        // Revert position in cart (#13009), but don't replace hgp -- hgPositionsHtml will need it.
        revertPosition(cart, pPosition, retChrom, retStart, retEnd, hgAppName, dyWarn);
        }
    else if (hgp->posCount == 0)
        {
        dyStringPrintf(dyWarn, "Sorry, couldn't locate %s in %s %s", hgp->query,
                       trackHubSkipHubName(hOrganism(db)), hFreezeDate(db));
        hgp = revertPosition(cart, pPosition, retChrom, retStart, retEnd, hgAppName, dyWarn);
        }
    if (hgp->singlePos && isEmpty(dyWarn->string))
        {
        char position[512];
        safef(position, sizeof(position), "%s:%d-%d",
              hgp->singlePos->chrom, hgp->singlePos->chromStart+1, hgp->singlePos->chromEnd);
        *pPosition = cloneString(addCommasToPos(NULL, position));
        }
    }
return hgp;
}

#if 0 /* not used */
static void noRelative(boolean relativeFlag, int relStart, int relEnd,
		       char *table)
{
if (relativeFlag)
    hUserAbort("Sorry, range spec (\":%d-%d\") is not supported for %s.",
	     relStart+1, relEnd, table);

}
#endif

static boolean isBigFileFind(struct hgFindSpec *hfs)
/* is this a find on a big* file? */
{
return sameString(hfs->searchType, "bigBed")
    || sameString(hfs->searchType, "bigPsl")
    || sameString(hfs->searchType, "bigBarChart")
    || sameString(hfs->searchType, "bigGenePred");
}

static boolean findBigBed(struct cart *cart, char *db, struct hgFindSpec *hfs, char *spec,
			    struct hgPositions *hgp, boolean measureTiming)
/* Look up items in bigBed  */
{
struct trackDb *tdb = tdbFindOrCreate(db, NULL, hfs->searchTable);

return findBigBedPosInTdbList(cart, db, tdb, spec, hgp, hfs, measureTiming);
}

boolean searchSpecial(struct cart *cart,
                             char *db, struct hgFindSpec *hfs, char *term, int limitResults,
			     struct hgPositions *hgp, boolean relativeFlag,
			     int relStart, int relEnd, boolean *retFound, boolean measureTiming)
/* Handle searchTypes for which we have special code.  Return true if 
 * we have special code.  Set retFind according to whether we find term. */
{
boolean isSpecial = TRUE;
boolean found = FALSE;
char *upcTerm = cloneString(term);
touppers(upcTerm);
if (startsWith("knownGene", hfs->searchType))
    {
    char *knownDatabase = hdbDefaultKnownDb(db);
    char *name = (sameString(knownDatabase, db)) ? "knownGene" : knownDatabase;
    char *indexPath = hReplaceGbdb(hgFindSpecSetting(hfs, "searchTrix"));
    if (indexPath == NULL)
        indexPath = makeIndexPath(db, name);
    if (gotFullText(db, indexPath))
	found = findKnownGeneFullText(db, term, hgp, name, indexPath, hfs, measureTiming);
    }
else if (sameString(hfs->searchType, "refGene"))
    {
    found = findRefGenes(db, hfs, term, hgp, measureTiming);
    }
else if (isBigFileFind(hfs))
    {
    found = findBigBed(cart, db, hfs, term, hgp, measureTiming);
    }
else if (sameString(hfs->searchType, "cytoBand"))
    {
    char *chrom;
    int start, end;
    found = hgFindCytoBand(db, term, &chrom, &start, &end);
    if (found)
	singlePos(hgp, hfs->searchDescription, NULL, hfs->searchTable, term,
		  term, chrom, start, end);
    }
else if (sameString(hfs->searchType, "gold"))
    {
    char *chrom;
    int start, end;
    found = findChromContigPos(db, term, &chrom, &start, &end);
    if (found)
	{
	if (relativeFlag)
	    {
	    end = start + relEnd;
	    start = start + relStart;
	    }
	singlePos(hgp, hfs->searchDescription, NULL, hfs->searchTable, term,
		  term, chrom, start, end);
	}
    }
else if (sameString(hfs->searchType, "mrnaAcc"))
    {
    found = findMrnaPos(cart, db, term, hgp, measureTiming);
    }
else if (sameString(hfs->searchType, "mrnaKeyword"))
    {
    found = findMrnaKeys(cart, db, hfs, upcTerm, limitResults, hgp, measureTiming);
    }
else if (sameString(hfs->searchType, "sgdGene"))
    {
    found = findYeastGenes(db, term, hgp);
    }
else
    {
    isSpecial = FALSE;
    }
*retFound = found;
freeMem(upcTerm);
return(isSpecial);
}


static struct slPair *getXrefTerms(char *db, struct hgFindSpec *hfs, char *term)
/* Search xrefTable for xrefQuery with term.  Return all matching names. */
{
struct slPair *xrefList = NULL, *xrefPtr = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row;
boolean isFuzzy = sameWord(hfs->searchMethod, "fuzzy");

// TODO we could re-work this better to get to upstream sql creation and 
//  then be able to avoid this complexity:?
// hfs->refTable sometimes contains a comma-separated table list
// in trackDb.ra and hgFindSpec table.

// example from human/hg19/trackDb.ra
// xrefTable kgXref, ucscRetroInfo5
// xrefQuery select ucscRetroInfo5.name, spDisplayID from %s where spDisplayID like '%s%%' and kgName = kgID

// NOTE this also goes into hgFindSpec table as hti fields hfs->xrefTable and hfs->xrefQuery.
// hfs->xrefTable is sometimes a comma-separated list of fields
//  xrefTable = [hgFixed.refLink, ucscRetroInfo8]

struct dyString *dy = dyStringNew(256);
sqlCkIl(xrefTableSafe, hfs->xrefTable)
// Replace the %s with %-s if it has not already been done in the upstream source .ra files
// it would be better to do this upstream in .ra and hgFindSpec
char *update = replaceChars(hfs->xrefQuery, " from %s ", " from %-s ");  // this patches older values that still need it.
sqlDyStringPrintf(dy, update, xrefTableSafe, term);
sqlDyStringPrintf(dy, " limit %d", NONEXHAUSTIVE_SEARCH_LIMIT);
freeMem(update);

sr = sqlGetResult(conn, dy->string);
dyStringFree(&dy);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!isFuzzy || keyIsPrefixIgnoreCase(term, row[1]))
        {
	xrefPtr = slPairNew(cloneString(row[1]), cloneString(row[0]));
	slAddHead(&xrefList, xrefPtr);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&xrefList);
if (xrefList == NULL && hgFindSpecSetting(hfs, "searchBoth") != NULL)
    xrefList = slPairNew(cloneString(""), cloneString(term));
return(xrefList);
}

char *addHighlight(char *db, char *chrom, unsigned start, unsigned end)
/* Return a string that can be assigned to the cart var addHighlight, to add a yellow highlight
 * at db.chrom:start+1-end for search results. */
{
char *color = "fcfcac";
struct dyString *dy = dyStringCreate("%s.%s:%u-%u#%s", db, chrom, start+1, end, color);
return dyStringCannibalize(&dy);
}

static boolean doQuery(char *db, struct hgFindSpec *hfs, struct slPair *xrefList, char *term,
		       struct hgPositions *hgp,
		       boolean relativeFlag, int relStart, int relEnd,
		       boolean multiTerm, int limitResults, boolean measureTiming)
/* Perform a query as specified in hfs, assuming table existence has been 
 * checked and xref'ing has been taken care of. */
{
struct slName *tableList = hSplitTableNames(db, hfs->searchTable);
struct slName *tPtr = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row = NULL;
char *termPrefix = hgFindSpecSetting(hfs, "termPrefix");
char *paddingStr = hgFindSpecSetting(hfs, "padding");
int padding = isEmpty(paddingStr) ? 0 : atoi(paddingStr);
boolean found = FALSE;
char *description = NULL;
char buf[2048];
long startTime = clock1000();

if (isNotEmpty(termPrefix) && startsWith(termPrefix, term))
    term += strlen(termPrefix);
if (isEmpty(term))
    return(FALSE);

if (isNotEmpty(hfs->searchDescription))
    truncatef(buf, sizeof(buf), "%s", hfs->searchDescription);
else
    safef(buf, sizeof(buf), "%s", hfs->searchTable);
description = cloneString(buf);

if (hgp->tableList != NULL &&
    sameString(hgp->tableList->name, hfs->searchTable) &&
    sameString(hgp->tableList->description, description))
    table = hgp->tableList;

// this is taken from hgFindSpecCustom.c:checkQueryFormat() and changed to
// capture the name field for a table
static char *queryFormatRegexForIdField =
    "^select [[:alnum:]]+, ?[[:alnum:]]+, ?[[:alnum:]]+, ?[[:alnum:]]+ "
    "from %s where ([[:alnum:]]+) (r?like|=) ['\"]?.*%s.*['\"]?$";
regmatch_t substrs[3];
for (tPtr = tableList;  tPtr != NULL;  tPtr = tPtr->next )
    {
    struct dyString *query = sqlDyStringCreate(hfs->query, tPtr->name, term);
    if (xrefList)
        {
        // NOTE: hfsPolish guarantees the query format and allows the below regex to work.
        // See hgFindSpecCustom.c for more details, specifically checkQueryFormat()
        if (regexMatchSubstr(hfs->query, queryFormatRegexForIdField, substrs, ArraySize(substrs)))
            {
            char *idField = regexSubstringClone(hfs->query, substrs[1]);
            sqlDyStringPrintf(query, " or %s in (", idField);
            struct slName *vals = NULL;
            struct slPair *ptr = xrefList;
            for (; ptr != NULL; ptr = ptr->next)
                {
                if (ptr->name && isNotEmpty(ptr->name))
                    {
                    // we got an actual xref term, otherwise the val
                    // just is the term clonestring'd, which we may have
                    // changed via termPrefix above
                    slAddHead(&vals, newSlName((char *)ptr->val));
                    }
                }
            if (!vals)
                slAddHead(&vals, newSlName(term));
            slReverse(&vals);
            sqlDyStringPrintValuesList(query, vals);
            sqlDyStringPrintf(query, ") order by find_in_set(%s, '%s')", idField, slNameListToString(vals, ','));
            }
        }
    if (limitResults != EXHAUSTIVE_SEARCH_REQUIRED)
        sqlDyStringPrintf(query, " limit %d", limitResults);
    sr = sqlGetResult(conn, dyStringContents(query));
    dyStringFree(&query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        if(table == NULL)
            {
            AllocVar(table);
            table->searchTime = -1;
            table->description = description;
            table->name = cloneString(hfs->searchTable);
            slAddHead(&hgp->tableList, table);
            }
        found = TRUE;
        AllocVar(pos);
        pos->chrom = cloneString(row[0]);
        pos->chromStart = atoi(row[1]);
        pos->chromEnd = atoi(row[2]);
        safef(buf, sizeof(buf), "%s%s",
              termPrefix ? termPrefix : "", row[3]);
        pos->name = cloneString(buf);
        pos->browserName = cloneString(row[3]);
        if (relativeFlag && (pos->chromStart + relEnd) <= pos->chromEnd)
            {
            pos->chromEnd   = pos->chromStart + relEnd;
            pos->chromStart = pos->chromStart + relStart;
            }
        else if (padding > 0 && !multiTerm)
            {
            // highlight the item bases to distinguish from padding
            pos->highlight = addHighlight(db, pos->chrom, pos->chromStart, pos->chromEnd);
            int chromSize = hChromSize(db, pos->chrom);
            pos->chromStart -= padding;
            pos->chromEnd   += padding;
            if (pos->chromStart < 0)
                pos->chromStart = 0;
            if (pos->chromEnd > chromSize)
                pos->chromEnd = chromSize;
            }
        slAddHead(&table->posList, pos);
        }

    }
if (table != NULL)
    slReverse(&table->posList);
sqlFreeResult(&sr);
hFreeConn(&conn);
slFreeList(&tableList);
if (measureTiming && table)
    table->searchTime += clock1000() - startTime;
return(found);
}

static boolean hgFindUsingSpec(struct cart *cart,
                        char *db, struct hgFindSpec *hfs, char *term, int limitResults,
			struct hgPositions *hgp, boolean relativeFlag,
			int relStart, int relEnd, boolean multiTerm, boolean measureTiming)
/* Perform the search described by hfs on term.  If successful, put results
 * in hgp and return TRUE.  (If not, don't modify hgp.) */
{
struct slPair *xrefList = NULL; 
boolean found = FALSE;

if (hfs == NULL || term == NULL || hgp == NULL)
    errAbort("NULL passed to hgFindUsingSpec.\n");

if (strlen(term)<2 && !
    (sameString(hfs->searchName, "knownGene") ||
     sameString(hfs->searchName, "flyBaseGeneSymbolOneLetter")))
    return FALSE;

if (isNotEmpty(hfs->termRegex) && ! regexMatchNoCase(term, hfs->termRegex))
    return(FALSE);

if ((!(sameString(hfs->searchType, "mrnaKeyword") || sameString(hfs->searchType, "mrnaAcc")))
    && !isBigFileFind(hfs))
    {
    if (! hTableOrSplitExists(db, hfs->searchTable))
        return(FALSE);
    }

if (isNotEmpty(hfs->searchType) && searchSpecial(cart, db, hfs, term, limitResults,
                        hgp, relativeFlag, relStart, relEnd, &found, measureTiming))
    return(found);

if (isNotEmpty(hfs->xrefTable))
    {
    struct sqlConnection *conn = hAllocConn(db);
    // NOTE hfs->xrefTable can sometimes contain a comma-separated table list, 
    // rather than just a single table. 
    char *tables = replaceChars(hfs->xrefTable, ",", " ");
    boolean exists = sqlTablesExist(conn, tables);
    hFreeConn(&conn);
    freeMem(tables);
    if (! exists)
	return(FALSE);
    
    xrefList = getXrefTerms(db, hfs, term);
    }
else
    xrefList = slPairNew(cloneString(""), cloneString(term));

found |= doQuery(db, hfs, xrefList, term, hgp,
         relativeFlag, relStart, relEnd, multiTerm, limitResults, measureTiming);
slPairFreeValsAndList(&xrefList);
return(found);
}


/* Support these formats for range specifiers.  Note the ()'s around chrom,
 * start and end portions for substring retrieval: */
char *canonicalRangeExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([-0-9,]+)"
		     "[[:space:]]*[-_][[:space:]]*"
		     "([0-9,]+)$";
char *gbrowserRangeExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)"
		     "[[:space:]]*\\.\\.[[:space:]]*"
		     "([0-9,]+)$";
char *lengthRangeExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)"
		     //"[[:space:]]*\\^[[:space:]]*"
		     "[[:space:]]*\\+[[:space:]]*"
		     "([0-9,]+)$";
char *bedRangeExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]+"
		     "([0-9,]+)"
		     "[[:space:]]+"
		     "([0-9,]+)$";
char *sqlRangeExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]*\\|[[:space:]]*"
		     "([0-9,]+)"
		     "[[:space:]]*\\|[[:space:]]*"
		     "([0-9,]+)$";

char *singleBaseExp = 
		     "^([[:alnum:]._#\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)$";

// allow gnomad variants, ex: 12-1234-A-C
char *gnomadVarExp =
            "^(([0-9]+)|(X|Y|M|MT))-"
            "([0-9]+)-"
            "([A-Za-z]+)-"
            "([A-Za-z]+)$";

// allow gnomad ranges, ex: 12-1234-11223344
char *gnomadRangeExp =
            "^(([0-9]+)|(X|Y|M|MT))-"
            "([0-9]+)-"
            "([0-9]+)$";

static void collapseSamePos(struct hgPositions *hgp)
/* If all positions in all tables in hgp are the same position, then 
 * trim all but the first table/pos. */
{
struct hgPosTable *firstTable = NULL, *table;
struct hgPos *firstPos = NULL, *pos;
char *chrom = NULL;
int start=0, end=0;

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
	if (pos->chrom != NULL)
	    {
	    if (chrom == NULL)
		{
		chrom = pos->chrom;
		start = pos->chromStart;
		end = pos->chromEnd;
		firstTable = table;
		firstPos = pos;
		}
	    else if (! (sameString(chrom, pos->chrom) &&
			start == pos->chromStart &&
			end == pos->chromEnd))
		return;
	    }
	}
    }
if (firstPos)
    {
    hgp->tableList = firstTable;
    hgp->tableList->posList = firstPos;
    hgPosTableFreeList(&(hgp->tableList->next));
    hgPosFreeList(&(hgp->tableList->posList->next));
    }
}

static boolean searchKnownCanonical(char *db, char *term, struct hgPositions *hgp)
/* Look for term in kgXref.geneSymbol, and if found, put knownCanonical coords and 
 * knownGene.name in hgp. */
{
boolean foundIt = FALSE;
char *knownDatabase = hdbDefaultKnownDb(db);
struct sqlConnection *conn = hAllocConn(knownDatabase);
if (sqlTableExists(conn, "knownGene") && sqlTableExists(conn, "knownCanonical") &&
    sqlTableExists(conn, "kgXref"))
    {
    char query[512];
    sqlSafef(query, sizeof(query), "select chrom,chromStart,chromEnd,kgID from knownCanonical,kgXref "
	  "where kgXref.geneSymbol = '%s' and kgXref.kgId = knownCanonical.transcript;", term);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    if ((row = sqlNextRow(sr)) != NULL)
	{
        char buffer[4096];
        safef(buffer, sizeof buffer, "%s.knownGene", knownDatabase);
	singlePos(hgp, "GENCODE Genes", term, cloneString(buffer), row[3], row[3],
		  cloneString(row[0]), atoi(row[1]), atoi(row[2]));
	foundIt = TRUE;
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return foundIt;
}

static struct hgFindSpec *hfsFind(struct hgFindSpec *list, char *name)
/* Return first element of list that matches name. */
{
struct hgFindSpec *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(name, el->searchName))
        return el;
return NULL;
}

static void myLoadFindSpecs(char *db, struct searchCategory *categories, struct hgFindSpec **quickList, struct hgFindSpec **fullList)
/* Get all find specs where the search table or search name is what we want */
{
struct hgFindSpec *shortList = NULL, *longList = NULL;
struct dyString *clause = dyStringNew(0);
struct searchCategory *categ;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
struct slName *tbl, *tblList = hTrackDbList();
for (tbl = tblList; tbl != NULL; tbl = tbl->next)
    {
    dyStringClear(clause);
    char *tblName = replaceChars(tbl->name, "trackDb", "hgFindSpec");
    if (hTableExists(db, tblName))
        {
        sqlDyStringPrintf(clause, "select * from %s where searchName in (", tblName);
        for (categ = categories; categ != NULL; categ = categ->next)
            {
            sqlDyStringPrintf(clause, "'%s'", categ->id);
            if (categ->next)
                sqlDyStringPrintf(clause, ",");
            }
        sqlDyStringPrintf(clause, ") or searchTable in (");
        for (categ = categories; categ != NULL; categ = categ->next)
            {
            if (sameString(categ->id, "mrna"))
                sqlDyStringPrintf(clause, "'all_mrna'");
            else
                sqlDyStringPrintf(clause, "'%s'", categ->id);
            if (categ->next)
                sqlDyStringPrintf(clause, ",");
            }
        sqlDyStringPrintf(clause, ")");
        sr = sqlGetResult(conn, dyStringContents(clause));
        char **row = NULL;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            struct hgFindSpec *hfs = hgFindSpecLoad(row);
            if (hfs->shortCircuit)
                slAddHead(&shortList, hfs);
            else
                slAddHead(&longList, hfs);
            }
        sqlFreeResult(&sr);
        }
    }
hFreeConn(&conn);
dyStringFree(&clause);

if (quickList != NULL)
    {
    slSort(&shortList, hgFindSpecPriCmp);
    *quickList = shortList;
    }
else
    hgFindSpecFreeList(&shortList);
if (fullList != NULL)
    {
    slSort(&longList, hgFindSpecPriCmp);
    *fullList = longList;
    }
else
    hgFindSpecFreeList(&longList);
}

static bool subtrackEnabledInTdb(struct trackDb *subTdb)
/* Return TRUE unless the subtrack was declared with "subTrack ... off". */
{
bool enabled = TRUE;
char *words[2];
char *setting;
if ((setting = trackDbLocalSetting(subTdb, "parent")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            enabled = FALSE;
    }
else
    return subTdb->visibility != tvHide;

return enabled;
}

static bool isSubtrackVisible(struct cart *cart, struct trackDb *tdb)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 *  * "subTrack ... off"?  -- assumes composite track is visible. */
{
boolean overrideComposite = (NULL != cartOptionalString(cart, tdb->track));
bool enabledInTdb = subtrackEnabledInTdb(tdb);
char option[1024];
safef(option, sizeof(option), "%s_sel", tdb->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
return enabled;
}

static bool isParentVisible(struct cart *cart, struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;

if (!isParentVisible(cart, tdb->parent))
    return FALSE;

char *cartVis = cartOptionalString(cart, tdb->parent->track);
boolean vis;
if (cartVis != NULL)
    vis =  differentString(cartVis, "hide");
else if (tdbIsSuperTrack(tdb->parent))
    vis = tdb->parent->isShow;
else
    vis = tdb->parent->visibility != tvHide;
return vis;
}

static bool isTrackVisible(struct cart *cart, struct trackDb *tdb)
/* Is a track visible? */
{
boolean isVisible = FALSE;
if (tdb->parent == NULL)
    {
    char *cartVis = cartOptionalString(cart, tdb->track);
    if (cartVis == NULL)
        isVisible =  tdb->visibility != tvHide;
    else
        isVisible =  differentString(cartVis, "hide");
    }
else if (isParentVisible(cart, tdb) &&  isSubtrackVisible(cart, tdb))
    isVisible = TRUE;
return isVisible;
}

int cmpCategories(const void *a, const void *b)
/* Compare two categories for uniquifying */
{
struct searchCategory *categA = *(struct searchCategory **)a;
struct searchCategory *categB = *(struct searchCategory **)b;
return strcmp(categA->id, categB->id);
}

static struct searchableTrack *getSearchableTracks(struct cart *cart, char *database)
/* Return the list of all tracks with an hgFindSpec available */
{
if (trackHubDatabase(database))
    return NULL;
struct searchableTrack *ret = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct slName *tbl, *tblList = hTrackDbList();
for (tbl = tblList; tbl != NULL; tbl = tbl->next)
    {
    char *tdbName, *findSpecName;
    tdbName = tbl->name;
    findSpecName = replaceChars(tbl->name, "trackDb", "hgFindSpec");
    if (hTableExists(database, findSpecName))
        {
        char query[1024];
        sqlSafef(query, sizeof(query), "select distinct "
            "tableName,shortLabel,longLabel,searchDescription,priority "
            "from %s join %s on "
            "%s.searchTable=%s.tableName or "
            "%s.searchName=%s.tableName or "
            "%s.searchTable = concat('all_', %s.tableName) "
            "order by priority,shortLabel",
            findSpecName, tdbName, findSpecName, tdbName, findSpecName, tdbName, findSpecName, tdbName);
        struct sqlResult *sr = sqlGetResult(conn, query);
        char **row = NULL;
        struct trackDb *tdb = NULL;
        while ( (row = sqlNextRow(sr)) != NULL)
            {
            if ( (tdb = hashFindVal(hgFindTrackHash, row[0])) != NULL)
                {
                struct searchableTrack *track = NULL;
                AllocVar(track);
                track->track = cloneString(row[0]);
                track->shortLabel = cloneString(row[1]);
                track->longLabel = cloneString(row[2]);
                track->description = cloneString(row[3]);
                track->visibility = isTrackVisible(cart, tdb);
                track->priority = sqlDouble(row[4]);
                track->grp = tdb->grp;
                slAddHead(&ret, track);
                }
            }
        sqlFreeResult(&sr);
        }
    }
hFreeConn(&conn);
slReverse(&ret);
return ret;
}

static struct trackDb *hubCategoriesToTdbList(struct searchCategory *categories)
/* Make a list of trackDbs for the selected tracks */
{
struct trackDb *ret = NULL;
struct searchCategory *categ;
for (categ = categories; categ != NULL; categ = categ->next)
    {
    if (startsWith("hub_", categ->id))
        slAddHead(&ret, categ->tdb);
    }
return ret;
}

static struct searchCategory *searchCategoryFromTdb(struct trackDb *tdb, struct searchableTrack *searchTrack, int visibility)
/* Make a searchCategory from a leaf tdb, use searchCategory settings if possible, as they
 * have more accurate visibilities and labels */
{
struct searchCategory *category = NULL;
AllocVar(category);
category->tdb = tdb;
if (sameString(tdb->track, "mrna") || sameString(tdb->track, "est"))
    {
    char tableName[10];
    safef(tableName, sizeof(tableName), "all_%s", tdb->track);
    category->id = cloneString(tableName);
    }
else
    category->id = tdb->track;
category->name = searchTrack != NULL ? searchTrack->shortLabel : tdb->shortLabel;
category->visibility = searchTrack != NULL ? searchTrack->visibility: tdb->visibility;
if (visibility > 0) // for when tdb is from a hub track
    category->visibility = visibility;
category->priority = searchTrack != NULL ? searchTrack->priority : tdb->priority;
if (slCount(category->errors) == 0)
    {
    category->label = searchTrack != NULL ? searchTrack->shortLabel: tdb->shortLabel;
    category->description = searchTrack != NULL ? searchTrack->description: tdb->longLabel;
    category->groupName = searchTrack != NULL ? searchTrack->grp: tdb->grp;
    category->parents = NULL;
    while (tdb->parent)
        {
        slNameAddHead(&category->parents, tdb->parent->track);
        slNameAddHead(&category->parents, tdb->parent->shortLabel);
        tdb = tdb->parent;
        }
    if (category->parents)
        slReverse(&category->parents);
    }
return category;
}

struct trix *openStaticTrix(char *trixName)
/* Open up a trix file in hgFixed */
{
char trixPath[PATH_LEN];
safef(trixPath, sizeof(trixPath), "%s%s.ix", hgFixedTrix, trixName);
struct trix *ret = trixOpen(trixPath);
return ret;
}

static struct searchCategory *makeTrixCategory(char *indexName, char *database)
/* Fill out the fields for a category filter for the UI. */
{
struct searchCategory *category = NULL;
AllocVar(category);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (sameString(indexName, "publicHubs"))
        {
        category->id = "publicHubs";
        category->name = "publicHubs";
        category->label = "Public Hubs";
        category->description = "Track names and descriptions of public hubs";
        category->priority = 4.0;
        category->trix = openStaticTrix(publicHubsTrix);
        }
    else if (sameString(indexName, "helpDocs"))
        {
        category->id = "helpDocs";
        category->name = "helpDocs";
        category->label = "Help Pages";
        category->description = "Help documentation";
        category->visibility = 1;
        category->priority = 5.0;
        category->trix = openStaticTrix(helpDocsTrix);
        }
    else if (startsWith("trackDb", indexName))
        {
        category->id = "trackDb";
        category->name = "trackDb";
        category->visibility = 1;
        category->priority = 3.0;
        char trixPath[PATH_LEN];
        safef(trixPath, sizeof(trixPath), "%s Track Labels/Descriptions", database);
        category->label = cloneString(trixPath);
        category->description = "Track names or descriptions";
        safef(trixPath, sizeof(trixPath), "/gbdb/%s/trackDb.ix", database);
        category->trix = trixOpen(trixPath);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    slAddHead(&category->errors, slNameNew(errCatch->message->string));
return category;
}

static struct searchCategory *makeCategoryForTrack(struct trackDb *tdb, struct searchableTrack *searchTrack)
/* Make a searchCategory from a track. If the track is any type of container,
 * we will recurse down all the way to subtracks, as only leaf nodes have searchSpecs */
{
struct trackDb *sub;
struct searchCategory *ret = NULL;
if (tdb->subtracks)
    {
    for (sub = tdb->subtracks; sub != NULL; sub = sub->next)
        {
        if (sub->subtracks)
            {
            struct searchCategory *temp = makeCategoryForTrack(sub, searchTrack);
            if (temp)
                slAddHead(&ret, temp);
            }
        else
            {
            struct searchCategory *temp = searchCategoryFromTdb(sub, NULL, 0);
            if (temp)
                slAddHead(&ret, temp);
            }
        }
    }
else
    ret = searchCategoryFromTdb(tdb, searchTrack, 0);
return ret;
}

struct searchCategory *makeCategory(struct cart *cart, char *categName, struct searchableTrack *searchTrack, char *db,
                                    struct hash *groupHash)
/* Make a single searchCategory, unless the requested categName is a container
 * track or track group (for example all phenotype tracks), in which case we make
 * categories for each subtrack */
{
struct searchCategory *ret = NULL;

if (sameString(categName, "helpDocs"))
    ret = makeTrixCategory("helpDocs", NULL);
else if (sameString(categName, "publicHubs"))
    ret = makeTrixCategory("publicHubs", NULL);
else if (startsWith("trackDb", categName))
    ret = makeTrixCategory("trackDb", db);
else if (hashLookup(groupHash, categName) != NULL)
    {
    // add all tracks for this track grouping
    struct hashEl *hel, *helList = hashElListHash(hgFindTrackHash);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
        struct trackDb *tdb = hel->val;
        if (isTdbSearchable(tdb) && sameString(tdb->grp, categName))
            {
            struct searchCategory *temp = makeCategoryForTrack(tdb, searchTrack);
            if (temp)
                slAddHead(&ret, temp);
            }
        }
    }
else
    {
    // must be a track, ret will contain subtracks if necessary
    // sometimes duplicate tracks point to the same (eg knownGeneAlpha and knownGene)
    // this shouldn't happen anymore due to cartTrackDb.c:hashTdbNames() changes, but
    // just in case it pops up again this code will stay
    struct hashEl *hel = hashLookup(hgFindTrackHash, categName);
    while (hel != NULL)
        {
        struct trackDb *tdb = hel->val;
        if (tdb)
            {
            struct searchCategory *temp = makeCategoryForTrack(tdb, searchTrack);
            slAddHead(&ret, temp);
            }
        hel = hashLookupNext(hel);
        }
    }
return ret;
}

struct searchCategory *getCategsForNonDb(struct cart *cart, char *db, struct hash *groupHash)
/* Return the default categories for all databases */
{
struct searchCategory *ret = NULL;
struct searchCategory *helpDocCategory = makeCategory(cart, "helpDocs", NULL, db, groupHash);
if (helpDocCategory)
    slAddHead(&ret, helpDocCategory);
struct searchCategory *publicHubCategory = makeCategory(cart, "publicHubs", NULL, db, groupHash);
if (publicHubCategory)
    slAddHead(&ret, publicHubCategory);
char trackDbIndexName[2048];
safef(trackDbIndexName, sizeof(trackDbIndexName), "trackDb%s", db);
struct searchCategory *tdbCategory = makeCategory(cart, trackDbIndexName, NULL, db, groupHash);
if (tdbCategory)
    slAddHead(&ret, tdbCategory);
return ret;
}

static struct searchableTrack *makeGenbankSearchableTrack(struct trackDb *tdb, struct cart *cart)
{
struct searchableTrack *track = NULL;
AllocVar(track);
track->track = cloneString(tdb->track);
track->shortLabel = cloneString(tdb->shortLabel);
track->longLabel = cloneString(tdb->longLabel);
track->description = cloneString(tdb->longLabel);
track->visibility = isTrackVisible(cart, tdb);
track->priority = tdb->priority;
track->grp = tdb->grp;
return track;
}

struct searchCategory *getCategsForDatabase(struct cart *cart, char *db, struct hash *groupHash)
/* Get the track categories to search for a particular database */
{
struct searchCategory *ret = NULL;
struct trackDb *tdb = NULL;

struct searchableTrack *track = NULL, *searchableTracks = getSearchableTracks(cart, db);
for (track = searchableTracks; track != NULL; track = track->next)
    {
    struct searchCategory *trackCategory = makeCategory(cart, track->track, track, db, groupHash);
    if (trackCategory)
        {
        if (ret)
            slCat(&ret, trackCategory);
        else
            ret = trackCategory;
        }
    }

// only the all_mrna table will have a valid struct searchableTrack added, we need
// to make some for the rest of the searchable genbank mrna/est tracks:
char *table = NULL;
char **tables = mrnaTables;
while ((table = *tables++) != NULL)
    {
    if (!sameString(table, "all_mrna") && (tdb = hashFindVal(hgFindTrackHash, table)) != NULL)
        {
        struct searchableTrack *tmp = makeGenbankSearchableTrack(tdb, cart);
        struct searchCategory *category = makeCategory(cart, tmp->track, tmp, db, groupHash);
        if (category)
            slAddHead(&ret, category);
        }
    }
tables = estTables;
while ((table = *tables++) != NULL)
    {
    if ( (tdb = hashFindVal(hgFindTrackHash, table)) != NULL)
        {
        struct searchableTrack  *tmp = makeGenbankSearchableTrack(tdb, cart);
        struct searchCategory *category = makeCategory(cart, tmp->track, tmp, db, groupHash);
        if (category)
            slAddHead(&ret, category);
        }
    }

// add hub tracks to list
struct trackDb *hubList = hgFindTdbList;
hubList = getSearchableBigBeds(hubList);
for (tdb = hubList; tdb != NULL; tdb = tdb->next)
    {
    int visibility = isTrackVisible(cart, tdb);
    struct searchCategory *tmp = searchCategoryFromTdb(tdb, NULL, visibility);
    if (tmp)
        slAddHead(&ret, tmp);
    }
return ret;
}

struct searchCategory *getAllCategories(struct cart *cart, char *db, struct hash *groupHash)
/* Return all searchable stuff, both current db specific tracks, and things like hubs that searchable
 * no matter the current database */
{
struct searchCategory *ret = NULL;
struct searchCategory *tdbCategories = getCategsForDatabase(cart, db, groupHash);
if (tdbCategories)
    ret = tdbCategories;
struct searchCategory *staticCategs = getCategsForNonDb(cart, db, groupHash);
if (staticCategs)
    {
    if (ret)
        slCat(&ret, staticCategs);
    else
        ret = staticCategs;
    }
return ret;
}

static struct hash *hubLabelHash = NULL;

/* struct hubLabel: a helper struct for making links to hubs in the search result list */
struct hubLabel
    {
    char *shortLabel;
    char *longLabel;
    char *hubId;
    };


static void getLabelsForHubs()
/* Hash up the shortLabels, longLabels and hub_id for a hubUrl */
{
if (hubLabelHash != NULL)
    return;
hubLabelHash = hashNew(0);
struct sqlConnection *conn = hConnectCentral();
char **row;
struct sqlResult *sr;
char query[2048];
sqlSafef(query, sizeof(query), "select hp.hubUrl, hp.shortLabel, hp.longLabel, concat('hub_',id) from hubPublic hp join hubStatus hs on hp.hubUrl=hs.hubUrl");
sr = sqlGetResult(conn, query);
while ( (row = sqlNextRow(sr)) != NULL)
    {
    struct hubLabel *label = NULL;
    AllocVar(label);
    label->shortLabel = cloneString(row[1]);
    label->longLabel = cloneString(row[2]);
    label->hubId = cloneString(row[3]);
    char *hubUrl = cloneString(row[0]);
    hashAdd(hubLabelHash, hubUrl, label);
    }
hDisconnectCentral(&conn);
}

static struct hubLabel *getLabelForHub(char *hubUrl)
/* Look up the shortLabel, longLabel, and hub_id for a hubUrl */
{
if (!hubLabelHash)
    getLabelsForHubs();
return (struct hubLabel *)hashFindVal(hubLabelHash, hubUrl);
}

static boolean fillOutTrackDbHgPos(struct hgPos *this, struct trixSearchResult *tsr)
{
boolean foundIt = FALSE;
struct trackDb *tdb = (struct trackDb *)hashFindVal(hgFindTrackHash, this->name);
if (tdb)
    {
    struct dyString *tdbLabels = dyStringNew(0);
    dyStringPrintf(tdbLabels, "%s:%s:%s", tsr->itemId, tdb->shortLabel, tdb->longLabel);
    this->name = dyStringCannibalize(&tdbLabels);
    foundIt = TRUE;
    }
return foundIt;
}

static boolean fillOutPublicHubsHgPos(struct hgPos *this, struct trixSearchResult *tsr)
{
boolean foundIt = FALSE;
char *itemId[5];
int numItems = chopString(tsr->itemId, ":", itemId, ArraySize(itemId));
struct dyString *hubLabel = dyStringNew(0);
char hubUrl[PATH_LEN];
safef(hubUrl, sizeof(hubUrl), "%s:%s", itemId[0], itemId[1]);
struct hubLabel *label = getLabelForHub(hubUrl);
if (!label)
    return foundIt;
else
    foundIt = TRUE;
char *db = "";
struct dyString *track = dyStringNew(0);
if (numItems > 2)
    db = itemId[2] != NULL ? itemId[2] : "";
if (numItems > 3)
    dyStringPrintf(track, "%s_%s", label->hubId, itemId[3]);
dyStringPrintf(hubLabel, "%s:%s:%s:%s:%s", hubUrl, db, dyStringCannibalize(&track), label->shortLabel, label->longLabel);
this->name = dyStringCannibalize(&hubLabel);
return foundIt;
}

static boolean doTrixQuery(struct searchCategory *category, char *searchTerm, struct hgPositions *hgp, char *database, boolean measureTiming)
/* Get a trix search result and potentially snippets for an hgFixed trix index.
 * TODO: return an error message if there is a problem with the trix index or snippet index */
{
long startTime = clock1000();
boolean ret = FALSE;
char *lowered = cloneString(searchTerm);
char *keyWords[16];
int keyCount;
tolowers(lowered);
keyCount = chopLine(lowered, keyWords);
// TODO: let the user control this:
int maxReturn = SNIPPET_LIMIT;
struct trixSearchResult *tsrList = NULL;
if (category->trix)
    {
    tsrList = trixSearch(category->trix, keyCount, keyWords, tsmExpand);
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        initSnippetIndex(category->trix);
    errCatchEnd(errCatch);
    // silently return if there was a problem opening the snippet index
    if (errCatch->gotError || errCatch->gotWarning)
        return FALSE;
    errCatchFree(&errCatch);
    }
struct trixSearchResult *tsr = NULL;
int len = 0;
struct hgPosTable *table = NULL;
AllocVar(table);
table->searchTime = -1;
table->name = cloneString(category->name);
table->description = cloneString(category->description);
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    if (startsWith(category->name,"publicHubs"))
        {
        // Check that this public hubs result is for our current database
        char *itemId[5];
        int numItems = chopString(cloneString(tsr->itemId), ":", itemId, ArraySize(itemId));
        if (numItems <= 2 || isEmpty(itemId[2]) || !sameString(itemId[2], database))
            continue;
        }
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        addSnippetForResult(tsr, category->trix);
        }
    errCatchEnd(errCatch);
    // silently return if there was a problem getting a single snippet, there is
    // probably a data error with the rest of the index if so
    if (errCatch->gotError || errCatch->gotWarning)
        return FALSE;
    errCatchFree(&errCatch);
    struct hgPos *this = NULL;
    AllocVar(this);
    this->name = tsr->itemId;
    this->description = tsr->snippet;
    if (startsWith(category->name, "trackDb"))
        {
        boolean addedTdbFields = fillOutTrackDbHgPos(this, tsr);
        if (!addedTdbFields)
            continue;
        }
    if (sameString(category->name, "publicHubs"))
        {
        boolean addedHubFields = fillOutPublicHubsHgPos(this, tsr);
        if (!addedHubFields)
            continue;
        }
    slAddHead(&table->posList, this);

    len++;
    if (len > maxReturn)
        break;
    }

if (table->posList != NULL)
    {
    slReverse(&table->posList);
    if (measureTiming)
        table->searchTime = clock1000() - startTime;
    slAddHead(&hgp->tableList, table);
    ret = TRUE;
    }
return ret;
}

static boolean userDefinedSearch(char *db, char *term, int limitResults, struct cart *cart,
                            struct hgPositions *hgp, struct searchCategory *categories, boolean multiTerm, boolean measureTiming)
/* If a search type(s) is specified in the cart, perform that search.
 * If the search is successful, fill in hgp and return TRUE. */
{
boolean foundIt = FALSE;
struct hash *foundSpecHash = hashNew(0);
struct hgFindSpec *shortList = NULL, *longList = NULL;
struct trackDb *hubCategoryList = NULL;
// get all the lists of what to query:
if (!trackHubDatabase(db))
    {
    if (categories)
        myLoadFindSpecs(db, categories, &shortList, &longList);
    else
        hgFindSpecGetAllSpecs(db, &shortList, &longList);
    }
// lastly search any included track hubs, or in the case of an assembly hub, any of the tracks
hubCategoryList = hubCategoriesToTdbList(categories);
struct hgFindSpec *hfs;
for (hfs = shortList; hfs != NULL; hfs = hfs->next)
    {
    if (hashFindVal(foundSpecHash, hfs->searchTable))
        continue; // we've already found a hit for this table, don't search it again
    boolean foundSpec = hgFindUsingSpec(cart, db, hfs, term, limitResults, hgp, FALSE, 0, 0, multiTerm, measureTiming);
    if (foundSpec)
        {
        hashAdd(foundSpecHash, hfs->searchTable, hfs->searchTable);
        }
    foundIt |= foundSpec;

    // for multiTerm searches (like '15q11;15q13'), each individual component
    // must resolve to a single position, so break once we find the first match
    if (multiTerm && foundSpec)
        break;
    }

if (!(multiTerm) || (multiTerm && !foundIt))
    {
    for (hfs = longList; hfs != NULL; hfs = hfs->next)
        {
        if (hashFindVal(foundSpecHash, hfs->searchTable) != NULL)
            continue;
        foundIt |= hgFindUsingSpec(cart, db, hfs, term, limitResults, hgp, FALSE, 0, 0, multiTerm, measureTiming);
        }
    // lastly search any included track hubs, or in the case of an assembly hub, any of the tracks
    if (hubCategoryList)
        foundIt |= findBigBedPosInTdbList(cart, db, hubCategoryList, term, hgp, NULL, measureTiming);
    }

// multiTerm searches must resolve to a single range on a chromosome, so don't
// do these non positional searches if a multiTerm was requested
if (!multiTerm)
    {
    getLabelsForHubs();
    struct searchCategory *category;
    for (category = categories; category != NULL; category = category->next)
        {
        if (startsWith("trackDb", category->id)
                || sameString(category->id, "helpDocs")
                || sameString(category->id, "publicHubs"))
            {
            foundIt |= doTrixQuery(category, term, hgp, db, measureTiming);
            }
        }
    }

return foundIt;
}

static boolean singleSearch(char *db, char *term, int limitResults, struct cart *cart,
                            struct hgPositions *hgp, boolean measureTiming)
/* If a search type is specified in the CGI line (not cart), perform that search. 
 * If the search is successful, fill in hgp as a single-pos result and return TRUE. */
{
char *search = cgiOptionalString("singleSearch");
if (search == NULL)
    return FALSE;

cartRemove(cart, "singleSearch");
boolean foundIt = FALSE;
if (sameString(search, "knownCanonical"))
    foundIt = searchKnownCanonical(db, term, hgp);
else
    {
    struct hgFindSpec *shortList = NULL, *longList = NULL;
    hgFindSpecGetAllSpecs(db, &shortList, &longList);
    struct hgFindSpec *hfs = hfsFind(shortList, search);
    if (hfs == NULL)
	hfs = hfsFind(longList, search);
    if (hfs != NULL)
	foundIt = hgFindUsingSpec(cart, db, hfs, term, limitResults, hgp, FALSE, 0,0, FALSE, measureTiming);
    else
	warn("Unrecognized singleSearch=%s in URL", search);
    }
if (foundIt)
    {
    fixSinglePos(hgp);
    if (cart != NULL)
        cartSetString(cart, "hgFind.matches", hgp->tableList->posList->browserName);
    }
return foundIt;
}

// a little data structure for combining multiple transcripts that resolve
// to the same hgvs change. This struct can be used to fill out a struct hgPos
struct hgvsHelper
    {
    struct hgvsHelper *next;
    char *chrom; // chromosome name of position
    int chromStart; // start of position
    int chromEnd; // end of position
    struct slName *validTranscripts; // valid transcripts/protein accessions for this position
    char *label; // corresponding hgvs term
    char *table; // type of match, LRG, NCBI, etc
    boolean mapError; // does this hgvs mapping result in a map error?
    };

static boolean matchesHgvs(struct cart *cart, char *db, char *term, struct hgPositions *hgp,
                            boolean measureTiming)
/* Return TRUE if the search term looks like a variant encoded using the HGVS nomenclature
 * See http://varnomen.hgvs.org/
 * If search term is a pseudo hgvs term like GeneName AminoAcidPosition (RUNX2 Arg155) and
 * matches more than one transcript, fill out the hgp with the potential matches so the user
 * can choose where to go, otherwise return a singlePos */
{
boolean foundIt = FALSE;
long startTime = clock1000();
struct hgvsVariant *hgvsList = hgvsParseTerm(term);
if (hgvsList == NULL)
    hgvsList = hgvsParsePseudoHgvs(db, term);
if (hgvsList)
    {
    struct hgvsVariant *hgvs = NULL;
    int hgvsListLen = slCount(hgvsList);
    struct hgPosTable *table;
    AllocVar(table);
    table->description = "HGVS";
    table->searchTime = -1;
    int padding = 5;
    int mapErrCnt = 0;
    struct dyString *dyWarn = dyStringNew(0);
    struct hgvsHelper *helper = NULL;
    struct hash *uniqHgvsPos = hashNew(0);
    struct dyString *chromPosIndex = dyStringNew(0);
    struct dyString *allWarnings = dyStringNew(0);
    for (hgvs = hgvsList; hgvs != NULL; hgvs = hgvs->next)
        {
        dyStringClear(dyWarn);
        dyStringClear(chromPosIndex);
        char *pslTable = NULL;
        struct bed *mapping = hgvsValidateAndMap(hgvs, db, term, dyWarn, &pslTable);
        if (dyStringLen(dyWarn) > 0)
            mapErrCnt++;
        if (mapping)
            {
            char *trackTable;
            if (isEmpty(pslTable))
                trackTable = "chromInfo";
            else if (startsWith("lrg", pslTable))
                trackTable = "lrgTranscriptAli";
            else if (startsWith("wgEncodeGencode", pslTable))
                trackTable = pslTable;
            else if (sameString("ncbiRefSeqPsl", pslTable))
                {
                if (startsWith("NM_", hgvs->seqAcc) || startsWith("NR_", hgvs->seqAcc) ||
                    startsWith("NP_", hgvs->seqAcc) || startsWith("YP_", hgvs->seqAcc))
                    trackTable = "ncbiRefSeqCurated";
                else if (startsWith("XM_", hgvs->seqAcc) || startsWith("XR_", hgvs->seqAcc) ||
                         startsWith("XP_", hgvs->seqAcc))
                    trackTable = "ncbiRefSeqPredicted";
                else
                    trackTable = "ncbiRefSeq";
                }
            else if (sameString("ncbiRefSeqPslHistorical", pslTable))
                trackTable = "ncbiRefSeqHistorical";
            else
                trackTable = "refGene";
            dyStringPrintf(chromPosIndex, "%s%s%d%d", trackTable, mapping->chrom,
                    mapping->chromStart-padding, mapping->chromEnd+padding);
            if ((helper = hashFindVal(uniqHgvsPos, chromPosIndex->string)) != NULL)
                {
                slNameAddHead(&helper->validTranscripts, hgvs->seqAcc);
                }
            else
                {
                AllocVar(helper);
                helper->chrom = mapping->chrom;
                helper->chromStart = mapping->chromStart;
                helper->chromEnd = mapping->chromEnd;
                helper->validTranscripts = slNameNew(hgvs->seqAcc);
                helper->label = cloneString(term);
                helper->table = trackTable;
                hashAdd(uniqHgvsPos, chromPosIndex->string, helper);
                }
            if (dyStringLen(dyWarn) > 0)
                {
                helper->mapError = TRUE;
                dyStringPrintf(allWarnings, "%s%s", dyStringLen(allWarnings) > 0 ? "\n" : "", dyStringContents(dyWarn));
                }
            }
        }
    if (mapErrCnt < hgvsListLen)
        // at least one of the hgvs terms mapped sucessfully, so we can go to that spot
        // or let the user pick a location
        {
        int numPositions = 0;
        struct hashEl *hel, *helList= hashElListHash(uniqHgvsPos);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            helper = (struct hgvsHelper *)hel->val;
            if (!helper->mapError)
                {
                if (hgp->tableList == NULL)
                    hgp->tableList = table;
                foundIt = TRUE;
                table->name = helper->table;
                struct hgPos *pos;
                AllocVar(pos);
                pos->chrom = helper->chrom;
                pos->chromStart = helper->chromStart - padding;
                pos->chromEnd = helper->chromEnd + padding;
                pos->name = slNameListToString(helper->validTranscripts, '/');
                pos->description = cloneString(helper->label);
                pos->browserName = "";
                slAddHead(&table->posList, pos);
                // highlight the mapped bases to distinguish from padding
                hgp->tableList->posList->highlight = addHighlight(db, helper->chrom,
                                                        helper->chromStart, helper->chromEnd);
                numPositions++;
                }
            }
        if (numPositions > 1)
            table->description = "HGVS search resulted in multiple positions, please select a transcript below";
        }
    else
        // all of the positions mapped incorrectly, so the term was bad. However, we may
        // be able to still go to a general area around the term, so build that, warn the
        // user about their bad search term, and warn that this is not an exactly correct position
        // NOTE: There is a bug here in general, in that when mapping an hgvs term we don't
        // consider alternate haplotypes, and thus below we will always get at least some range
        // on the same chromosome within a gene, but if the mapping code were to change in the
        // future, we might end up with some weird coordinates
        {
        struct hashEl *hel, *helList= hashElListHash(uniqHgvsPos);
        if (helList)
            {
            if (hgp->tableList == NULL)
                hgp->tableList = table;
            foundIt = TRUE;
            struct hgPos *pos;
            AllocVar(pos);
            char *chrom = NULL;
            int spanStart = INT_MAX, spanEnd = 0;
            for (hel = helList; hel != NULL; hel = hel->next)
                {
                helper = (struct hgvsHelper *)hel->val;
                chrom = helper->chrom;
                spanStart = helper->chromStart < spanStart ? helper->chromStart : spanStart;
                spanEnd = helper->chromEnd > spanEnd ? helper->chromEnd : spanEnd;
                table->name = helper->table;
                }
            pos->chrom = cloneString(chrom);
            pos->chromStart = spanStart-padding;
            pos->chromEnd = spanEnd + padding;
            pos->name = "Approximate area";
            pos->description = term;
            pos->browserName = term;
            slAddHead(&table->posList, pos);
            // highlight the 'mapped' bases to distinguish from padding
            hgp->tableList->posList->highlight = addHighlight(db, helper->chrom, spanStart, spanEnd);
            warn("%s", dyStringContents(allWarnings));
            warn("Sorry, couldn't locate %s, moving to general location", term);
            }
        else
            warn("%s", dyStringContents(dyWarn));
        }
    dyStringFree(&dyWarn);
    dyStringFree(&allWarnings);
    if (measureTiming && hgp && hgp->tableList)
        table->searchTime = clock1000() - startTime;
    }
return foundIt;
}

struct hgPositions *hgPositionsFind(char *db, char *term, char *extraCgi,
	char *hgAppNameIn, struct cart *cart, boolean multiTerm, boolean measureTiming, struct searchCategory *categories)
/* Return container of tracks and positions (if any) that match term. */
{
struct hgPositions *hgp = NULL, *hgpItem = NULL;
regmatch_t substrs[7];
boolean canonicalSpec = FALSE;
boolean gbrowserSpec = FALSE;
boolean lengthSpec = FALSE;
boolean singleBaseSpec = FALSE;
boolean gnomadVarSpec = FALSE;
boolean gnomadRangeSpec = FALSE;
boolean relativeFlag = FALSE;
int relStart = 0, relEnd = 0;

hgAppName = hgAppNameIn;

// Exhaustive searches can lead to timeouts on CGIs (#11626).
// However, hgGetAnn requires exhaustive searches (#11665).
// So... set a non-exhaustive search limit on all except hgGetAnn.
// NOTE: currently non-exhaustive search limits are only applied to findMrnaKeys
int limitResults = NONEXHAUSTIVE_SEARCH_LIMIT;
if (sameString(hgAppNameIn,"hgGetAnn"))
    limitResults = EXHAUSTIVE_SEARCH_REQUIRED;

AllocVar(hgp);
hgp->useAlias = FALSE;
term = trimSpaces(term);
if(isEmpty(term))
    return hgp;

hgp->query = cloneString(term);
hgp->database = db;
if (extraCgi == NULL)
    extraCgi = "";
hgp->extraCgi = cloneString(extraCgi);

if (singleSearch(db, term, limitResults, cart, hgp, measureTiming))
    return hgp;

if (categories != NULL)
    {
    char *originalTerm = term;
    if (hgOfficialChromName(db, term) != NULL) // this mangles the term
        {
        char *chrom;
        int start, end;

        hgParseChromRange(db, term, &chrom, &start, &end);
        if (relativeFlag)
            {
            int chromSize = end;
            end = start + relEnd;
            start = start + relStart;
            if (end > chromSize)
                end = chromSize;
            if (start < 0)
                start = 0;
            }
        singlePos(hgp, "Chromosome Range", NULL, "chromInfo", originalTerm,
              "", chrom, start, end);
        }
    else if (!matchesHgvs(cart, db, term, hgp, measureTiming))
        userDefinedSearch(db, term, limitResults, cart, hgp, categories, multiTerm, measureTiming);
    slReverse(&hgp->tableList);
    if (multiTerm)
        collapseSamePos(hgp);
    fixSinglePos(hgp);
    if (cart && hgp->singlePos && isNotEmpty(hgp->singlePos->highlight))
        cartSetString(cart, "addHighlight", hgp->singlePos->highlight);
    if (hgp->posCount > 0)
        return hgp;
    else
        // if categories was passed in we should explicitly return no results
        // if there weren't any
        return NULL;
    }

/* Allow any search term to end with a :Start-End range -- also support stuff 
 * pasted in from BED (chrom start end) or SQL query (chrom | start | end).  
 * If found, strip it off and remember the start and end. */
char *originalTerm = term;
if ((canonicalSpec = 
        regexMatchSubstrNoCase(term, canonicalRangeExp, substrs, ArraySize(substrs))) ||
    (gbrowserSpec = 
        regexMatchSubstrNoCase(term, gbrowserRangeExp, substrs, ArraySize(substrs))) ||
    (lengthSpec = 
        regexMatchSubstrNoCase(term, lengthRangeExp, substrs, ArraySize(substrs))) ||
    regexMatchSubstrNoCase(term, bedRangeExp, substrs, ArraySize(substrs)) ||
    (singleBaseSpec =
	regexMatchSubstrNoCase(term, singleBaseExp, substrs, ArraySize(substrs))) ||
    (gnomadVarSpec =
	regexMatchSubstrNoCase(term, gnomadVarExp, substrs, ArraySize(substrs))) ||
    (gnomadRangeSpec =
	regexMatchSubstrNoCase(term, gnomadRangeExp, substrs, ArraySize(substrs))) ||
    regexMatchSubstrNoCase(term, sqlRangeExp, substrs, ArraySize(substrs)))
    {
    term = cloneString(term);
    if (gnomadVarSpec || gnomadRangeSpec)
        {
        /* Since we got a match, substrs[1] is the chrom/term, [4] is relStart,
         * [5] is relEnd or an allele. ([0] is all.) */
        term[substrs[1].rm_eo] = 0;
        eraseTrailingSpaces(term);
        term[substrs[4].rm_eo] = 0;
        relStart = atoi(term+substrs[4].rm_so);
        term[substrs[5].rm_eo] = 0;
        if (gnomadVarSpec)
            singleBaseSpec = TRUE; // relEnd = relStart, relStart -= 1
        else
            relEnd = atoi(term+substrs[5].rm_so);
        }
    else
        {
        /* Since we got a match, substrs[1] is the chrom/term, [2] is relStart,
         * [3] is relEnd. ([0] is all.) */
        term[substrs[1].rm_eo] = 0;
        eraseTrailingSpaces(term);
        term[substrs[2].rm_eo] = 0;
        relStart = atoi(stripCommas(term+substrs[2].rm_so));
        term[substrs[3].rm_eo] = 0;
        }
    if (singleBaseSpec)
        {
        relEnd   = relStart;
        relStart--;
        }
    else
        relEnd   = atoi(stripCommas(term+substrs[3].rm_so));
    if (lengthSpec)
        relEnd += relStart;
    if (relStart > relEnd)
        {
        int tmp  = relStart;
        relStart = relEnd;
        relEnd   = tmp;
        }
    if (canonicalSpec || gbrowserSpec || lengthSpec)
        relStart--;
    relativeFlag = TRUE;
    }
term = cloneString(term); // because hgOfficialChromName mangles it

if (hgOfficialChromName(db, term) != NULL) // this mangles the term
    {
    char *chrom;
    int start, end;

    hgParseChromRange(db, term, &chrom, &start, &end);
    if (relativeFlag)
	{
	int chromSize = end;
	end = start + relEnd;
	start = start + relStart;
	if (end > chromSize)
	    end = chromSize;
	if (start < 0)
	    start = 0;
	}
    singlePos(hgp, "Chromosome Range", NULL, "chromInfo", originalTerm,
	      "", chrom, start, end);
    }
else if (!matchesHgvs(cart, db, term, hgp, measureTiming))
    {
    struct hgFindSpec *shortList = NULL, *longList = NULL;
    struct hgFindSpec *hfs;
    boolean done = FALSE;

    // Disable singleBaseSpec for any term that is not hgOfficialChromName
    // because that mangles legitimate IDs that are [A-Z]:[0-9]+.
    if (singleBaseSpec)
	{
	singleBaseSpec = relativeFlag = FALSE;
	term = cloneString(originalTerm);  // restore original term
	relStart = relEnd = 0;
	}

    if (!trackHubDatabase(db))
	hgFindSpecGetAllSpecs(db, &shortList, &longList);
    if ((cart == NULL) || (cartOptionalString(cart, "noShort") == NULL))
        {
        hgp->shortCircuited = TRUE;
        for (hfs = shortList;  hfs != NULL;  hfs = hfs->next)
            {
            if (hgFindUsingSpec(cart, db, hfs, term, limitResults, hgp, relativeFlag, relStart, relEnd,
                                multiTerm, measureTiming))
                {
                done = TRUE;
                if (! hgFindSpecSetting(hfs, "semiShortCircuit"))
                    break;
                }
            }
        }
    else
        cartRemove(cart, "noShort");
    if (! done)
	{
        hgp->shortCircuited = FALSE;
	for (hfs = longList;  hfs != NULL;  hfs = hfs->next)
	    {
	    hgFindUsingSpec(cart, db, hfs, term, limitResults, hgp, relativeFlag, relStart, relEnd,
			    multiTerm, measureTiming);
	    }
	/* Lowe lab additions -- would like to replace these with specs, but 
	 * will leave in for now. */
	if (!trackHubDatabase(db))
	    findTigrGenes(db, term, hgp);

	trackHubFindPos(cart, db, term, hgp, measureTiming);
	}
    hgFindSpecFreeList(&shortList);
    hgFindSpecFreeList(&longList);
    if (cart != NULL)
        {
        if(hgpMatchNames == NULL)
            hgpMatchNames = dyStringNew(256);
        dyStringClear(hgpMatchNames);
        int matchCount = 0;
        for(hgpItem = hgp; hgpItem != NULL; hgpItem = hgpItem->next)
            {
            struct hgPosTable *hpTable = NULL;
            for(hpTable = hgpItem->tableList; hpTable != NULL; hpTable = hpTable->next)
                {
                struct hgPos *pos = NULL;
                for(pos = hpTable->posList; pos != NULL; pos = pos->next)
                    {
                    if (limitResults != EXHAUSTIVE_SEARCH_REQUIRED && matchCount++ >= limitResults)
                        break;
                    dyStringPrintf(hgpMatchNames, "%s,", pos->browserName);
                    }
                }
            }
        cartSetString(cart, "hgFind.matches", hgpMatchNames->string);
        }
    }
slReverse(&hgp->tableList);
if (multiTerm)
    collapseSamePos(hgp);
fixSinglePos(hgp);
if (cart && hgp->singlePos && isNotEmpty(hgp->singlePos->highlight))
    cartSetString(cart, "addHighlight", hgp->singlePos->highlight);
return hgp;
}


void hgPositionsHelpHtmlCart(struct cart *cart, char *organism, char *database)
/* Display contents of dbDb.htmlPath for database, or print an HTML comment 
 * explaining what's missing. */
{
char *htmlPath = hHtmlPath(database);
char *htmlString = NULL;
size_t htmlStrLength = 0;

if (strstrNoCase(organism, "zoo")) 
    webNewSection("About the NISC Comparative Sequencing Program Browser");
else
    webNewSection("%s Genome Browser &ndash; %s assembly"
		  "  <A HREF=\"%s?%s=%s&chromInfoPage=\">(sequences)</A>",
		  trackHubSkipHubName(organism), 
		  trackHubSkipHubName(database),
		  hgTracksName(), cartSessionVarName(), cartSessionId(cart));

if (htmlPath != NULL) 
    {
    if (fileExists(htmlPath))
	readInGulp(htmlPath, &htmlString, &htmlStrLength);
    else if (   startsWith("http://" , htmlPath) ||
		startsWith("https://", htmlPath) ||
		startsWith("ftp://"  , htmlPath))
	{
	struct lineFile *lf = udcWrapShortLineFile(htmlPath, NULL, 256*1024);
	htmlString =  lineFileReadAll(lf);
	htmlStrLength = strlen(htmlString);
	lineFileClose(&lf);
	}
    }

if (htmlStrLength > 0)
    {
    puts(htmlString);
    freeMem(htmlString);
    freeMem(htmlPath);
    }
else
    {
    printf("<H2>%s</H2>\n", trackHubSkipHubName(organism));
    if (htmlPath == NULL || htmlPath[0] == 0)
	printf("\n<!-- No dbDb.htmlPath for %s -->\n", database);
    else
	printf("\n<!-- Couldn't get contents of %s -->\n", htmlPath);
   } 
}
