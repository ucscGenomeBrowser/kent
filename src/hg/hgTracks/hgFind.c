/* hgFind.c - Find things in human genome annotations. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "genePred.h"
#include "glDbRep.h"
#include "bed.h"
#include "cytoBand.h"
#include "mapSts.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "knownInfo.h"
#include "hgFind.h"
#include "refLink.h"

void hgPositionsFree(struct hgPositions **pEl)
/* Free up hgPositions. */
{
struct hgPositions *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->query);
    freeMem(el->extraCgi);
    hgPosTableFreeList(&el->tableList);
    freez(pEl);
    }
}

void hgPositionsFreeList(struct hgPositions **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPositions *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPositionsFree(&el);
    }
*pList = NULL;
}


static void singlePos(struct hgPositions *hgp, char *tableDescription, char *posDescription,
	char *posName, char *chrom, int start, int end)
/* Fill in pos for simple case single position. */
{
struct hgPosTable *table;
struct hgPos *pos;

AllocVar(table);
AllocVar(pos);

hgp->tableList = table;
table->posList = pos;
table->name = cloneString(tableDescription);
pos->chrom = chrom;
pos->chromStart = start;
pos->chromEnd = end;
pos->name = cloneString(posName);
pos->description = cloneString(posDescription);
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

static boolean isContigName(char *contig)
/* Return TRUE if a FPC contig name. */
{
return startsWith("ctg", contig);
}

static void findContigPos(char *contig, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct ctgPos *ctgPos;
dyStringPrintf(query, "select * from ctgPos where contig = '%s'", contig);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find contig %s", contig);
ctgPos = ctgPosLoad(row);
*retChromName = hgOfficialChromName(ctgPos->chrom);
*retWinStart = ctgPos->chromStart;
*retWinEnd = ctgPos->chromEnd;
ctgPosFree(&ctgPos);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static char *startsWithShortHumanChromName(char *chrom)
/* Return "cannonical" name of chromosome or NULL
 * if not a chromosome.  This expects no 'chr' in name. */
{
int num;
char buf[64];
char c = chrom[0];

if (c == 'x' || c == 'X' || c == 'Y' || c == 'y')
    {
    sprintf(buf, "chr%c", toupper(c));
    return hgOfficialChromName(buf);
    }
if (!isdigit(chrom[0]))
    return NULL;
num = atoi(chrom);
if (num < 1 || num > 22)
    return NULL;
sprintf(buf, "chr%d", num);
return hgOfficialChromName(buf);
}

static struct cytoBand *loadAllBands()
/* Load up all bands from database. */
{
struct cytoBand *list = NULL, *el;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, "select * from cytoBand");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = cytoBandLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
hFreeConn(&conn);
return list;
}

static struct cytoBand *bandList = NULL;

void hgFindChromBand(char *chromosome, char *band, int *retStart, int *retEnd)
/* Return start/end of band in chromosome. */
{
struct cytoBand *chrStart = NULL, *chrEnd = NULL, *cb;
int start = 0, end = 500000000;
boolean anyMatch;
char choppedBand[64], *s, *e;

if (bandList == NULL)
    bandList = loadAllBands();

/* Find first band in chromosome. */
for (cb = bandList; cb != NULL; cb = cb->next)
    {
    if (sameString(cb->chrom, chromosome))
        {
	chrStart = cb;
	break;
	}
    }
if (chrStart == NULL)
    errAbort("Couldn't find chromosome %s in band list", chromosome);

/* Find last band in chromosome. */
for (cb = chrStart->next; cb != NULL; cb = cb->next)
    {
    if (!sameString(cb->chrom, chromosome))
        break;
    }
chrEnd = cb;

if (sameWord(band, "cen"))
    {
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
        {
	if (cb->name[0] == 'p')
	    start = cb->chromEnd - 500000;
	else if (cb->name[0] == 'q')
	    {
	    end = cb->chromStart + 500000;
	    break;
	    }
	}
    *retStart = start;
    *retEnd = end;
    return;
    }
else if (sameWord(band, "qter"))
    {
    *retStart = *retEnd = hChromSize(chromosome);
    *retStart -= 1000000;
    return;
    }
/* Look first for exact match. */
for (cb = chrStart; cb != chrEnd; cb = cb->next)
    {
    if (sameWord(cb->name, band))
        {
	*retStart = cb->chromStart;
	*retEnd = cb->chromEnd;
	return;
	}
    }

/* See if query is less specific.... */
strcpy(choppedBand, band);
for (;;) 
    {
    anyMatch = FALSE;
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
	{
	if (startsWith(choppedBand, cb->name))
	    {
	    if (!anyMatch)
		{
		anyMatch = TRUE;
		start = cb->chromStart;
		}
	    end = cb->chromEnd;
	    }
	}
    if (anyMatch)
	{
	*retStart = start;
	*retEnd = end;
	return;
	}
    s = strrchr(choppedBand, '.');
    if (s == NULL)
	errAbort("Couldn't find anything like band '%s'", band);
    else
	{
	e = choppedBand + strlen(choppedBand) - 1;
	*e = 0;
	if (e[-1] == '.')
	   e[-1] = 0;
        warn("Band %s%s is at higher resolution than data, chopping to %s%s",
	    chromosome+3, band, chromosome+3, choppedBand);
	}
    }
}

boolean hgIsCytoBandName(char *spec, char **retChromName, char **retBandName)
/* Return TRUE if spec is a cytological band name including chromosome short name.  
 * Returns chromosome chrN name and band (with chromosome stripped off). */
{
char *fullChromName, *shortChromName;
int len;
int dotCount = 0;
char *s, c;

/* First make sure spec is in format to be a band name. */
if ((fullChromName = startsWithShortHumanChromName(spec)) == NULL)
    return FALSE;
shortChromName = skipChr(fullChromName);
len = strlen(shortChromName);
spec += len;
c = spec[0];
if (c != 'p' && c != 'q')
    return FALSE;
if (!isdigit(spec[1]))
    return FALSE;

/* Make sure rest is digits with maybe one '.' */
s = spec+2;
while ((c = *s++) != 0)
    {
    if (c == '.')
        ++dotCount;
    else if (!isdigit(c))
        return FALSE;
    }
if (dotCount > 1)
    return FALSE;
*retChromName = fullChromName;
*retBandName = spec;
return TRUE;
}

boolean hgFindCytoBand(char *spec, char **retChromName, int *retWinStart, int *retWinEnd)
/* Return position associated with cytological band if spec looks to be in that form. */
{
char *bandName;

if (!hgIsCytoBandName(spec, retChromName, &bandName))
     return FALSE;
hgFindChromBand(*retChromName, bandName, retWinStart, retWinEnd);
return TRUE;
}

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

boolean hgFindClonePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Return clone position. */
{
if (!isAccForm(spec))
    return FALSE;
else
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    struct dyString *query = newDyString(256);
    char **row;
    boolean ok = FALSE;
    struct clonePos *clonePos;
    dyStringPrintf(query, "select * from clonePos where name like '%s%%'", spec);
    sr = sqlGetResult(conn, query->string);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	clonePos = clonePosLoad(row);
	*retChromName = hgOfficialChromName(clonePos->chrom);
	*retWinStart = clonePos->chromStart;
	*retWinEnd = clonePos->chromEnd;
	clonePosFree(&clonePos);
	ok = TRUE;
	}
    freeDyString(&query);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return ok;
    }
}

