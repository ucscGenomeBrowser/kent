/* spToDb - Create a relational database out of SwissProt/trEMBL flat files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: spToDb.c,v 1.12 2006/07/14 16:32:17 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spToDb - Create a relational database out of SwissProt/trEMBL flat files\n"
  "usage:\n"
  "   spToDb swissProt.dat outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void groupLine(struct lineFile *lf, char *type, 
	char *firstLine, struct dyString *val)
/* Add first line and any subsequence lines that start with
 * type to val. */
{
char *line;
char rType[3];
rType[0] = type[0];
rType[1] = type[1];
dyStringClear(val);
dyStringAppend(val, firstLine);
while (lineFileNext(lf, &line, NULL))
    {
    if (rType[0] != line[0] || rType[1] != line[1])
	{
        lineFileReuse(lf);
	break;
	}
    dyStringAppendC(val, ' ');
    line += 5;
    dyStringAppend(val, line);
    }
}

void stripLastPeriod(char *s)
/* Remove last period if any. */
{
int end = strlen(s)-1;
if (s[end] == '.')
    s[end] = 0;
}

void stripLastChar(char *s)
/* Remove last character from string */
{
int end = strlen(s)-1;
s[end] = 0;
}

struct spRecord
/* A swissProt record. */
    {
    char *id;	/* Display ID */
    bool isCurated;	/* Is curated (SwissProt vs. trEMBL) */
    int aaSize;		/* Amino acid size. */
    struct slName *accList;	/* Accession list, first is primary. */
    char *createDate;   /* Creation date */
    char *seqDate;	/* Sequence last update. */
    char *annDate;	/* Annotation last update. */
    char *description;  /* Description - may be a couple of lines. */
    char *genes;	/* Associated genes (not yet parsed) */
    struct hashEl *gnList; /* Gene name list */
    char *orgSciName;	/* Organism scientific name. */
    struct slName *commonNames;	/* Common name(s). */
    char *taxonToGenus;	/* Taxonomy up to genus. */
    struct spTaxon *taxonList;	/* NCBI Taxonomy ID. */
    char *organelle;	/* Organelle. */
    struct spLitRef *literatureList;	/* List of literature references. */
    struct spComment *commentList;	/* List of comments. */
    struct spDbRef *dbRefList;	/* List of database cross references. */
    struct slName *keyWordList;	/* Key word list. */
    struct spFeature *featureList;	/* List of features. */
    int molWeight;	/* Molecular weight. */
    char *seq;	/* Sequence, one letter per amino acid. */
    };

struct spTaxon
/* List of taxon IDs. */
    {
    struct spTaxon *next;
    int id;
    };

struct spComment 
/* A swissProt structured comment. */
    {
    struct spComment *next;
    char *type;	/* Type of comment. */
    char *text;	/* Text of comment. */
    };

struct spDbRef 
/* A reference to another database from swissProt. */
    {
    struct spDbRef *next;	/* Next in list. */
    char *db;			/* Name of other database. */
    struct slName *idList;	/* ID's in other database. */
    };

struct spFeature
/* A feature - descibes a section of a protein. */
    {
    struct spFeature *next;	/* Next in list. */
    char *class;		/* Class of feature. */
    int start;			/* Zero based. */
    int end;			/* Non-inclusive. */
    char *type;			/* Type of feature, more specific than class. 
                                 * May be NULL. */
    char *FTId;			/* FT Id, may be NULL. */
    char softEndBits;/* 1 for start <, 2 for start ?, 4 for end >, 8 for end ? */
    };
#define spFeatureStartLess 1
#define spFeatureStartFuzzy 2
#define spFeatureEndGreater 4
#define spFeatureEndFuzzy 8

