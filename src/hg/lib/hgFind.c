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
#include "fishClones.h"
#include "lfs.h"
#include "snp.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "knownInfo.h"
#include "cart.h"
#include "hgFind.h"
#include "hdb.h"
#include "refLink.h"
#include "cheapcgi.h"

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


static char *getUiUrl(struct cart *cart)
/* Get rest of UI from browser. */
{
static struct dyString *dy = NULL;
static char *s = NULL;
if (dy == NULL)
    {
    dy = newDyString(64);
    if (cart != NULL)
	dyStringPrintf(dy, "%s=%u", cartSessionVarName(), cartSessionId(cart));
    s = dy->string;
    }
return s;
}


static void singlePos(struct hgPositions *hgp, char *tableDescription, char *posDescription,
	char *posName, char *chrom, int start, int end)
/* Fill in pos for simple case single position. */
{
struct hgPosTable *table;
struct hgPos *pos;

AllocVar(table);
AllocVar(pos);

slAddHead(&hgp->tableList, table);
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
return startsWith("ctg", contig) || startsWith("NT_", contig);
}

static boolean isAncientRName(char *name)
/* Return TRUE if name is an ancientRepeat ID. */
{
return startsWith("ar", name);
}



static void findAncientRPos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find human/mouse ancient repeat start, end, and chrom
 * from "name".  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct bed *bed = NULL;
dyStringPrintf(query, "select * from ancientR where name = '%s'", name);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find human/mouse ancient repeat: %s", name);
bed = bedLoadN(row+1,12);  /* 1 here since hasBin is TRUE, 12 for extended bed 12*/
*retChromName = hgOfficialChromName(bed->chrom);
*retWinStart = bed->chromStart;
*retWinEnd = bed->chromEnd;
bedFree(&bed);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


