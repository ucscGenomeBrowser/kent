/* das - Distributed Annotation System server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "psl.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "bed.h"
#include "genePred.h"
#include "trackTable.h"
#include <regex.h>

char *version = "1.00";
char *database = NULL;	

void usage()
/* Explain usage and exit. */
{
errAbort(
  "das - Distributed Annotation System server.\n"
  "Needs to be called from a Web server generally.\n"
  "You can spoof it from the command line by giving\n"
  "it a command argument such as:\n"
  "     das dsn\n"
  "     das hg6/types segment=chr22\n"
  );
}

void dasHead(int err)
/* Write out very start of DAS header */
{
printf("X-DAS-Version: DAS/0.95\n");
printf("X-DAS-Status: %d\n", err);
printf("Content-Type:text/plain\n");
printf("\n");
}

void dasHeader(int err)
/* Write out DAS header */
{
dasHead(err);
if (err != 200)
    exit(-1);
printf("<?xml version=\"1.0\" standalone=\"no\"?>\n");
}

void dasAbout()
/* Print a little info when they just hit cgi-bin/das. */
{
dasHead(200);
puts("UCSC DAS Server.\n");
puts("See http://www.biodas.org for more info on DAS.");
exit(0);
}

void normalHeader()
/* Write normal (non-error) header. */
{
dasHeader(200);
}

void earlyError(int errCode)
/* Return error in early processing (before writing header) */
{
dasHeader(errCode);
}

char *currentUrl()
/* Query environment to get current URL. */
{
static char url[512];
sprintf(url, "http://%s%s%s", getenv("SERVER_NAME"), getenv("SCRIPT_NAME"), getenv("PATH_INFO"));
return url;
}

struct tableDef
/* A table definition. */
    {
    struct tableDef *next;	/* Next in list. */
    char *name;			/* Name of table. */
    struct slName *splitTables;	/* Names of subTables.  Only used if isSplit. */
    char *chromField;		/* Name of field chromosome is stored in. */
    char *startField;		/* Name of field chromosome start is in. */
    char *endField;		/* Name of field chromosome end is in. */
    char *category;		/* Category type. */
    char *method;		/* Category type. */
    boolean hasBin;		/* Has bin field. */
    };

boolean tableIsSplit(char *table)
/* Return TRUE if table is split. */
{
if (!startsWith("chr", table))
    return FALSE;
if (strchr(table, '_') == NULL)
    return FALSE;
return TRUE;
}

char *skipOverChrom(char *table)
/* Skip over chrN_ or chrN_random_. */
{
char *e = strchr(table, '_');
if (e != NULL)
    {
    ++e;
    if (startsWith("random_", e))
	e += 7;
    table = e;
    }
return table;
}

boolean isChromId(char *seqName)
{
return((! startsWith("chr", seqName)) &&
       (isdigit(seqName[0]) ||
	sameString("NA", seqName) || sameString("UL", seqName) ||
	sameString("Un", seqName) ||
	sameString("X", seqName)  || sameString("Y", seqName) ||
	sameString("M", seqName)  ));
}

