/* spToDb - Create a relational database out of SwissProt/trEMBL flat files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"

static char const rcsid[] = "$Id: spToDb.c,v 1.1 2003/09/29 09:04:34 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spToDb - Create a relational database out of SwissProt/trEMBL flat files\n"
  "usage:\n"
  "   spToDb db swissProt.dat\n"
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
    char *orgSciName;	/* Organism scientific name. */
    char *orgCommonName;/* Organism common name. */
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
    };

struct spLitRef 
/* A swissProt literature reference. */
    {
    struct spLitRef *next;
    char *title;	/* Title of article. */
    char *cite;		/* Journal/book/patent citation. */
    struct slName *authorList;	/* Author names in lastName, F.M. format. */
    char *rp;		/* Somewhat complex 'Reference Position' line. */
    struct hashEl *rcList; /* TISSUE=XXX X; STRAIN=YYY; parsed out. */
    struct hashEl *rxList; /* Cross-references. */
    };

static void spParseComment(struct lineFile *lf, char *line, 
	struct lm *lm, struct dyString *dy, struct spRecord *spr)
/* Parse comment into records and hang them on spr. */
{
struct spComment *com = NULL;
char *word;
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
	word = nextWord(&line);	/* Skip -!- */
	word = nextWord(&line);
	if (word != NULL)
	    {
	    int len = strlen(word);
	    word[len-1] = 0;	/* Strip ':' */
	    lmAllocVar(lm, com);
	    com->type = lmCloneString(lm, word);
	    slAddHead(&spr->commentList, com);
	    dyStringClear(dy);
	    dyStringAppend(dy, skipLeadingSpaces(line));
	    }
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
	    dyStringAppend(dy, line);
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
struct hashEl *hel;
while (line != NULL && line[0] != 0)
    {
    name = skipLeadingSpaces(line);
    val = strchr(name, '=');
    if (val == NULL)
        errAbort("Expecting = line %d of %s", lf->lineIx, lf->fileName);
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
}

static void spParseReference(struct lineFile *lf, char *line, 
	struct lm *lm, struct dyString *dy, struct spRecord *spr)
/* Parse refence into record and hang it on spr. */
{
struct spLitRef *lit;
char *name, *val, *e;

/* We just ignore the RN line.  It is implicit in order in list. */
lmAllocVar(lm, lit);

while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("RP", line))
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
	groupLine(lf, line, line+5, dy);
	lit->title = lmCloneString(lm, dy->string);
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
if (end == NULL || end[0] == 0)
    errAbort("Short FT line %d of %s", lf->lineIx, lf->fileName);
lmAllocVar(lm, feat);
feat->class = lmCloneString(lm, class);
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
	dyStringAppend(dy, line);
	}
    if (dy->string[dy->stringSize-1] == '.')
        {
	dy->string[dy->stringSize-1] = 0;
	dy->stringSize -= 1;
	}
    feat->type = lmCloneString(lm, dy->string);
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
    // uglyf("%d %s\n", lf->lineIx, line);
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
	lmAllocVar(lm, dr);
	dr->db = lmCloneString(lm, word);
	while ((word = nextWord(&line)) != NULL)
	    {
	    int len = strlen(word);
	    word[len-1] = 0;	/* Strip off ; or . */
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
	    spr->seqDate = lmCloneString(lm, date);
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
	    int len = strlen(word);
	    /* Cut off '.' or ';' */
	    word[len-1] = 0;
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
	char *common;
	char *latin;
	char *end;
	groupLine(lf, type, line, dy);
	latin = dy->string;
	common = strchr(latin, '(');
	if (common == NULL)
	    {
	    common = latin;
	    end = strchr(latin, '.');
	    }
	else
	    {
	    *common++ = 0;
	    end = strchr(common, ')');
	    }
	if (end != NULL)
	    *end = 0;
	spr->orgSciName = lmCloneString(lm, latin);
	spr->orgCommonName = lmCloneString(lm, common);
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
	while ((word = nextWord(&line)) != NULL)
	    {
	    int len = strlen(word);
	    word[len-1] = 0;	/* Get rid of ';' or '.' */
	    n = lmSlName(lm, word);
	    slAddHead(&spr->keyWordList, n);
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
	}
    else
        {
	errAbort("Unrecognized line %d of %s:\n%s",
		lf->lineIx, lf->fileName, type);
	}
    }
slReverse(&spr->accList);
slReverse(&spr->taxonList);
slReverse(&spr->literatureList);
slReverse(&spr->commentList);
slReverse(&spr->dbRefList);
slReverse(&spr->keyWordList);
slReverse(&spr->featureList);
return spr;
}

void spToDb(char *database, char *datFile)
/* spToDb - Create a relational database out of SwissProt/trEMBL flat files. */
{
struct lineFile *lf = lineFileOpen(datFile, TRUE);
struct hash *wordHash = newHash(16);	/* Small words. */
struct spRecord *spr;
struct dyString *dy = newDyString(4096);

for (;;)
    {
    struct lm *lm = lmInit(4096);
    spr = spRecordNext(lf, lm, dy);
    if (spr == NULL)
        break;
    uglyf("%s %d %s %s %s\n", spr->id, spr->isCurated, spr->accList->name,
       spr->orgSciName, spr->orgCommonName);
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