static char *mrnaType(char *acc)
/* Return "mrna" or "est" if acc is mRNA, otherwise NULL. */
{
static char typeBuf[16];
char *type;
struct sqlConnection *conn = hAllocConn();
char query[128];
sprintf(query, "select type from mrna where acc = '%s'", acc);
type = sqlQuickQuery(conn, query, typeBuf, sizeof(typeBuf));
hFreeConn(&conn);
return type;
}

static struct psl *findAllAli(char *acc, char *type)
/* Find all alignments of the given type. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct psl *pslList = NULL, *psl;
struct sqlResult *sr;
char **row;

sprintf(query, "select * from all_%s where qName = '%s'", type, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row);
    slAddHead(&pslList, psl);
    }
hFreeConn(&conn);
slReverse(&pslList);
return pslList;
}

static int pslMrnaScore(const struct psl *psl)
/* Return a simple score for psl. */
{
return psl->match + (psl->repMatch>>1) - psl->misMatch - psl->qNumInsert;
}

static int pslCmpScore(const void *va, const void *vb)
/* Compare two psl to sort by score position . */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
return pslMrnaScore(b) - pslMrnaScore(a);
}

static void mrnaHtmlStart(struct hgPosTable *table, FILE *f)
/* Print preamble to mrna alignment positions. */
{
fprintf(f, "<H2>%s</H2>", table->name);
fprintf(f, "This aligns in multiple positions.  Click on a hyperlink to ");
fprintf(f, "go to tracks display at a particular alignment.<BR>");

fprintf(f, "<TT><PRE>");
fprintf(f, " SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
fprintf(f, "------------------------------------------------------------------------------\n");
}

static void mrnaHtmlEnd(struct hgPosTable *table, FILE *f)
/* Print end to mrna alignment positions. */
{
fprintf(f, "</TT></PRE>");
}

static void mrnaHtmlOnePos(struct hgPosTable *table, struct hgPos *pos, FILE *f)
/* Print one mrna alignment position. */
{
fprintf(f, "%s", pos->description);
}

static boolean findMrnaPos(char *acc,  struct hgPositions *hgp)
/* Look to see if it's an mRNA.  Fill in hgp and return
 * TRUE if it is, otherwise return FALSE. */
{
char *type;
char *browserUrl = hgTracksName();
char *extraCgi = hgp->extraCgi;

if ((type = mrnaType(acc)) == NULL)
    return FALSE;
else
    {
    struct psl *pslList, *psl;
    int pslCount;
    char prefix[16];
    struct hgPosTable *table;
    struct hgPos *pos;

    strncpy(prefix, type, sizeof(prefix));
    tolowers(prefix);
    pslList = psl = findAllAli(acc, prefix);
    pslCount = slCount(pslList);
    if (pslCount <= 0)
        {
	errAbort("%s %s doesn't align anywhere in the draft genome", type, acc);
	return FALSE;
	}
    else
        {
	struct dyString *dy = newDyString(1024);
	AllocVar(table);
	table->htmlStart = mrnaHtmlStart;
	table->htmlEnd = mrnaHtmlEnd;
	table->htmlOnePos = mrnaHtmlOnePos;
	slAddHead(&hgp->tableList, table);
	dyStringPrintf(dy, "%s Alignments", acc);
	table->name = cloneString(dy->string);
	slSort(&pslList, pslCmpScore);
	for (psl = pslList; psl != NULL; psl = psl->next)
	    {
	    dyStringClear(dy);
	    AllocVar(pos);
	    pos->chrom = hgOfficialChromName(psl->tName);
	    pos->chromStart = psl->tStart;
	    pos->chromEnd = psl->tEnd;
	    pos->name = cloneString(psl->qName);
	    dyStringPrintf(dy, "<A HREF=\"%s?position=%s%s\">",
	        browserUrl, hgPosBrowserRange(pos, NULL), extraCgi);
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
	freeDyString(&dy);
	return TRUE;
	}
    }
}

static boolean findStsPos(char *spec, struct hgPositions *hgp)
/* Look for position in stsMarker table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
struct stsMarker sm;
char *tableName = "stsMarker";
char *chrom;
char *alias;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (!hTableExists(tableName))
    return FALSE;
conn = hAllocConn();
query = newDyString(256);
if (hTableExists("stsAlias"))
    {
    dyStringPrintf(query, 
        "select trueName from stsAlias where alias = '%s'", spec);
    alias = sqlQuickQuery(conn, query->string, buf, sizeof(buf));
    if (alias != NULL)
	spec = alias;
    }
dyStringClear(query);
dyStringPrintf(query, "select * from %s where name = '%s'", tableName, spec);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	dyStringPrintf(query, "STS %s Positions", spec);
	table->name = cloneString(query->string);
	slAddHead(&hgp->tableList, table);
	}
    stsMarkerStaticLoad(row, &sm);
    if ((chrom = hgOfficialChromName(sm.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s", sm.chrom, tableName); 
    AllocVar(pos);
    pos->chrom = chrom;
    pos->chromStart = sm.chromStart - 100000;
    pos->chromEnd = sm.chromEnd + 100000;
    pos->name = cloneString(spec);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}


static boolean findOldStsPos(char *table, char *spec, 
   char **retChromName, int *retWinStart, int *retWinEnd)
/* Look for position in some STS table. */
/* This code is being replaced by the newer sts pos finder,
 * but is still used on the hg3 (July 17 2000 Freeze) database. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean ok = FALSE;
struct mapSts *mapSts;

if (!hTableExists(table))
    return FALSE;
dyStringPrintf(query, "select * from %s where name = '%s'", table, spec);
sr = sqlGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row != NULL)
    {
    mapSts = mapStsLoad(row);
    *retChromName = hgOfficialChromName(mapSts->chrom);
    *retWinStart = mapSts->chromStart - 100000;
    *retWinEnd = mapSts->chromEnd + 100000;
    mapStsFree(&mapSts);
    ok = TRUE;
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findGenethonPos(char *spec, char **retChromName, int *retWinStart, int *retWinEnd)
/* See if it's a genethon map position. */
{
return findOldStsPos("mapGenethon", spec, retChromName, retWinStart, retWinEnd);
}

#ifdef OLD
static struct slName *accsThatMatchKey(char *key, char *field)
/* Return list of accessions that match key on a particular field. */
{
struct slName *list = NULL, *el;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

sprintf(query, "select mrna.acc from mrna,%s where %s.name like '%%%s%%'  and mrna.%s = %s.id", field, field, key, field, field);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}
#endif /* OLD */

static void findHitsToTables(char *key, char *tables[], int tableCount, 
    struct hash **retHash, struct slName **retList)
/* Return all unique accessions that match any table. */
{
struct slName *list = NULL, *el;
struct hash *hash = newHash(0);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
char *field;
int i;

for (i = 0; i<tableCount; ++i)
    {
    struct slName *idList = NULL, *idEl;
    
    /* I'm doing this query in two steps in C rather than
     * in one step in SQL just because it somehow is much
     * faster this way (like 100x faster) when using mySQL. */
    field = tables[i];
    sprintf(query, "select id from %s where name like '%%%s%%'", 
       field, key);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	idEl = newSlName(row[0]);
	slAddHead(&idList, idEl);
	}
    sqlFreeResult(&sr);
    for (idEl = idList; idEl != NULL; idEl = idEl->next)
        {
	sprintf(query, "select acc from mrna where %s = %s and type = 'mRNA'", field, idEl->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char *acc = row[0];
	    if (!hashLookup(hash, acc))
		{
		el = newSlName(acc);
		slAddHead(&list, el);
		hashAdd(hash, acc, NULL);
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
    if (hashLookup(bHash, name) && !hashLookup(hash, name))
        {
	newEl = newSlName(name);
	slAddHead(&list, newEl);
	hashAdd(hash, name, NULL);
	}
    }
*retHash = hash;
*retList = list;
}

static void mrnaKeysHtmlOnePos(struct hgPosTable *table, struct hgPos *pos, FILE *f)
{
fprintf(f, "%s", pos->description);
}

static void findMrnaKeys(char *keys, struct hgPositions *hgp)
/* Find mRNA that has keyword in one of it's fields. */
{
char *words[32];
char buf[512];
int wordCount;
struct slName *el;
static char *tables[] = {
	"productName", "geneName",
	"author", "tissue", "cell", "description", "development", 
	};
struct hash *oneKeyHash = NULL;
struct slName *oneKeyList = NULL;
struct hash *allKeysHash = NULL;
struct slName *allKeysList = NULL;
struct hash *andedHash = NULL;
struct slName *andedList = NULL;
int i;
struct dyString *dy = NULL;
struct hgPosTable *table;
struct hgPos *pos;


strncpy(buf, keys, sizeof(buf));
wordCount = chopLine(buf, words);
if (wordCount == 0)
    return;
for (i=0; i<wordCount; ++i)
    {
    findHitsToTables(words[i], tables, ArraySize(tables), &oneKeyHash, &oneKeyList);
    if (allKeysHash == NULL)
        {
	allKeysHash = oneKeyHash;
	oneKeyHash = NULL;
	allKeysList = oneKeyList;
	oneKeyList = NULL;
	}
    else
        {
	andHits(oneKeyHash, oneKeyList, allKeysHash, allKeysList, &andedHash, &andedList);
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
if (allKeysList == NULL)
    return;


dy = newDyString(256);
AllocVar(table);
slAddHead(&hgp->tableList, table);
table->name = cloneString("mRNA Associated Search Results");
table->htmlOnePos = mrnaKeysHtmlOnePos;

/* Fetch descriptions of all matchers and display. */
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    char description[512];
    for (el = allKeysList; el != NULL; el = el->next)
	{
	AllocVar(pos);
	slAddHead(&table->posList, pos);
	pos->name = cloneString(el->name);
	dyStringClear(dy);
	dyStringPrintf(dy, "<A HREF=\"%s?position=%s%s\">", 
		hgTracksName(), el->name, hgp->extraCgi);
	dyStringPrintf(dy, "%s </A>", el->name);
	sprintf(query, 
		"select description.name from mrna,description"
		" where mrna.acc = '%s' and mrna.description = description.id",
		el->name);
        if (sqlQuickQuery(conn, query, description, sizeof(description)))
	    dyStringPrintf(dy, "- %s", description);
	dyStringPrintf(dy, "\n");
	pos->description = cloneString(dy->string);
	}
    slReverse(&table->posList);
    hFreeConn(&conn);
    }
freeDyString(&dy);
}

static void findKnownGenes(char *spec, struct hgPositions *hgp)
/* Look up known genes in table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean gotOne = FALSE;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct genePred *gp;
struct knownInfo *knownInfo;
char *kiTable = NULL;

if (sqlTableExists(conn, "knownMore"))
    kiTable = "knownMore";
else if (sqlTableExists(conn, "knownInfo"))
    kiTable = "knownInfo";
if (kiTable != NULL)
    {
    dyStringPrintf(query, "select * from %s where name like '%s%%'", kiTable, spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (!gotOne)
	    {
	    gotOne = TRUE;
	    AllocVar(table);
	    slAddHead(&hgp->tableList, table);
	    table->name = cloneString("Known Genes");
	    }
	knownInfo = knownInfoLoad(row);
	AllocVar(pos);
	slAddHead(&table->posList, pos);
	pos->name = cloneString(knownInfo->name);
	pos->description = cloneString(knownInfo->transId);
	}
    sqlFreeResult(&sr);

    if (table != NULL)
	{
	slReverse(&table->posList);
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    dyStringClear(query);
	    dyStringPrintf(query, "select * from genieKnown where name = '%s'", pos->description);
	    sr = sqlGetResult(conn, query->string);
	    if ((row = sqlNextRow(sr)) == NULL)
		errAbort("Internal error: %s in knownInfo but not genieKnown", pos->description);
	    gp = genePredLoad(row);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    freez(&pos->description);
	    genePredFree(&gp);
	    sqlFreeResult(&sr);
	    }
	}
    }
freeDyString(&query);
hFreeConn(&conn);
}


static void findRefGenes(char *spec, struct hgPositions *hgp)
/* Look up refSeq genes in table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row;
boolean gotOne = FALSE;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct genePred *gp;
struct refLink *rlList = NULL, *rl;

if ((startsWith("NM_", spec) || startsWith("XM_", spec)) && sqlTableExists(conn, "refLink"))
    {
    dyStringPrintf(ds, "select * from refLink where mrnaAcc = '%s'", spec);
    sr = sqlGetResult(conn, ds->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rl = refLinkLoad(row);
	slAddHead(&rlList, rl);
	}
    sqlFreeResult(&sr);
    }
else if (sqlTableExists(conn, "refLink"))
    {
    dyStringPrintf(ds, "select * from refLink where name like '%s%%'", spec);
    sr = sqlGetResult(conn, ds->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rl = refLinkLoad(row);
	slAddHead(&rlList, rl);
	}
    sqlFreeResult(&sr);
    }
if (rlList != NULL)
    {
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->name = cloneString("Known Genes");
    for (rl = rlList; rl != NULL; rl = rl->next)
        {
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refGene where name = '%s'", rl->mrnaAcc);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    gp = genePredLoad(row);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(rl->name);
	    dyStringClear(ds);
	    dyStringPrintf(ds, "(%s) %s", rl->mrnaAcc, rl->product);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    genePredFree(&gp);
	    }
	sqlFreeResult(&sr);
	}
#ifdef SOON
#endif /* SOON */
    refLinkFreeList(&rlList);
    }
freeDyString(&ds);
hFreeConn(&conn);
}


struct hgPositions *hgPositionsFind(char *query, char *extraCgi)
/* Return table of positions that match query or NULL if none such. */
{
struct hgPositions *hgp;
struct hgPosTable *table;
struct hgPos *pos;
int start,end;
char *chrom;

AllocVar(hgp);
query = trimSpaces(query);
hgp->query = cloneString(query);
hgp->database = hGetDb();
if (extraCgi == NULL)
    extraCgi = "";
hgp->extraCgi = cloneString(extraCgi);

if (hgIsChromRange(query))
    {
    hgParseChromRange(query, &chrom, &start, &end);
    singlePos(hgp, "Chromosome Range", NULL, query, chrom, start, end);
    }
else if (isContigName(query))
    {
    findContigPos(query, &chrom, &start, &end);
    singlePos(hgp, "FPC Contig", NULL, query, chrom, start, end);
    }
else if (hgFindCytoBand(query, &chrom, &start, &end))
    {
    singlePos(hgp, "Cytological Band", NULL, query, chrom, start, end);
    }
else if (hgFindClonePos(query, &chrom, &start, &end))
    {
    singlePos(hgp, "Genomic Clone", NULL, query, chrom, start, end);
    }
else if (findMrnaPos(query, hgp))
    {
    }
else if (findStsPos(query, hgp))
    {
    }
else if (findGenethonPos(query, &chrom, &start, &end))	/* HG3 only. */
    {
    singlePos(hgp, "STS Position", NULL, query, chrom, start, end);
    }
else 
    {
    findKnownGenes(query, hgp);
    findRefGenes(query, hgp);
    findMrnaKeys(query, hgp);
    }
slReverse(&hgp->tableList);
fixSinglePos(hgp);
return hgp;
}

void hgPosTableFree(struct hgPosTable **pEl)
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

void hgPosTableFreeList(struct hgPosTable **pList)
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


void hgPosFree(struct hgPos **pEl)
/* Free up hgPos. */
{
struct hgPos *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    freeMem(el->description);
    freez(pEl);
    }
}

void hgPosFreeList(struct hgPos **pList)
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

char *hgPosBrowserRange(struct hgPos *pos, char range[64])
/* Convert pos to chrN:123-456 format.  If range parameter is NULL it returns
 * static buffer, otherwise writes and returns range. */
{
static char buf[64];

if (range == NULL)
    range = buf;
sprintf(range, "%s:%d-%d", pos->chrom, pos->chromStart, pos->chromEnd);
return range;
}

void hgPositionsHtml(struct hgPositions *hgp, FILE *f)
/* Write out hgp table as HTML to file. */
{
struct hgPosTable *table;
struct hgPos *pos;
char *desc;
char range[64];
char *browserUrl = hgTracksName();
char *extraCgi = hgp->extraCgi;

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    if (table->posList != NULL)
	{
	if (table->htmlStart) 
	    table->htmlStart(table, f);
	else
	    fprintf(f, "<H2>%s</H2><PRE><TT>", table->name);
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    hgPosBrowserRange(pos, range);
	    if (table->htmlOnePos)
	        table->htmlOnePos(table, pos, f);
	    else
		{
		fprintf(f, "<A HREF=\"%s?position=%s%s\">%s at %s</A>",
		    browserUrl, range, extraCgi, pos->name, range);
		desc = pos->description;
		if (desc)
		    fprintf(f, " - %s", desc);
		fprintf(f, "\n");
		}
	    }
	if (table->htmlEnd) 
	    table->htmlEnd(table, f);
	else
	    fprintf(f, "</PRE></TT>\n");
	}
    }
}