struct tableDef *getTables()
/* Get all tables. */
{
struct sqlConnection *conn = hAllocConn();
struct hash *hash = newHash(0);
struct hash *skipHash = newHash(7);
struct tableDef *tdList = NULL, *td;
struct sqlResult *sr;
char **row;
char *table, *root;
boolean isSplit, hasBin;
char chromField[32], startField[32], endField[32];

/* Set up some big tables that we don't actually want to serve. */
hashAdd(skipHash, "all_est", NULL);
hashAdd(skipHash, "all_mrna", NULL);
hashAdd(skipHash, "refFlat", NULL);
hashAdd(skipHash, "simpleRepeat", NULL);
hashAdd(skipHash, "ctgPos", NULL);
hashAdd(skipHash, "gold", NULL);
hashAdd(skipHash, "clonePos", NULL);
hashAdd(skipHash, "gap", NULL);
hashAdd(skipHash, "rmsk", NULL);
hashAdd(skipHash, "estPair", NULL);

sr = sqlGetResult(conn, "show tables");
while ((row = sqlNextRow(sr)) != NULL)
    {
    table = root = row[0];
    if (hFindFieldsAndBin(table, chromField, startField, endField, &hasBin))
	{
	isSplit = tableIsSplit(table);
	if (isSplit)
	    root = skipOverChrom(table);
	if (hashLookup(skipHash, root) == NULL)
	    {
	    if ((td = hashFindVal(hash, root)) == NULL)
		{
		AllocVar(td);
		slAddHead(&tdList, td);
		hashAdd(hash, root, td);
		td->name = cloneString(root);
		td->chromField = cloneString(chromField);
		td->startField = cloneString(startField);
		td->endField = cloneString(endField);
		td->hasBin = hasBin;
		if (stringIn("snp", root) || stringIn("Snp", root))
		     {
		     td->category = "variation";
		     }
		else if (stringIn("est", root) || stringIn("Est", root) 
		    || stringIn("mrna", root) || stringIn("Mrna", root))
		    {
		    td->category = "transcription";
		    td->method = "BLAT";
		    }
		else if (sameString("txStart", startField) || stringIn("rnaGene", root))
		    {
		    td->category = "transcription";
		    }
		else if (stringIn("mouse", root) || stringIn("Mouse", root) ||
		    stringIn("fish", root) || stringIn("Fish", root))
		    {
		    td->category = "similarity";
		    if (startsWith("blat", root))
		         td->method = "BLAT";
		    else if (sameString("exoMouse", root))
		         td->method = "Exonerate";
		    else if (sameString("exoMouse", root))
		         td->method = "Exofish";
		    }
		else if (sameString("gap", root) || sameString("gold", root) ||
			sameString("gl", root) || sameString("clonePos", root))
		    {
		    td->category = "structural";
		    td->method = "GigAssembler";
		    }
		else
		    td->category = "other";
		}
	    if (isSplit)
	        {
		struct slName *sln = newSlName(table);
		slAddHead(&td->splitTables, sln);
		}
	    }
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hashFree(&hash);
hashFree(&skipHash);
slReverse(&tdList);
return tdList;
}

struct hash *hashOfTracks()
/* Get list of tracks and put into hash keyed by tableName*/
{
struct hash *trackHash = newHash(7);
struct trackTable *ttList = hGetTracks(), *tt;
for (tt = ttList; tt != NULL; tt = tt->next)
    hashAdd(trackHash, tt->tableName, tt);
return trackHash;
}

struct segment
/* A subset of a sequence. */
    {
    struct segment *next;	/* Next in list. */
    char *seq;		/* Name of sequence, often a chromosome. */
    int start;		/* Zero based start. */
    int end;		/* Base after end. */
    boolean wholeThing;	/* TRUE if user requested whole sequence. */
    char *seqName;      /* Name of sequence in external DAS world */
    };

struct segment *segmentNew(char *seq, int start, int end, boolean wholeThing,
			   char *seqName)
/* Make a new segment. */
{
struct segment *segment;
AllocVar(segment);
segment->seq = cloneString(seq);
segment->start = start;
segment->end = end;
segment->wholeThing = wholeThing;
segment->seqName = cloneString(seqName);
return segment;
}

struct segment *dasSegmentList(boolean mustExist)
/* Get a DAS segment list from either segment or ref parameters. 
 * Call this before you write out header so it can return errors
 * properly if parameters are malformed. */
{
struct slName *segList, *seg;
struct segment *segmentList = NULL, *segment;
char *seq, *seqName;
int start=0, end=0;
boolean wholeThing;

segList = cgiStringList("segment");
if (segList != NULL)
    {
    /* Handle new format (post 0.995 spec) lists. */
    for (seg = segList; seg != NULL; seg = seg->next)
	{
	char *parts[3];
	int partCount;

	wholeThing = FALSE;
	partCount = chopString(seg->name, ":,", parts, ArraySize(parts));
	seq = seqName = parts[0];
	if (isChromId(seqName))
	    {
	    /* Prepend "chr" if client passes in chromosome number */
	    seq = needMem(strlen(seqName)+4);
	    snprintf(seq, strlen(seqName)+4, "chr%s", seqName);
	    }
	if (partCount == 1)
	    {
	    start = 0;
	    end = hChromSize(seq);
	    wholeThing = TRUE;
	    }
	else if (partCount == 3)
	    {
	    if (!isdigit(parts[1][0]) || !isdigit(parts[2][0]))
		earlyError(402);
	    start = atoi(parts[1])-1;
	    end = atoi(parts[2]);
	    if (start > end)
	        earlyError(405);
	    }
	else
	    {
	    earlyError(402);
	    }
	segment = segmentNew(seq, start, end, wholeThing, seqName);
	slAddHead(&segmentList, segment);
	}
    slReverse(&segmentList);
    }
else
    {
    /* Handle old format (pre 0.995 spec) lists. */
    seq = seqName = cgiOptionalString("ref");
    wholeThing = TRUE;
    if (seq == NULL)
	{
	if (mustExist)
	    earlyError(402);
	else
	    {
	    return NULL;
	    }
	}
    if (isChromId(seqName))
	{
	/* Prepend "chr" if client passes in chromosome number */
	seq = needMem(strlen(seqName)+4);
	snprintf(seq, strlen(seqName)+4, "chr%s", seqName);
	}
    if (cgiVarExists("start"))
	{
        start = cgiInt("start")-1;
	wholeThing = FALSE;
	}
    else
        start = 0;
    if (cgiVarExists("stop"))
	{
        end = cgiInt("stop");
	wholeThing = FALSE;
	}
    else
        end = hChromSize(seq);
    if (start > end)
	earlyError(405);
    segmentList = segmentNew(seq, start, end, wholeThing, seqName);
    }
/* Check all segments are chromosomes. */
for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    if (!startsWith("chr", segment->seq))
        earlyError(403);
    }
return segmentList;
}

struct regExp
/* A compiled regular expression. */
    {
    struct regExp *next;
    char *exp;			/* Expression (uncompiled) */
    regex_t compiled;		/* Compiled version of regExp. */
    };

struct regExp *regExpFromCgiVar(char *cgiVar)
/* Make up regExp list from cgiVars */
{
struct slName *nList = cgiStringList(cgiVar), *n;
struct regExp *reList = NULL, *re;
for (n = nList; n != NULL; n = n->next)
    {
    AllocVar(re);
    slAddHead(&reList, re);
    re->exp = n->name;
    if (regcomp(&re->compiled, n->name, REG_NOSUB))
	earlyError(402);
    }
slReverse(&reList);
return reList;
}

boolean regExpFilter(struct regExp *reList, char *string)
/* Return TRUE if string matches any of the regular expressions
 * on reList. */
{
struct regExp *re;
for (re = reList; re != NULL; re = re->next)
    {
    if (regexec(&re->compiled, string, 0, NULL, 0) == 0)
        return TRUE;
    }
return FALSE;
}

boolean catTypeFilter(struct regExp *catExp, char *cat, struct regExp *typeExp, char *type)
/* Combined category/type filter. */
{
if (catExp != NULL && typeExp != NULL)
    return regExpFilter(catExp,cat) || regExpFilter(typeExp, type);
if (catExp != NULL)
    return regExpFilter(catExp,cat);
if (typeExp != NULL)
    return regExpFilter(typeExp, type);
return TRUE;
}


void doDsn(struct slName *dbList)
/* dsn - DSN Server for DAS. */
{
struct slName *db;

normalHeader();
printf(
// " <!DOCTYPE DASDSN SYSTEM \"http://www.biodas.org/dtd/dasdsn.dtd\">\n"
" <DASDSN>\n");
for (db = dbList; db != NULL; db = db->next)
    {
    char *freeze = hFreezeFromDb(db->name);
    printf("   <DSN>\n");
    printf("     <SOURCE id=\"%s\" version=\"%s\">%s at UCSC</SOURCE>\n", 
    	db->name, version, freeze);
    printf("     <MAPMASTER>http://genome.cse.ucsc.edu:80/cgi-bin/das/%s</MAPMASTER>\n",
        db->name);
    printf("     <DESCRIPTION>%s Human Genome at UCSC</DESCRIPTION>\n", freeze);
    printf("   </DSN>\n");
    }
printf(" </DASDSN>\n");
}

int countFeatures(struct tableDef *td, struct segment *segmentList)
/* Count all the features in a given segment. */
{
struct segment *segment;
int acc = 0;
struct sqlConnection *conn = hAllocConn();
char chrTable[256];
char query[512];
struct slName *n;

if (segmentList == NULL)
    {
    if (td->splitTables == NULL)
        {
	sprintf(query, "select count(*) from %s", td->name);
	acc = sqlQuickNum(conn, query);
	}
    else
        {
	for (n = td->splitTables; n != NULL; n = n->next)
	    {
	    sprintf(query, "select count(*) from %s", n->name);
	    acc += sqlQuickNum(conn, query);
	    }
	}
    }
else
    {
    for (segment = segmentList; segment != NULL; segment = segment->next)
	{
	if (segment->wholeThing)
	    {
	    if (td->splitTables == NULL)
	        {
		sprintf(query, "select count(*) from %s where %s = '%s'", 
			td->name, td->chromField, segment->seq);
		acc += sqlQuickNum(conn, query);
		}
	    else
	        {
		sprintf(chrTable, "%s_%s", segment->seq, td->name);
		if (sqlTableExists(conn, chrTable))
		    {
		    sprintf(query, "select count(*) from %s", 
			    chrTable);
		    acc += sqlQuickNum(conn, query);
		    }
		}
	    }
	else
	    {
	    if (td->splitTables == NULL)
	        {
		sprintf(query, "select count(*) from %s where %s = '%s' and %s < %d and %s > %d",
		     td->name, td->chromField, segment->seq,
		     td->startField, segment->end, td->endField, segment->start);
		acc += sqlQuickNum(conn, query);
		}
	    else
	        {
		sprintf(chrTable, "%s_%s", segment->seq, td->name);
		if (sqlTableExists(conn, chrTable))
		    {
		    sprintf(query, "select count(*) from %s where %s < %d and %s > %d", chrTable, 
			 td->startField, segment->end, td->endField, segment->start);
		    acc += sqlQuickNum(conn, query);
		    }
		}
	    }
	}
    }
hFreeConn(&conn);
return acc;
}


void regExpTest(char *needle, char *haystack)
/* Test regular expression. */
{
regex_t preg;
int res;
res = regcomp(&preg, needle, REG_NOSUB);
printf("Result of regcomp is %d\n", res);
res = regexec(&preg, haystack, 0, NULL, 0);
printf("Result of regexec is %d\n", res);
regfree(&preg);
printf("All done\n");
}

void doTypes()
/* Handle a types request. */
{
struct segment *segment, *segmentList = dasSegmentList(FALSE);
struct tableDef *tdList = getTables(), *td;
struct regExp *category = regExpFromCgiVar("category");
struct regExp *type = regExpFromCgiVar("type");

normalHeader();
// printf("<!DOCTYPE DASTYPES SYSTEM \"http://www.biodas.org/dtd/dastypes.dtd\">\n");
printf("<DASTYPES>\n");
printf("<GFF version=\"1.2\" summary=\"yes\" href=\"%s\">\n", currentUrl());
for (segment = segmentList;;)
    {
    if (segment == NULL)
        printf("<SEGMENT version=\"%s\">\n", version);
    else
	printf("<SEGMENT id=\"%s\" start=\"%d\" stop=\"%d\" version=\"%s\">\n", 
	    segment->seqName, segment->start+1, segment->end, version);
    for (td = tdList; td != NULL; td = td->next)
	{
	if (catTypeFilter(category, td->category, type, td->name) )
	    {
	    int count = countFeatures(td, segment);
	    printf("<TYPE id=\"%s\" category=\"%s\" ", td->name, td->category);
	    if (td->method != NULL)
		printf("method=\"%s\" ", td->method);
	    printf(">");
	    printf("%d", count);
	    printf("</TYPE>\n");
	    }
	}
    printf("</SEGMENT>\n");
    if (segment == NULL || segment->next == NULL)
        break;
    segment = segment->next;
    }
printf("</GFF>\n");
printf("</DASTYPES>\n");
}

void dasPrintType(struct tableDef *td, struct trackTable *tt)
/* Print out from <TYPE> to </TYPE> inside a feature. */
{
char *description = (tt != NULL ? tt->shortLabel : td->name);
printf(" <TYPE id=\"%s\" category=\"%s\" reference=\"no\">%s</TYPE>\n", td->name, td->category, description);
}

void dasOutGp(struct genePred *gp, struct tableDef *td, struct trackTable *tt)
/* Write out DAS info on a gene prediction. */
{
int i;
for (i=0; i<gp->exonCount; ++i)
    {
    int start = gp->exonStarts[i];
    int end =  gp->exonEnds[i];
    char strand = gp->strand[0];
    if (strand == 0) strand = '?';
    printf(
    "<FEATURE id=\"%s.%s.%d.%d\" label=\"%s\">\n", gp->name, gp->chrom, gp->txStart, i, gp->name);
    dasPrintType(td, tt);
    if (td->method != NULL)
	printf(" <METHOD id=\"id\">%s</METHOD>\n", td->method);
    else
	printf(" <METHOD></METHOD>\n");
    printf(" <START>%d</START>\n", start+1);
    printf(" <END>%d</END>\n", end);
    printf(" <SCORE>-</SCORE>\n");
    printf(" <ORIENTATION>%c</ORIENTATION>\n", strand);
    printf(" <PHASE>-</PHASE>\n");
    printf(" <GROUP id=\"%s.%s.%d\">\n", gp->name, gp->chrom, gp->txStart);
    printf("  <LINK href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&amp;db=%s\">Link to UCSC Browser</LINK>\n", 
	gp->chrom, gp->txStart, gp->txEnd, database);
    printf(" </GROUP>\n");
    printf("</FEATURE>\n");
    }
}

void dasOutPsl(struct psl *psl, struct tableDef *td, struct trackTable *tt)
/* Write out DAS info on a psl alignment. */
{
int i;
int score = 1000 - pslCalcMilliBad(psl, TRUE);
for (i=0; i<psl->blockCount; ++i)
    {
    int s = psl->tStarts[i];
    int e = s + psl->blockSizes[i];
    int qs, qe, qStart, qEnd;
    int start,end;
    if (psl->strand[1] == '-')
        {
	start = psl->tSize - e;
	end = psl->tSize - s;
	}
    else
        {
	start = s;
	end = e;
	}
    s = psl->qStarts[i];
    e = s + psl->blockSizes[i];
    if (psl->strand[0] == '-')
        {
	qStart = psl->qSize - e;
	qEnd = psl->qSize - s;
	}
    else
        {
	qStart = s;
	qEnd = e;
	}
    printf(
    "<FEATURE id=\"%s.%s.%d.%d\" label=\"%s\">\n", psl->qName, psl->tName, psl->tStart, i, psl->qName);
    dasPrintType(td, tt);
    if (td->method != NULL)
	printf(" <METHOD id=\"id\">%s</METHOD>\n", td->method);
    else
	printf(" <METHOD></METHOD>\n");
    printf(" <START>%d</START>\n", start+1);
    printf(" <END>%d</END>\n", end);
    printf(" <SCORE>%d</SCORE>\n", score);
    printf(" <ORIENTATION>%c</ORIENTATION>\n", psl->strand[0]);
    printf(" <PHASE>-</PHASE>\n");
    printf(" <GROUP id=\"%s.%s.%d\">\n", psl->qName, psl->tName, psl->tStart);
    printf("  <LINK href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&amp;db=%s\">Link to UCSC Browser</LINK>\n", 
	psl->tName, psl->tStart, psl->tEnd, database);
    printf("  <TARGET id=\"%s\" start=\"%d\" stop=\"%d\">%s</TARGET>\n",
        psl->qName, qStart+1, qEnd, psl->qName);
    printf(" </GROUP>\n");
    printf("</FEATURE>\n");
    }
}

void dasOutBed(char *chrom, int start, int end, 
	char *name, char *strand, char *score, 
	struct tableDef *td, struct trackTable *tt)
/* Write out a generic one. */
{
printf("<FEATURE id=\"%s.%s.%d\" label=\"%s\">\n", name, chrom, start, name);
dasPrintType(td, tt);
if (td->method != NULL)
    printf(" <METHOD id=\"id\">%s</METHOD>\n", td->method);
else
    printf(" <METHOD></METHOD>\n");
printf(" <START>%d</START>\n", start+1);
printf(" <END>%d</END>\n", end);
printf(" <SCORE>%s</SCORE>\n", score);
printf(" <ORIENTATION>%s</ORIENTATION>\n", strand);
printf(" <PHASE>-</PHASE>\n");
printf(" <GROUP id=\"%s.%s.%d\">\n", name, chrom, start);
printf("  <LINK href=\"http://genome.ucsc.edu/cgi-bin/hgTracks?position=%s:%d-%d&amp;db=%s\">Link to UCSC Browser</LINK>\n", 
    chrom, start, end, database);
printf(" </GROUP>\n");
printf("</FEATURE>\n");
}

int fieldIndex(char *table, char *field)
/* Returns index of field in a row from table, or -1 if it 
 * doesn't exist. */
{
struct sqlConnection *conn = hAllocConn();
int ix = sqlFieldIndex(conn, table, field);
hFreeConn(&conn);
return ix;
}


void doFeatures()
/* features - DAS Annotation Feature Server. */
{
struct segment *segmentList = dasSegmentList(TRUE), *segment;
struct hash *trackHash = hashOfTracks();
struct trackTable *tt;
struct tableDef *tdList = getTables(), *td;
struct slName *typeList = cgiStringList("type"), *typeEl;
struct regExp *category = regExpFromCgiVar("category");
struct regExp *type = regExpFromCgiVar("type");
char *parts[3];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start, end;
char *seq, *seqName;
struct dyString *query = newDyString(0);

/* Write out DAS features header. */
normalHeader();
printf(
// "<!DOCTYPE DASGFF SYSTEM \"http://www.biodas.org/dtd/dasgff.dtd\">\n"
"<DASGFF>\n"
"<GFF version=\"1.0\" href=\"%s\">\n", currentUrl());

for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    /* Print segment header. */
    seq = segment->seq;
    start = segment->start;
    end = segment->end;
    seqName = segment->seqName;
    printf(
    "<SEGMENT id=\"%s\" start=\"%d\" stop=\"%d\" version=\"%s\" label=\"%s\">\n",
	   seqName, start+1, end, version, seqName);

    /* Query database and output features. */
    for (td = tdList; td != NULL; td = td->next)
	{
	if (catTypeFilter(category, td->category, type, td->name) )
	    {
	    int rowOffset;
	    boolean hasBin;
	    char table[64];

	    hFindSplitTable(seq, td->name, table, &hasBin);
	    tt = hashFindVal(trackHash, td->name);
	    sr = hRangeQuery(conn, td->name, seq, start, end, NULL, &rowOffset);
	    if (sameString(td->startField, "tStart"))
		{
		while ((row = sqlNextRow(sr)) != NULL)
		    {
		    struct psl *psl = pslLoad(row+rowOffset);
		    dasOutPsl(psl, td, tt);
		    pslFree(&psl);
		    }
		}
	    else if (sameString(td->startField, "txStart"))
	        {
		while ((row = sqlNextRow(sr)) != NULL)
		    {
		    struct genePred *gp = genePredLoad(row+rowOffset);
		    dasOutGp(gp, td, tt);
		    genePredFree(&gp);
		    }
		}
	    else if (sameString(td->startField, "chromStart"))
	        {
		int scoreIx = fieldIndex(table, "score");
		int strandIx = fieldIndex(table, "strand");
		int nameIx = fieldIndex(table, "name");

		if (scoreIx == -1)
		    scoreIx = fieldIndex(table, "gcPpt");
		while ((row = sqlNextRow(sr)) != NULL)
		    {
		    char *strand = (strandIx >= 0 ? row[strandIx] : "0");
		    char *score = (scoreIx >= 0 ? row[scoreIx] : "-");
		    char *name = (nameIx >= 0 ? row[nameIx] : td->name);
		    dasOutBed(row[0+rowOffset], 
		    	sqlUnsigned(row[1+rowOffset]), 
			sqlUnsigned(row[2+rowOffset]), 
			name, strand, score, td, tt);
		    }
		}
	    sqlFreeResult(&sr);
	    }
	}
    printf("</SEGMENT>\n");
    }

/* Write out DAS footer. */
printf("</GFF></DASGFF>\n");

/* Clean up. */
freeDyString(&query);
freeHash(&trackHash);
hFreeConn(&conn);
}