struct spLitRef 
/* A swissProt literature reference. */
    {
    struct spLitRef *next;
    char *title;	/* Title of article. */
    char *cite;		/* Journal/book/patent citation. */
    struct slName *authorList;	/* Author names in lastName, F.M. format. */
    char *rp;		/* Somewhat complex 'Reference Position' line. */
    struct hashEl *rxList; /* Cross-references. */
    struct hashEl *rcList; /* TISSUE=XXX X; STRAIN=YYY; parsed out. */
    char *pubMedId;	/* pubMed ID, may be NULL. */
    char *medlineId;	/* Medline ID, may be NULL. */
    char *doiId;	/* DOI ID, may be NULL. */
    };

static void spParseComment(struct lineFile *lf, char *line, 
	struct lm *lm, struct dyString *dy, struct spRecord *spr)
/* Parse comment into records and hang them on spr. */
{
struct spComment *com = NULL;
char *type;
for (;;)
    {
    /* Process current line. */
    if (startsWith("-!-", line))
        {
	/* Start new structured line. */
	if (com != NULL)
	    {
	    com->text = lmCloneString(lm, dy->string);
	    com = NULL;
	    }
	line += 4;	/* Skip '-!- '*/
	type = line;
	line = strchr(line, ':');
	if (line == NULL)
	    errAbort("expecting ':' line %d of %s", lf->lineIx, lf->fileName);
	*line++ = 0;
	lmAllocVar(lm, com);
	com->type = lmCloneString(lm, type);
	slAddHead(&spr->commentList, com);
	dyStringClear(dy);
	dyStringAppend(dy, skipLeadingSpaces(line));
	}
    else if (startsWith("---", line))
        {
	/* Probably copyright or something.  We don't save it
	 * here in order to save space, but we do respect it! */
	if (com != NULL)
	    {
	    com->text = lmCloneString(lm, dy->string);
	    com = NULL;
	    }
	}
    else
        {
	/* Save if we are in an open comment record. */
	if (com != NULL)
	    {
	    dyStringAppendC(dy, ' ');
	    dyStringAppend(dy, line+4);
	    }
	}

    /* Fetch next line.  Break if it is not comment. */
    if (!lineFileNext(lf, &line, NULL))
        break;
    if (!startsWith("CC", line))
	{
        lineFileReuse(lf);
	break;
	}
    line += 5;
    }
if (com != NULL)
    com->text = lmCloneString(lm, dy->string);
}

static void parseNameVals(struct lineFile *lf, struct lm *lm, 
	char *line, struct hashEl **pList)
/* Parse things of form 'xxx=yyyy zzz; aaa=bbb ccc;' into 
 * *pList. */
{
char *name, *val, *e;
char *inLine;
struct hashEl *hel;
inLine = strdup(line);
while (line != NULL && line[0] != 0)
    {
    name = skipLeadingSpaces(line);
    val = strchr(name, '=');
    if (val == NULL)
        {
	/* temporarily disable hard exit to accept new Swiss-Prot file format */
	/* errAbort("Expecting = line %d of %s", lf->lineIx, lf->fileName); */
        /* fprintf(stderr, "Expecting = line %d of %s:%s\n", lf->lineIx, lf->fileName, inLine); */
        if (!strstr(inLine, "DOI"))
	    {
	    fprintf(stderr, "%d of %s:%s\n", lf->lineIx, lf->fileName, inLine);
	    }
	break;
	}
    *val++ = 0;
    e = strchr(val, ';');
    if (e == NULL)
        errAbort("Expecting ';' line %d of %s", lf->lineIx, lf->fileName);
    *e++ = 0;
    lmAllocVar(lm, hel);
    hel->name = lmCloneString(lm, name);
    hel->val = lmCloneString(lm, val);
    slAddHead(pList, hel);
    line = e;
    }
freeMem(inLine);
}

static void spParseReference(struct lineFile *lf, char *line, 
	struct lm *lm, struct dyString *dy, struct spRecord *spr)