boolean findContigPos(char *contig, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean foundIt;

dyStringPrintf(query, "select * from ctgPos where contig = '%s'", contig);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    foundIt = FALSE;
else
    {
    struct ctgPos *ctgPos = ctgPosLoad(row);
    *retChromName = hgOfficialChromName(ctgPos->chrom);
    *retWinStart = ctgPos->chromStart;
    *retWinEnd = ctgPos->chromEnd;
    ctgPosFree(&ctgPos);
    foundIt = TRUE;
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundIt;
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
if (!isAccForm(spec) || !hTableExists("clonePos"))
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
int rowOffset;
char table[64];

sprintf(table, "all_%s", type);
rowOffset = hOffsetPastBin(NULL, table);
sprintf(query, "select * from %s where qName = '%s'", table, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
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

static boolean findMrnaPos(char *acc,  struct hgPositions *hgp, boolean useHgTracks, struct cart *cart)
/* Look to see if it's an mRNA.  Fill in hgp and return
 * TRUE if it is, otherwise return FALSE. */
{
char *type;
char *extraCgi = hgp->extraCgi;
char *ui = getUiUrl(cart);

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
	char *browserUrl;

	if(useHgTracks)
		browserUrl = hgTracksName();
	else
		browserUrl = hgTextName();
	
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
	    dyStringPrintf(dy, "<A HREF=\"%s?position=%s",
	        browserUrl, hgPosBrowserRange(pos, NULL) );
	    if (ui != NULL)
	        dyStringPrintf(dy, "&%s", ui);
	    dyStringPrintf(dy, "%s\">",
		extraCgi);
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
/* Look for position in stsMarker/stsMap table. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
char *alias = NULL, *temp;
struct stsMap sm;
char *tableName;
boolean newFormat;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("stsMap"))
    {
    newFormat = TRUE;
    tableName = "stsMap";
    }
else if (hTableExists("stsMarker"))
    {
    newFormat = FALSE;
    tableName = "stsMarker";
    }
else
    return FALSE;
conn = hAllocConn();
query = newDyString(256);
if (hTableExists("stsAlias"))
    {
    dyStringPrintf(query, 
        "select trueName from stsAlias where alias = '%s'", spec);
    alias = sqlQuickQuery(conn, query->string, buf, sizeof(buf));
    if ((alias != NULL) && (!sameString(alias, spec)))
        {
	hgp->useAlias = TRUE;
	temp = spec;
	spec = alias;
	alias = temp;
	}
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
	if (hgp->useAlias)
	    dyStringPrintf(query, "STS %s uses name %s in browser", alias, spec);
	else
	    dyStringPrintf(query, "STS %s Positions", spec);
	table->name = cloneString(query->string);
	slAddHead(&hgp->tableList, table);
	}
    if (newFormat)
	stsMapStaticLoad(row, &sm);
    else
        {
	struct stsMarker oldSm;
	stsMarkerStaticLoad(row, &oldSm);
	stsMapFromStsMarker(&oldSm, &sm);
	}
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

static boolean findFishClones(char *spec, struct hgPositions *hgp)
/* Look for position in fishClones table. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct fishClones *fc;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("fishClones"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from fishClones where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
            {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(fc);
	fc = fishClonesLoad(row);
	if ((chrom = hgOfficialChromName(fc->chrom)) == NULL)
	     errAbort("Internal Database error: Odd chromosome name '%s' in fishClones", fc->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = fc->chromStart - 100000;
	pos->chromEnd = fc->chromEnd + 100000;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions in FISH Clones track", spec);
	table->name = cloneString(query->string);
	slAddHead(&table->posList, pos);
	fishClonesFree(&fc);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findBacEndPairs(char *spec, struct hgPositions *hgp)
/* Look for position in bacEndPairs table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct lfs *be;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("bacEndPairs"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from bacEndPairs where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
	    {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(be);
	be = lfsLoad(row+1);
	if ((chrom = hgOfficialChromName(be->chrom)) == NULL)
	    errAbort("Internal Database error: Odd chromosome name '%s' in bacEndPairs", be->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = be->chromStart - 100000;
	pos->chromEnd = be->chromEnd + 100000;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions found using BAC end sequences", spec);
	table->name = cloneString(query->string);
	slAddHead(&table->posList, pos);
	lfsFree(&be);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findGenePred(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

localName = spec;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, "SELECT chrom, txStart, txEnd, name FROM %s WHERE name LIKE '%s'", tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
		ok = TRUE;
		AllocVar(table);
		dyStringClear(query);
		dyStringPrintf(query, "%s Gene Predictions", tableName);
		table->name = cloneString(query->string);
		slAddHead(&hgp->tableList, table);
		}

    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}


static boolean findSnpPos(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in stsMarker table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

/* Make sure it start's with 'rs'.  Then skip over it. */
if (!startsWith("rs", spec))
    return FALSE;
localName = spec+2;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, "select * from %s where name = '%s'", tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	dyStringPrintf(query, "SNP %s Position", spec);
	table->name = cloneString(query->string);
	slAddHead(&hgp->tableList, table);
	}
    snpStaticLoad(row+rowOffset, &snp);
    if ((chrom = hgOfficialChromName(snp.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s", snp.chrom, tableName); 
    AllocVar(pos);
    pos->chrom = chrom;
    pos->chromStart = snp.chromStart - 5000;
    pos->chromEnd = snp.chromEnd + 5000;
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

static void findMrnaKeys(char *keys, struct hgPositions *hgp, boolean useHgTracks, struct cart *cart)
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
    char *ui = getUiUrl(cart);
    for (el = allKeysList; el != NULL; el = el->next)
	{
	AllocVar(pos);
	slAddHead(&table->posList, pos);
	pos->name = cloneString(el->name);
	dyStringClear(dy);

	if(useHgTracks)
		dyStringPrintf(dy, "<A HREF=\"%s?position=%s", hgTracksName(), el->name);
	else
		dyStringPrintf(dy, "<A HREF=\"%s?position=%s", hgTextName(), el->name);

	if (ui != NULL)
	    dyStringPrintf(dy, "&%s", ui);
	dyStringPrintf(dy, "%s\">", 
		hgp->extraCgi);
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
return TRUE;
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
boolean gotRefLink = sqlTableExists(conn, "refLink");

if (gotRefLink)
    {
    if (startsWith("NM_", spec) || startsWith("XM_", spec))
	{
	dyStringPrintf(ds, "select * from refLink where mrnaAcc = '%s'", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else if (startsWith("NP_", spec) || startsWith("XP_", spec))
        {
	dyStringPrintf(ds, "select * from refLink where protAcc = '%s'", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else if (isUnsignedInt(spec))
        {
	dyStringPrintf(ds, "select * from refLink where locusLinkId = %s", spec);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refLink where omimId = %s", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else 
	{
	dyStringPrintf(ds, "select * from refLink where name like '%%%s%%'", spec);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refLink where product like '%%%s%%'", spec);
	addRefLinks(conn, ds, &rlList);
	}
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
    refLinkFreeList(&rlList);
    }
freeDyString(&ds);
hFreeConn(&conn);
}

static boolean isAffyProbeName(char *name)
/* Return TRUE if name is an Affymetrix Probe ID for HG-U95Av2. */
{
return startsWith("HG-U95Av2:", name);
}

static void findAffyProbePos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find affy probe start, end, and chrom
 * from "name".  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct bed *bed = NULL;
char *temp = strstr(name, ":"); /* parse name out of something like "HG-U95Av2:probeName" */
assert(temp);
temp++;
dyStringPrintf(query, "select * from affyRatio where name = '%s'", temp);
if(!hTableExists("affyRatio"))
    errAbort("Sorry GNF Ratio track not available yet in this version of the browser.");
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find affy probe: %s", name);
bed = bedLoadN(row,15);
*retChromName = hgOfficialChromName(bed->chrom);
*retWinStart = bed->chromStart;
*retWinEnd = bed->chromEnd;
bedFree(&bed);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static boolean genomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart, boolean showAlias)
/* Search for positions in genome that match user query.   
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */
{ 
struct hgPositions *hgp;
struct hgPos *pos;
struct dyString *ui;


if (strstr(spec,";") != NULL)
    return handleTwoSites(spec, retChromName, retWinStart, retWinEnd, cart);


hgp = hgPositionsFind(spec, "", TRUE, cart);
if (hgp == NULL || hgp->posCount == 0)
    {
    hgPositionsFree(&hgp);
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
    return TRUE;
    }
if (((pos = hgp->singlePos) != NULL) && (!showAlias || !hgp->useAlias))
    {
    *retChromName = pos->chrom;
    *retWinStart = pos->chromStart;
    *retWinEnd = pos->chromEnd;
    hgPositionsFree(&hgp);
    return TRUE;
    }
else
    {
    if (*retWinStart != 1)
	hgPositionsHtml(hgp, stdout, TRUE, cart);
    else
	*retWinStart = hgp->posCount;
    hgPositionsFree(&hgp);
    return FALSE;
    }
}



boolean findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart)
/* Search for positions in genome that match user query.   
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */
{
return genomePos(spec, retChromName, retWinStart, retWinEnd, cart, TRUE);
}

boolean handleTwoSites(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart)
/* Deal with specifications that in form start;end. */
{
struct hgPositions *hgp;
struct hgPos *pos;
struct dyString *ui;
char firststring[512];
char secondstring[512];
int commaspot;
char *firstChromName;
int firstWinStart = 0;
int firstWinEnd;
char *secondChromName;
int secondWinStart = 0;
int secondWinEnd;
boolean firstSuccess;
boolean secondSuccess;

firstWinStart = 1;     /* Pass flags indicating we are dealing with two sites through */
secondWinStart = 1;    /*    firstWinStart and secondWinStart.                        */



commaspot = strcspn(spec,";");
strncpy(firststring,spec,commaspot);
firststring[commaspot] = '\0';
strncpy(secondstring,spec + commaspot + 1,strlen(spec));
firstSuccess = genomePos(firststring,&firstChromName,&firstWinStart,&firstWinEnd, cart, FALSE);
secondSuccess = genomePos(secondstring,&secondChromName,&secondWinStart,&secondWinEnd, cart, FALSE); 
if (firstSuccess == FALSE && secondSuccess == FALSE)
    {
    errAbort("Neither site uniquely determined.  %d locations for %s and %d locations for %s.",firstWinStart,firststring,secondWinStart,secondstring);
    return TRUE;
    }
if (firstSuccess == FALSE) 
    {
    errAbort("%s not uniquely determined: %d locations.",firststring,firstWinStart);
    return TRUE;
    }
if (secondSuccess == FALSE)
    {
    errAbort("%s not uniquely determined: %d locations.",secondstring,secondWinStart);
    return TRUE;
    }
if (strcmp(firstChromName,secondChromName) != 0)
    {
    errAbort("Sites occur on different chromosomes: %s,%s.",firstChromName,secondChromName);
    return TRUE;
    }
*retChromName = firstChromName;
*retWinStart = min(firstWinStart,secondWinStart);
*retWinEnd = max(firstWinEnd,secondWinEnd);
return TRUE;
}


struct hgPositions *hgPositionsFind(char *query, char *extraCgi, boolean useHgTracks, 
	struct cart *cart)
/* Return table of positions that match query or NULL if none such. */
{
struct hgPositions *hgp;
struct hgPosTable *table;
struct hgPos *pos;
int start,end;
char *chrom;
boolean relativeFlag;
char buf[256];
char *startOffset,*endOffset;
int iStart = 0, iEnd = 0;

AllocVar(hgp);

hgp->useAlias = FALSE;
query = trimSpaces(query);
if(query == 0)
    return hgp;

hgp->query = cloneString(query);
hgp->database = hGetDb();
if (extraCgi == NULL)
    extraCgi = "";
hgp->extraCgi = cloneString(extraCgi);

/* MarkE offset code */

relativeFlag = FALSE;
strncpy(buf, query, 256);
startOffset = strchr(buf, ':');
if (startOffset != NULL) 
    {
    *startOffset++ = 0;
    endOffset = strchr(startOffset, '-');
    if (endOffset != NULL)
	{
	*endOffset++ = 0;
	startOffset = trimSpaces(startOffset);
	endOffset = trimSpaces(endOffset);
	if ((isdigit(startOffset[0])) && (isdigit(endOffset[0])))
	    {
	    iStart = atoi(startOffset)-1;
	    iEnd = atoi(endOffset);
	    relativeFlag = TRUE;
	    query = buf;
	    }
	}
    }

/* end MarkE offset code */




if (hgIsChromRange(query))
    {
    hgParseChromRange(query, &chrom, &start, &end);
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Chromosome Range", NULL, query, chrom, start, end);
    }

else if (isAffyProbeName(query))
    {
    findAffyProbePos(query, &chrom, &start, &end);
    singlePos(hgp, "GNF Ratio Expression data", NULL, query, chrom, start, end);
    }
else if (isAncientRName(query))
    {
    findAncientRPos( query, &chrom, &start, &end );
    singlePos( hgp, "Human/Mouse Ancient Repeats", NULL, query, chrom, start, end );
    }

else if (isContigName(query) && findContigPos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Map Contig", NULL, query, chrom, start, end);
    }
else if (hgFindCytoBand(query, &chrom, &start, &end))
    {
    singlePos(hgp, "Cytological Band", NULL, query, chrom, start, end);
    }
else if (hgFindClonePos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Genomic Clone", NULL, query, chrom, start, end);
    }
else if (findMrnaPos(query, hgp, useHgTracks, cart))
    {
    }
else if (findSnpPos(query, hgp, "snpTsc"))
    {
    }
else if (findSnpPos(query, hgp, "snpNih"))
    {
    }
else if (findGenePred(query, hgp, "sanger22"))
    {
    }
else if (findGenePred(query, hgp, "sanger20"))
    {
    }
else if (findGenePred(query, hgp, "ensGene"))
    {
    }
else if (findGenePred(query, hgp, "genieAlt"))
    {
    }
else if (findGenePred(query, hgp, "softberryGene"))
    {
    }
else if (findGenePred(query, hgp, "acembly"))
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
    findFishClones(query, hgp);
    findBacEndPairs(query, hgp);
    findStsPos(query, hgp);
    findMrnaKeys(query, hgp, useHgTracks, cart);
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

void hgPositionsHtml(struct hgPositions *hgp, FILE *f, boolean useHgTracks, struct cart *cart)
/* Write out hgp table as HTML to file. */
{
struct hgPosTable *table;
struct hgPos *pos;
char *desc;
char range[64];
char *browserUrl;
char *ui = getUiUrl(cart);
char *extraCgi = hgp->extraCgi;

if(useHgTracks)
	browserUrl = hgTracksName();
else
	browserUrl = hgTextName();

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
		fprintf(f, "<A HREF=\"%s?position=%s",
		    browserUrl, range);
		if (ui != NULL)
		    fprintf(f, "&%s", ui);
		fprintf(f, "%s\">%s at %s</A>",
		    extraCgi, pos->name, range);
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