void doEntryPoints()
/* Handle entry points request. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct chromInfo *ci;

normalHeader();
conn = hAllocConn();
// printf("<!DOCTYPE DASEP SYSTEM \"http://www.biodas.org/dtd/dasep.dtd\">\n");
printf("<DASEP>\n");
printf("<ENTRY_POINTS href=\"%s\" version=\"7.00\">\n",
	currentUrl());

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ci = chromInfoLoad(row);
    /* "chr"-less chromosome ID for clients such as Ensembl: */
    if (startsWith("chr", ci->chrom))
	printf(" <SEGMENT id=\"%s\" start=\"%d\" stop=\"%d\" orientation=\"+\" subparts=\"no\">%s</SEGMENT>\n", ci->chrom+3, 1, ci->size, ci->chrom+3);
    else
	printf(" <SEGMENT id=\"%s\" start=\"%d\" stop=\"%d\" orientation=\"+\" subparts=\"no\">%s</SEGMENT>\n", ci->chrom, 1, ci->size, ci->chrom);
    chromInfoFree(&ci);
    }
#ifdef SOON
#endif /* SOON */

printf("</ENTRY_POINTS>\n");
printf("</DASEP>\n");
}

void doDna()
/* Handle request for DNA. */
{
struct segment *segmentList = dasSegmentList(TRUE), *segment;
int size = 0;
struct dnaSeq *seq;
int i, oneSize, lineSize = 50;

/* Write header. */
normalHeader();
// printf("<!DOCTYPE DASDNA SYSTEM \"http://www.biodas.org/dtd/dasdna.dtd\">\n");
printf("<DASDNA>\n");

/* Write each sequence. */
for (segment = segmentList; segment != NULL; segment = segment->next)
    {
    printf("<SEQUENCE id=\"%s\" start=\"%d\" stop=\"%d\" version=\"%s\">\n",
	    segment->seqName, segment->start+1, segment->end, version);
    printf("<DNA length=\"%d\">\n", segment->end - segment->start);

    /* Write out DNA. */
    seq = hDnaFromSeq(segment->seq, segment->start, segment->end, dnaLower);
    if (seq == NULL)
        errAbort("Couldn't fetch %s\n", seq);
    size = seq->size;
    for (i=0; i<size; i += oneSize)
	{
	oneSize = size - i;
	if (oneSize > lineSize) oneSize = lineSize;
	mustWrite(stdout, seq->dna+i, oneSize);
	fputc('\n', stdout);
	}
    printf("</DNA>\n");
    printf("</SEQUENCE>\n");
    }

/* Write footer. */
printf("</DASDNA>\n");
}

void dispatch(char *dataSource, char *command)
/* Dispatch a dase command. */
{
struct slName *dbList = hDbList();
if (sameString(dataSource, "dsn"))
    {
    doDsn(dbList);
    }
else if (slNameFind(dbList, dataSource) != NULL)
    {
    database = dataSource;
    hSetDb(dataSource);
    if (sameString(command, "entry_points"))
        doEntryPoints();
    else if (sameString(command, "dna"))
        doDna();
    else if (sameString(command, "types"))
        doTypes();
    else if (sameString(command, "features"))
        doFeatures();
    else
        earlyError(501);
    }
else 
    earlyError(401);
}

void das(char *pathInfo)
/* das - Das Server. */
{
static char *parts[3];
int partCount;
if (pathInfo == NULL)
    dasAbout();
pathInfo = cloneString(pathInfo);
partCount = chopString(pathInfo, "/", parts, ArraySize(parts));
if (partCount < 1)
    dasAbout();
dispatch(parts[0], parts[1]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *path = getenv("PATH_INFO");

cgiSpoof(&argc, argv);
if (argc == 2)
    path = argv[1];
das(path);
return 0;
}