/* Parse refence into record and hang it on spr. */
{
struct spLitRef *lit;

/* We just ignore the RN line.  It is implicit in order in list. */
lmAllocVar(lm, lit);

while (lineFileNext(lf, &line, NULL))
    {
    /* uglyf("%d %s\n", lf->lineIx, line); */
    if (startsWith("RP", line))
        {
	groupLine(lf, line, line+5, dy);
	lit->rp = lmCloneString(lm, dy->string);
	}
    else if (startsWith("RG", line))
        {
	groupLine(lf, line, line+5, dy);
	lit->rp = lmCloneString(lm, dy->string);
	}
    else if (startsWith("RC", line))
        {
	groupLine(lf, line, line+5, dy);
	parseNameVals(lf, lm, dy->string, &lit->rcList);
	}
    else if (startsWith("RX", line))
        {
	groupLine(lf, line, line+5, dy);
	parseNameVals(lf, lm, dy->string, &lit->rxList);
	}
    else if (startsWith("RA", line))
        {
	groupLine(lf, line, line+5, dy);
	line = dy->string;
	for (;;)
	    {
	    char *e;
	    struct slName *n;
	    line = skipLeadingSpaces(line);
	    if (line == NULL || line[0] == 0)
	        break;
	    e = strchr(line, ',');
	    if (e == NULL)
	       e = strchr(line, ';');
	    if (e != NULL)
	       *e++ = 0;
	    n = lmSlName(lm, line);
	    slAddHead(&lit->authorList, n);
	    line = e;
	    }
	}
    else if (startsWith("RT", line))
        {
	char *s;
	groupLine(lf, line, line+5, dy);
	s = dy->string;
	stripLastChar(s);
	if (s[0] == '"')
	    parseQuotedString(s, s, NULL);
	lit->title = lmCloneString(lm, s);
	}
    else if (startsWith("RL", line))
        {
	groupLine(lf, line, line+5, dy);
	lit->cite = lmCloneString(lm, dy->string);
	}
    else 
        {
	lineFileReuse(lf);
	break;
	}
    }
slReverse(&lit->authorList);
slReverse(&lit->rcList);
slReverse(&lit->rxList);

/* Look up medline/pubmed IDs. */
lit->pubMedId = hashElFindVal(lit->rxList, "PubMed");
lit->medlineId = hashElFindVal(lit->rxList, "MEDLINE");
lit->doiId     = hashElFindVal(lit->rxList, "DOI");
slAddHead(&spr->literatureList, lit);
}

static void spReadSeq(struct lineFile *lf, struct lm *lm, 
	struct dyString *dy, struct spRecord *spr)
/* Read sequence and attach it to record. */
{
char *line;
dyStringClear(dy);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] != ' ')
	{
        lineFileReuse(lf);
	break;
	}
    stripChar(line, ' ');
    dyStringAppend(dy, line);
    }
spr->seq = lmCloneString(lm, dy->string);
}

static void spParseFeature(struct lineFile *lf, char *line, 
	struct lm *lm, struct dyString *dy, struct spRecord *spr)
/* Parse feature into record and hang it on spr. */
{
struct spFeature *feat;
char *class = nextWord(&line);
char *start = nextWord(&line);
char *end = nextWord(&line);
char *type = skipLeadingSpaces(line);
char *FTId;
char c;
if (end == NULL || end[0] == 0)
    errAbort("Short FT line %d of %s", lf->lineIx, lf->fileName);
lmAllocVar(lm, feat);
feat->class = lmCloneString(lm, class);
c = *start;
if (c == '<')
    {
    feat->softEndBits |= spFeatureStartLess;
    start += 1;
    }
else if (c == '?')
    {
    feat->softEndBits |= spFeatureStartFuzzy;
    start += 1;
    }
c = *end;
if (c == '>')
    {
    feat->softEndBits |= spFeatureEndGreater;
    end += 1;
    }
else if (c == '?')
    {
    feat->softEndBits |= spFeatureEndFuzzy;
    end += 1;
    }
feat->start = atoi(start)-1;
feat->end = atoi(end);

if (type != NULL)
    {
    /* Looks like multi-line type. */
    dyStringClear(dy);
    dyStringAppend(dy, type);
    while (lineFileNext(lf, &line, NULL))
	{
	char *sig = "FT    ";	/* Extra space after FT */
	if (!startsWith(sig, line))
	    {
	    lineFileReuse(lf);
	    break;
	    }
	line = skipLeadingSpaces(line+strlen(sig));
        FTId= strstr(line, "/FTId=");
    	if (FTId != NULL)
    	    {
    	    FTId = lmCloneString(lm, FTId+6);
	    stripLastPeriod(FTId);
	    feat->FTId = FTId;
	    }
        else
	    {
	    dyStringAppend(dy, line);
	    }
	}
    stripLastPeriod(dy->string);
    feat->type = lmCloneString(lm, dy->string);
    }
else
    {
    lineFileNext(lf, &line, NULL);
    FTId= strstr(line, "/FTId=");
    if (FTId != NULL)
    	{
    	FTId = lmCloneString(lm, FTId+6);
	stripLastPeriod(FTId);
	feat->FTId = FTId;
	}
    else
        {
	lineFileReuse(lf);
	}
    }
slAddHead(&spr->featureList, feat);
}

struct spRecord *spRecordNext(struct lineFile *lf, 
	struct lm *lm, 	/* Local memory pool for this structure. */
	struct dyString *dy)	/* Scratch string to use. */
/* Read next record from file and parse it into spRecord structure
 * that is allocated in memory. */
{
char *line, *word, *type;
struct spRecord *spr;
struct slName *n;

/* Parse ID line. */
    {
    char *row[5];
    if (!lineFileRow(lf, row))
	return NULL;
    if (!sameString(row[0], "ID"))
	errAbort("Expecting ID line %d of %s", lf->lineIx, lf->fileName);
    lmAllocVar(lm, spr);
    spr->id = lmCloneString(lm, row[1]);
    spr->isCurated = sameString(row[2], "STANDARD;");
    spr->aaSize = lineFileNeedNum(lf, row, 4);
    }

/* Loop around parsing until get '//' */
for (;;)
    {
    if (!lineFileNextReal(lf, &line))
        errAbort("%s ends in middle of a record", lf->fileName);
    /* uglyf("%d %s\n", lf->lineIx, line);*/
    type = line;
    line += 5;
    if (startsWith("FT", type))
        {
	spParseFeature(lf, line, lm, dy, spr);
	}
    else if (startsWith("DR", type))
        {
	struct spDbRef *dr;
	word = nextWord(&line);
	if (word == NULL)
	    errAbort("Short DR line %d of %s", lf->lineIx, lf->fileName);
	stripLastChar(word);
	lmAllocVar(lm, dr);
	dr->db = lmCloneString(lm, word);
	while ((word = nextWord(&line)) != NULL)
	    {
	    stripLastChar(word);
	    n = lmSlName(lm, word);
	    slAddHead(&dr->idList, n);
	    }
	slReverse(&dr->idList);
	slAddHead(&spr->dbRefList, dr);
	}
    else if (startsWith("DT", type))
        {
	char *date;
	date = nextWord(&line);
	if (date == NULL)
	    errAbort("Short DT line %d of %s", lf->lineIx, lf->fileName);
	if (endsWith(line, "Created)"))
	    spr->createDate = lmCloneString(lm, date);
	else if (endsWith(line, "Last sequence update)"))
	    spr->seqDate = lmCloneString(lm, date);
	else if (endsWith(line, "Last annotation update)"))
	    spr->annDate = lmCloneString(lm, date);
	else
	    {
	    errAbort("Unrecognized date type '%s' line %d of %s", 
	    	line, lf->lineIx, lf->fileName);
	    }
	}
    else if (startsWith("//", type))
        break;
    else if (startsWith("AC", type))
        {
	while ((word = nextWord(&line)) != NULL)
	    {
	    stripLastChar(word);	/* Cut of '.' or ';' */
	    n = lmSlName(lm, word);
	    slAddHead(&spr->accList, n);
	    }
	}
    else if (startsWith("DE", type))
        {
	groupLine(lf, type, line, dy);
	spr->description = lmCloneString(lm, dy->string);
	}
    else if (startsWith("OS", type))
        {
	char *common, *s;
	char *latin;
	groupLine(lf, type, line, dy);
	latin = dy->string;
	stripLastPeriod(latin);
	s = latin;
	while ((common = strchr(s, '(')) != NULL)
	    {
	    char *end = strchr(common, ')');
	    *common++ = 0;
	    if (end != NULL)
	        *end++ = 0;
	    else
	        break;
	    n = lmSlName(lm, common);
	    slAddHead(&spr->commonNames, n);
	    s = end;
	    }
	eraseTrailingSpaces(latin);
	spr->orgSciName = lmCloneString(lm, latin);
	}
    else if (startsWith("OC", type))
        {
	groupLine(lf, type, line, dy);
	subChar(dy->string, '\n', ' ');
	spr->taxonToGenus = lmCloneString(lm, dy->string);
	}
    else if (startsWith("OG", type))
        {
	groupLine(lf, type, line, dy);
	spr->organelle = lmCloneString(lm, dy->string);
	}
    else if (startsWith("OX", type))
        {
	char *sig = "NCBI_TaxID=";
	char *s;
	struct spTaxon *taxon;
	groupLine(lf, type, line, dy);
	s = dy->string;
	if (!startsWith(sig, s))
	    errAbort("Don't understand OX line %d of %s. Expecting %s", 
	    	lf->lineIx, lf->fileName, sig);
	s += strlen(sig);
	while ((word = nextWord(&s)) != NULL)
	    {
	    lmAllocVar(lm, taxon);
	    taxon->id = atoi(word);
	    slAddHead(&spr->taxonList, taxon);
	    }
	}
    else if (startsWith("RN", type))
        {
	spParseReference(lf, line, lm, dy, spr);
	}
    else if (startsWith("CC", type))
        {
	spParseComment(lf, line, lm, dy, spr);
	}
    else if (startsWith("KW", type))
        {
	char *end;
	stripLastPeriod(line);
	while (line != NULL)
	    {
	    end = strchr(line, ';');
	    if (end != NULL)
	       *end++ = 0;
	    line = trimSpaces(line);
	    n = lmSlName(lm, line);
	    slAddHead(&spr->keyWordList, n);
	    line = end;
	    }
	}
    else if (startsWith("SQ", type))
        {
	char *row[5];
	int wordCount;
	wordCount = chopLine(line, row);
	if (wordCount < 5)
	    errAbort("Short SQ line %d of %s", lf->lineIx, lf->fileName);
	if (!sameString(row[4], "MW;"))
	    errAbort("Expecting MW; field 6 line %d of %s, got %s", 
	    	lf->lineIx, lf->fileName, row[4]);
	spr->molWeight = atoi(row[3]);
	spReadSeq(lf, lm, dy, spr);
	}
    else if (startsWith("GN", type))
        {
	groupLine(lf, type, line, dy);
	spr->genes = lmCloneString(lm, dy->string);
	parseNameVals(lf, lm, dy->string, &spr->gnList);
	}
    else
        {
	errAbort("Unrecognized line %d of %s:\n%s",
		lf->lineIx, lf->fileName, type);
	}
    }
slReverse(&spr->accList);
slReverse(&spr->gnList);
slReverse(&spr->taxonList);
slReverse(&spr->commonNames);
slReverse(&spr->literatureList);
slReverse(&spr->commentList);
slReverse(&spr->dbRefList);
slReverse(&spr->keyWordList);
slReverse(&spr->featureList);
return spr;
}

/* ------------- Start main program ------------------  */

struct uniquer
/* Help manage a table that is simply unique. */
    {
    struct uniquer *next;
    struct hash *hash;
    int curId;
    FILE *f;
    };

struct uniquer *uniquerNew(FILE *f, int hashSize)
/* Make new uniquer structure. */
{
struct uniquer *uni;
AllocVar(uni);
uni->f = f;
uni->hash = newHash(hashSize);
return uni;
}
   
static char *nullPt = NULL;

int uniqueStore(struct uniquer *uni, char *name)
/* Store name in unique table.  Return id associated with name. */
{
if (name == NULL)
    return 0;
else
    {
    struct hash *hash = uni->hash;
    struct hashEl *hel = hashLookup(hash, name);

    if (hel != NULL)
	{
	return (char *)(hel->val) - nullPt;
	}
    else
	{
	uni->curId += 1;
	hashAdd(hash, name, nullPt + uni->curId);
	fprintf(uni->f, "%u\t%s\n", uni->curId, name);
	return uni->curId;
	}
    }
}

void toSqlDate(FILE *f, char *spDate)
/* Write out SwissProt data in MySQL format. 
 * SwissProt:  01-NOV-1990
 * MySQL:      1990-11-01 */
{
static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
char dup[13];
int monthIx;
char *day, *month, *year;
if (spDate == NULL)
    return;
strncpy(dup, spDate, sizeof(dup));
day = dup;
month = dup+3;
year = dup+7;
dup[2] = dup[6] = 0;
monthIx = stringIx(month, months);
if (monthIx < 0)
   errAbort("Strange month %s", month);
fprintf(f, "%s-%02d-%s", year, monthIx+1, day);
}

char *blankForNull(char *s)
/* Return s, or "" if it is NULL */
{
if (s == NULL)
    return "";
else
    return s;
}

char *nextGeneWord(char **pS)
/* Return next gene, which is "," separated. */
{
char *start = skipLeadingSpaces(*pS);
char *next;

if (start == NULL || start[0] == 0)
    return NULL;
next = stringIn(",", start);
if (next != NULL)
    {
    *next = 0;
    next += 1;
    }
*pS = next;
return start;
}

FILE *createAt(char *dir, char *file)
/* Open file in dir. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s.txt", dir, file);
return mustOpen(path, "w");
}

void spToDb(char *datFile, char *tabDir)
/* spToDb - Create a relational database out of SwissProt/trEMBL flat files. */
{
struct lineFile *lf = lineFileOpen(datFile, TRUE);
struct spRecord *spr;
struct dyString *dy = newDyString(4096);

/* We have 25 tables to make this fully relational and not
 * lose any info. Better start opening files. */
makeDir(tabDir);
FILE *displayId = createAt(tabDir, "displayId");
FILE *otherAcc = createAt(tabDir, "otherAcc");
FILE *organelle = createAt(tabDir, "organelle");
FILE *info = createAt(tabDir, "info");
FILE *description = createAt(tabDir, "description");
FILE *geneLogic = createAt(tabDir, "geneLogic");
FILE *gene = createAt(tabDir, "gene");
FILE *taxon = createAt(tabDir, "taxon");
FILE *accToTaxon = createAt(tabDir, "accToTaxon");
FILE *commonName = createAt(tabDir, "commonName");
FILE *keyword = createAt(tabDir, "keyword");
FILE *accToKeyword = createAt(tabDir, "accToKeyword");
FILE *commentType = createAt(tabDir, "commentType");
FILE *commentVal = createAt(tabDir, "commentVal");
FILE *comment = createAt(tabDir, "comment");
FILE *protein = createAt(tabDir, "protein");
FILE *extDb = createAt(tabDir, "extDb");
FILE *extDbRef = createAt(tabDir, "extDbRef");
FILE *featureClass = createAt(tabDir, "featureClass");
FILE *featureType = createAt(tabDir, "featureType");
FILE *featureId = createAt(tabDir, "featureId");
FILE *feature = createAt(tabDir, "feature");
FILE *author = createAt(tabDir, "author");
FILE *reference = createAt(tabDir, "reference");
FILE *referenceAuthors = createAt(tabDir, "referenceAuthors");
FILE *citationRp = createAt(tabDir, "citationRp");
FILE *citation = createAt(tabDir, "citation");
FILE *rcType = createAt(tabDir, "rcType");
FILE *rcVal = createAt(tabDir, "rcVal");
FILE *citationRc = createAt(tabDir, "citationRc");

/* Some of the tables require unique IDs */
struct uniquer *organelleUni = uniquerNew(organelle, 14);
struct uniquer *keywordUni = uniquerNew(keyword, 14);
struct uniquer *commentTypeUni = uniquerNew(commentType, 10);
struct uniquer *commentValUni = uniquerNew(commentVal, 18);
struct uniquer *extDbUni = uniquerNew(extDb, 8);
struct uniquer *featureClassUni = uniquerNew(featureClass, 10);
struct uniquer *featureTypeUni = uniquerNew(featureType, 14);
struct uniquer *featureIdUni = uniquerNew(featureId, 14);
struct uniquer *authorUni = uniquerNew(author, 18);
struct uniquer *referenceUni = uniquerNew(reference, 18);
struct uniquer *citationRpUni = uniquerNew(citationRp, 18);
struct uniquer *rcTypeUni = uniquerNew(rcType, 14);
struct uniquer *rcValUni = uniquerNew(rcVal, 18);

/* Other unique helpers. */
struct hash *taxonHash = newHash(18);
struct hash *taxonIdHash = newHash(18);
int citationId = 0;

for (;;)
    {
    struct lm *lm = lmInit(8*1024);
    char *acc;
    struct slName *n;
    int organelleId;

    spr = spRecordNext(lf, lm, dy);
    if (spr == NULL)
        break;
    acc = spr->accList->name;

    /* displayId */
    fprintf(displayId, "%s\t%s\n", acc, spr->id);

    /* otherAcc */
    for (n = spr->accList->next; n != NULL; n = n->next)
        fprintf(otherAcc, "%s\t%s\n", acc, n->name);

    /* organelle */
    organelleId = uniqueStore(organelleUni, spr->organelle);

    /* info */
    fprintf(info, "%s\t%d\t%d\t%d\t", 
    	acc, spr->isCurated, spr->aaSize, spr->molWeight);
    toSqlDate(info, spr->createDate);
    fputc('\t', info);
    toSqlDate(info, spr->seqDate);
    fputc('\t', info);
    toSqlDate(info, spr->annDate);
    fputc('\t', info);
    fprintf(info, "%d\n", organelleId);

    /* description */
    if (spr->description != NULL)
	{
	subChar(spr->description, '\t', ' ');
	fprintf(description, "%s\t%s\n", acc, spr->description);
	}
	
    /* gene logic and gene */
    if (spr->gnList != NULL)
	{
	struct hashEl *hel;
	char *s = spr->genes, *word;
	char *gn;
	stripLastPeriod(s);
	fprintf(geneLogic, "%s\t%s\n", acc, s);
	for (hel = spr->gnList; hel != NULL; hel = hel->next)
	    {
	    gn = (char *) hel->val;
	    /* separte gene names if there are multiple gene names */ 
	    for (;;)
	    	{
	    	word = nextGeneWord(&gn);
	    	if (word == NULL)
	            break;
	    	fprintf(gene, "%s\t%s\n", acc, word);
	    	}
	    }
 	}
    
    /* taxon, commonName, accToTaxon */
    if (spr->taxonList != NULL)
        {
	struct spTaxon *tax;
	if (slCount(spr->taxonList) == 1)
	    {
	    /* Swiss prot only has full info on the first taxa when it
	     * contains multiple taxons, so we have to rely on NCBI here... */
	    int ncbiId = spr->taxonList->id;
	    char ncbiIdX[16];
	    safef(ncbiIdX, sizeof(ncbiIdX), "%x", ncbiId);
	    if (!hashLookup(taxonHash, spr->orgSciName) 
	    	|| !hashLookup(taxonIdHash, ncbiIdX))
	        {
		hashAdd(taxonHash, spr->orgSciName, NULL);
		hashAdd(taxonIdHash, ncbiIdX, NULL);
		fprintf(taxon, "%d\t%s\t%s\n",
			ncbiId, spr->orgSciName, spr->taxonToGenus);
		for (n = spr->commonNames; n != NULL; n = n->next)
		    fprintf(commonName, "%d\t%s\n", ncbiId, n->name);
		}
	    }
	for (tax = spr->taxonList; tax != NULL; tax = tax->next)
	    fprintf(accToTaxon, "%s\t%d\n", acc, tax->id);
	}

    /* keyword and accToKeyword */
    for (n = spr->keyWordList; n != NULL; n = n->next)
        {
	int id = uniqueStore(keywordUni, n->name);
	fprintf(accToKeyword, "%s\t%d\n", acc, id);
	}
    
    /* commentType, commenVal, and comment. */
        {
	struct spComment *spCom;
	for (spCom = spr->commentList; spCom != NULL; spCom = spCom->next)
	    {
	    int commentType = uniqueStore(commentTypeUni, spCom->type);
	    int commentVal = uniqueStore(commentValUni, spCom->text);
	    fprintf(comment, "%s\t%d\t%d\n", acc, commentType, commentVal);
	    }
	}

    /* protein */
    fprintf(protein, "%s\t%s\n", acc, spr->seq);

    /* extDb and extDbRef */
        {
	struct spDbRef *ref;
	for (ref = spr->dbRefList; ref != NULL; ref = ref->next)
	    {
	    int extDb = uniqueStore(extDbUni, ref->db);
	    int i = 0;
	    char *extAccs[3];
	    for (i=0; i<ArraySize(extAccs); ++i)
	        extAccs[i] = "";
	    for (n = ref->idList, i=0; n != NULL && i<3; n = n->next, ++i)
		extAccs[i] = n->name;
	    fprintf(extDbRef, "%s\t%d\t%s\t%s\t%s\n", acc, extDb, 
	        extAccs[0], extAccs[1], extAccs[2]);
	    }
	}

    /* featureClass, featureType, and feature */
        {
	struct spFeature *feat;
	for (feat = spr->featureList; feat != NULL; feat = feat->next)
	    {
	    int class = uniqueStore(featureClassUni, feat->class);
	    int type = uniqueStore(featureTypeUni, feat->type);
	    int Id   = uniqueStore(featureIdUni, feat->FTId);
	    fprintf(feature, "%s\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    	acc, feat->start, feat->end, class, type, feat->softEndBits, Id);
	    }
	}

    /* citation, reference, author, and related tables. */
        {
	struct spLitRef *ref;
	struct hashEl *hel;
	int refId;
	for (ref = spr->literatureList; ref != NULL; ref = ref->next)
	    {
	    /* Do get reference ID and if necessary output new reference and other
	     * tables. */
	    if ((hel = hashLookup(referenceUni->hash, ref->cite)) != NULL)
		{
		refId = (char *)hel->val - nullPt;
		}
	    else
	        {
		referenceUni->curId += 1;
		refId = referenceUni->curId;
		hashAddInt(referenceUni->hash, ref->cite, refId);
		if (ref->title)
		    subChar(ref->title, '\t', ' ');
		fprintf(reference, "%d\t%s\t%s\t%s\t%s\t%s\n", 
		    refId, blankForNull(ref->title), ref->cite, 
		    blankForNull(ref->pubMedId), blankForNull(ref->medlineId), blankForNull(ref->doiId));
		for (n = ref->authorList; n != NULL; n = n->next)
		    {
		    int authorId = uniqueStore(authorUni, n->name);
		    fprintf(referenceAuthors, "%d\t%d\n", refId, authorId);
		    }
		}

            /* Do citation and related tables. */
	        {
		int rpId = uniqueStore(citationRpUni, ref->rp);
		++citationId;
		fprintf(citation, "%d\t%s\t%d\t%d\n",
			citationId, acc, refId, rpId);

		for (hel = ref->rcList; hel != NULL; hel = hel->next)
		    {
		    int rcTypeId = uniqueStore(rcTypeUni, hel->name);
		    int rcValId = uniqueStore(rcValUni, hel->val);
		    fprintf(citationRc, "%d\t%d\t%d\n", 
		    	citationId, rcTypeId, rcValId);
		    }
		}
	    }
	}

    lmCleanup(&lm);
    }
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
spToDb(argv[1], argv[2]);
return 0;
}
