/* genePixLoad - Load data into genePix database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "jksql.h"
#include "dystring.h"

/* Variables you can override from command line. */
char *database = "genePix";
boolean replace = FALSE;
boolean multicolor = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePixLoad - Load data into genePix database\n"
  "usage:\n"
  "   genePixLoad setInfo.ra itemInfo.tab\n"
  "Please see genePixLoad.doc for description of the .ra and .tab files\n"
  "Options:\n"
  "   -database=%s - Specifically set database\n"
  "   -replace - Replace image rather than complaining if it exists\n"
  "   -multicolor - More than one probe of different color on image\n"
  "                 (Use with care, disables some error checking)\n"
  , database
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING,},
   {"replace", OPTION_BOOLEAN,},
   {"multicolor", OPTION_BOOLEAN,},
   {NULL, 0},
};

struct hash *hashRowOffsets(char *line)
/* Given a space-delimited line, create a hash keyed by the words in 
 * line with values the position of the word (0 based) in line */
{
struct hash *hash = hashNew(0);
char *word;
int wordIx = 0;
while ((word = nextWord(&line)) != 0)
    {
    hashAdd(hash, word, intToPt(wordIx));
    wordIx += 1;
    }
return hash;
}

char *getVal(char *fieldName, struct hash *raHash, struct hash *rowHash, char **row, char *defaultVal)
/* Return value in row if possible, else in ra, else in default.  If no value and no default
 * return an error. */
{
char *val = NULL;
struct hashEl *hel = hashLookup(rowHash, fieldName);
if (hel != NULL)
    {
    int rowIx = ptToInt(hel->val);
    val = row[rowIx];
    }
else
    {
    val = hashFindVal(raHash, fieldName);
    if (val == NULL)
	{
	if (defaultVal != NULL)
	    val = defaultVal;
	else
	    errAbort("Can't find value for field %s", fieldName);
	}
    }
return val;
}

static char *requiredItemFields[] = {"fileName", "submitId"};
static char *requiredSetFields[] = {"contributor"};
static char *requiredFields[] = {"fullDir", "screenDir", "thumbDir", "taxon", "isEmbryo", 
	"age", "bodyPart", "sliceType", "probeColor", };
static char *optionalFields[] = {
	"sectionSet", "sectionIx", "gene", "locusLink", "refSeq", "genbank", 
	"submitId", "fPrimer", "rPrimer", "seq", "abName", "abDescription",  "abTaxon",
	"journal", "journalUrl"};

char *hashValOrDefault(struct hash *hash, char *key, char *defaultVal)
/* Lookup key in hash and return value, or return default if it doesn't exist. */
{
char *val = hashFindVal(hash, key);
if (val == NULL)
    val = defaultVal;
return val;
}

int findExactSubmissionId(struct sqlConnection *conn,
	char *contributors, char *publication, char *pubUrl, 
	char *setUrl, char *itemUrl)
/* Find ID of submissionSet that matches all parameters.  Return 0 if none found. */
{
char query[1024];
safef(query, sizeof(query),
      "select id from submissionSet "
      "where contributors = \"%s\" "
      "and publication = \"%s\" "
      "and pubUrl = '%s' and setUrl = '%s' and itemUrl = '%s'"
      , contributors, publication, pubUrl, setUrl, itemUrl);
uglyf("query %s\n", query);
return sqlQuickNum(conn, query);
}

int findOrAddIdTable(struct sqlConnection *conn, char *table, char *field, 
	char *value)
/* Get ID associated with field.value in table.  */
{
char query[256];
int id;
safef(query, sizeof(query), "select id from %s where %s = \"%s\"",
	table, field, value);
id = sqlQuickNum(conn, query);
if (id == 0)
    {
    safef(query, sizeof(query), "insert into %s values(default, \"%s\")",
    	table, value);
    uglyf("%s\n", query);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    }
return id;
}

int doJournal(struct sqlConnection *conn, char *name, char *url)
/* Update journal table if need be.  Return journal ID. */
{
if (name[0] == 0)
    return 0;
else
    {
    struct dyString *dy = dyStringNew(0);
    int id = 0;

    dyStringPrintf(dy, "select id from journal where name = '%s'", name);
    id = sqlQuickNum(conn, dy->string);
    if (id == 0)
	{
	dyStringClear(dy);
	dyStringPrintf(dy, "insert into journal set");
	dyStringPrintf(dy, " id=default,\n");
	dyStringPrintf(dy, " name=\"%s\",\n", name);
	dyStringPrintf(dy, " url=\"%s\"\n", url);
	uglyf("%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	id = sqlLastAutoId(conn);
	}
    dyStringFree(&dy);
    return id;
    }
}

int createSubmissionId(struct sqlConnection *conn,
	char *contributors, char *publication, 
	char *pubUrl, char *setUrl, char *itemUrl,
	char *journal, char *journalUrl)
/* Add submission and contributors to database and return submission ID */
{
struct slName *slNameListFromString(char *s, char delimiter);
struct slName *contribList = NULL, *contrib;
int submissionSetId;
int journalId = doJournal(conn, journal, journalUrl);
struct dyString *dy = dyStringNew(0);
char query[1024];

dyStringPrintf(dy, "insert into submissionSet set");
dyStringPrintf(dy, " id = default,\n");
dyStringPrintf(dy, " contributors = \"%s\",\n", contributors);
dyStringPrintf(dy, " publication = \"%s\",\n", publication);
dyStringPrintf(dy, " pubUrl = \"%s\",\n", pubUrl);
dyStringPrintf(dy, " journal = %d,\n", journalId);
dyStringPrintf(dy, " setUrl = \"%s\",\n", setUrl);
dyStringPrintf(dy, " itemUrl = \"%s\"\n", itemUrl);
uglyf("%s\n", dy->string);
sqlUpdate(conn, dy->string);
submissionSetId = sqlLastAutoId(conn);

contribList = slNameListFromComma(contributors);
for (contrib = contribList; contrib != NULL; contrib = contrib->next)
    {
    int contribId = findOrAddIdTable(conn, "contributor", "name", 
    	skipLeadingSpaces(contrib->name));
    safef(query, sizeof(query),
          "insert into submissionContributor values(%d, %d)",
	  submissionSetId, contribId);
    uglyf("%s\n", query);
    sqlUpdate(conn, query);
    }
slFreeList(&contribList);
dyStringFree(&dy);
return submissionSetId;
}

int saveSubmissionSet(struct sqlConnection *conn, struct hash *raHash)
/* Create submissionSet, submissionContributor, and contributor records. */
{
char *contributor = hashMustFindVal(raHash, "contributor");
char *publication = hashValOrDefault(raHash, "publication", "");
char *pubUrl = hashValOrDefault(raHash, "pubUrl", "");
char *setUrl = hashValOrDefault(raHash, "setUrl", "");
char *itemUrl = hashValOrDefault(raHash, "itemUrl", "");
char *journal = hashValOrDefault(raHash, "journal", "");
char *journalUrl = hashValOrDefault(raHash, "journalUrl", "");
int submissionId = findExactSubmissionId(conn, contributor, 
	publication, pubUrl, setUrl, itemUrl);
if (submissionId != 0)
     return submissionId;
else
     return createSubmissionId(conn, contributor, 
     	publication, pubUrl, setUrl, itemUrl, journal, journalUrl);
}

int cachedId(struct sqlConnection *conn, char *tableName, char *fieldName,
	struct hash *cache, char *raFieldName, struct hash *raHash, 
	struct hash *rowHash, char **row)
/* Get value for named field, and see if it exists in table.  If so
 * return associated id, otherwise create new table entry and return 
 * that id. */
{
char *value = getVal(raFieldName, raHash, rowHash, row, "");
if (value[0] == 0)
    return 0;
return findOrAddIdTable(conn, tableName, fieldName, value);
}


boolean optionallyAddOr(struct dyString *dy, char *field, char *val, 
	boolean needOr)
/* Add field = 'val', optionally preceded by or to dy. */
{
if (val[0] == 0)
    return needOr;
if (needOr)
    dyStringAppend(dy, " or ");
dyStringPrintf(dy, "%s = '%s'", field, val);
return TRUE;
}

int doSectionSet(struct sqlConnection *conn, struct hash *sectionSetHash, 
	char *sectionSet)
/* Update section set table if necessary.  Return section set id */
{
int sectionId = 0;
if (sectionSet[0] != 0 && !sameString(sectionSet, "0"))
    {
    struct hashEl *hel = hashLookup(sectionSetHash, sectionSet);
    if (hel != NULL)
	sectionId = ptToInt(hel->val);
    else
	{
	sqlUpdate(conn, "insert into sectionSet values(default)");
	sectionId = sqlLastAutoId(conn);
	hashAdd(sectionSetHash, sectionSet, intToPt(sectionId));
	}
    }
return sectionId;
}

int doGene(struct lineFile *lf, struct sqlConnection *conn,
	char *gene, char *locusLink, char *refSeq, 
	char *uniProt, char *genbank, char *taxon)
/* Update gene table if necessary, and return gene ID. */
{
struct dyString *dy = dyStringNew(0);
boolean needOr = FALSE;
int geneId = 0;
int bestScore = 0;
struct sqlResult *sr;
char **row;

/* Make sure that at least one field specifying gene is
 * specified. */
if (gene[0] == 0 && locusLink[0] == 0 && refSeq[0] == 0
    && uniProt[0] == 0 && genbank[0] == 0)
    errAbort("No gene, locusLink, refSeq, uniProt, or genbank "
	     " specified line %d of %s", lf->lineIx, lf->fileName);

/* Create query string that will catch relevant existing genes. */
dyStringPrintf(dy, "select id,name,locusLink,refSeq,genbank,uniProt from gene ");
dyStringPrintf(dy, "where taxon = %s and (",  taxon);
needOr = optionallyAddOr(dy, "name", gene, needOr);
needOr = optionallyAddOr(dy, "locusLink", locusLink, needOr);
needOr = optionallyAddOr(dy, "refSeq", refSeq, needOr);
needOr = optionallyAddOr(dy, "uniProt", uniProt, needOr);
needOr = optionallyAddOr(dy, "genbank", genbank, needOr);
dyStringPrintf(dy, ")");
uglyf("query %s\n", dy->string);

/* Loop through query results finding ID that best matches us.
 * The locusLink/refSeq ID's are worth 8,  the genbank 4, the
 * uniProt 2, and the name 1.  This scheme will allow different
 * genes with the same name and even the same uniProt ID to 
 * cope with alternative splicing.  */
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]);
    int score = 0;
    char *nameOne = row[1];
    char *locusLinkOne = row[2];
    char *refSeqOne = row[3];
    char *genbankOne = row[4];
    char *uniProtOne = row[5];

    /* If disagree on locusLink, or refSeq ID, then this is not the
     * gene for us */
    if (locusLink[0] != 0 && locusLinkOne[0] != 0 
	&& differentString(locusLink, locusLinkOne))
	continue;
    if (refSeq[0] != 0 && refSeqOne[0] != 0 
	&& differentString(refSeq, refSeqOne))
	continue;

    /* If we've agreed on locusLink or refSeq ID, then this is the
     * gene for us. */
    if (locusLink[0] != 0 || refSeq[0] != 0)
	score += 8;

    if (genbank[0] != 0 && genbankOne[0] != 0
	&& sameString(genbank, genbankOne))
	score += 4;
    if (uniProt[0] != 0 && uniProtOne[0] != 0
	&& sameString(uniProt, uniProtOne))
	score += 2;
    if (gene[0] != 0 && nameOne[0] != 0
	&& sameString(gene, nameOne))
	score += 1;
    if (score > bestScore)
	{
	bestScore = score;
	geneId = id;
	}
    }
sqlFreeResult(&sr);

if (geneId == 0)
   {
   dyStringClear(dy);
   dyStringAppend(dy, "insert into gene set\n");
   dyStringPrintf(dy, " id = default,\n");
   dyStringPrintf(dy, " name = '%s',\n", gene);
   dyStringPrintf(dy, " locusLink = '%s',\n", locusLink);
   dyStringPrintf(dy, " refSeq = '%s',\n", refSeq);
   dyStringPrintf(dy, " genbank = '%s',\n", genbank);
   dyStringPrintf(dy, " uniProt = '%s',\n", uniProt);
   dyStringPrintf(dy, " taxon = %s\n", taxon);
   uglyf("%s\n", dy->string);
   sqlUpdate(conn, dy->string);
   geneId = sqlLastAutoId(conn);
   }
else
   {
   char *oldName;

   /* Handle updating name field - possibly creating a synonym. */
   dyStringClear(dy);
   dyStringPrintf(dy, "select name from gene where id = %d", geneId);
   oldName = sqlQuickString(conn, dy->string);
   if (gene[0] != 0)
       {
       if (oldName[0] == 0)
	   {
	   dyStringClear(dy);
	   dyStringPrintf(dy, "update gene set name = '%s'");
	   dyStringPrintf(dy, " name = '%s',", gene);
	   dyStringPrintf(dy, " where id = %d", geneId);
	   uglyf("%s\n", dy->string);
	   sqlUpdate(conn, dy->string);
	   }
       else if (differentString(oldName, gene))
	   {
	   dyStringClear(dy);
	   dyStringAppend(dy, "select count(*) from geneSynonym where ");
	   dyStringPrintf(dy, "gene = %d and name = '%s'", geneId, gene);
	   if (sqlQuickNum(conn, dy->string) == 0)
	       {
	       dyStringClear(dy);
	       dyStringPrintf(dy, "insert into geneSynonym "
			     "values(%d,\"%s\")", geneId, gene);
	       uglyf("%s\n", dy->string);
	       sqlUpdate(conn, dy->string);
	       }
	   }
       }

   /* Update other fields. */
   dyStringClear(dy);
   dyStringAppend(dy, "update gene set\n");
   if (locusLink[0] != 0)
       dyStringPrintf(dy, " locusLink = '%s',", locusLink);
   if (refSeq[0] != 0)
       dyStringPrintf(dy, " refSeq = '%s',", refSeq);
   if (genbank[0] != 0)
       dyStringPrintf(dy, " genbank = '%s',", genbank);
   if (uniProt[0] != 0)
       dyStringPrintf(dy, " uniProt = '%s',", uniProt);
   dyStringPrintf(dy, " taxon = %s", taxon);
   dyStringPrintf(dy, " where id = %d", geneId);
   uglyf("%s\n", dy->string);
   sqlUpdate(conn, dy->string);
   geneId = geneId;
   freez(&oldName);
   }
dyStringFree(&dy);
return geneId;
}

int doAntibody(struct sqlConnection *conn, char *abName, char *abDescription,
	char *abTaxon)
/* Update antibody table if necessary and return antibody ID. */
{
int antibodyId = 0;

if (abName[0] != 0)
    {
    int bestScore = 0;
    struct sqlResult *sr;
    char **row;
    struct dyString *dy = dyStringNew(0);

    /* Try to hook up with existing antibody record. */
    dyStringAppend(dy, "select id,name,description,taxon from antibody");
    dyStringPrintf(dy, " where name = '%s'", abName);
    sr = sqlGetResult(conn, dy->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	int id = sqlUnsigned(row[0]);
	char *name = row[1];
	char *description = row[2];
	char *taxon = row[3];
	int score = 1;
	if (abTaxon[0] != '0' && taxon[0] != '0')
	    {
	    if (differentString(taxon, abTaxon))
		continue;
	    else
		score += 2;
	    }
	if (description[0] != 0 && abDescription[0] != 0)
	    {
	    if (differentString(description, abDescription))
		continue;
	    else
		score += 4;
	    }
	if (score > bestScore)
	    {
	    bestScore = score;
	    antibodyId = id;
	    }
	}
    sqlFreeResult(&sr);

    if (antibodyId == 0)
	{
	dyStringClear(dy);
	dyStringAppend(dy, "insert into antibody set");
	dyStringPrintf(dy, " id = default,\n");
	dyStringPrintf(dy, " name = '%s',\n", abName);
	dyStringPrintf(dy, " description = \"%s\",\n", abDescription);
	dyStringPrintf(dy, " taxon = %s", abTaxon);
	uglyf("%s\n", dy->string);
	sqlUpdate(conn, dy->string);
        antibodyId = sqlLastAutoId(conn);
	}
    else
	{
	if (abDescription[0] != 0)
	    {
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update antibody set description = \"%s\"",
		    abDescription);
	    dyStringPrintf(dy, " where id = %d", antibodyId);
	    uglyf("%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	if (abTaxon[0] != 0)
	    {
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update antibody set taxon = %s", abTaxon);
	    dyStringPrintf(dy, " where id = %d", antibodyId);
	    uglyf("%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	}
    dyStringFree(&dy);
    }
return antibodyId;
}

int doProbe(struct lineFile *lf, struct sqlConnection *conn, 
	int geneId, int antibodyId,
	char *fPrimer, char *rPrimer, char *seq)
/* Update probe table and probeType table if need be and return probe ID. */
{
struct sqlResult *sr;
char **row;
int probeTypeId = 0;
int probeId = 0;
int bestScore = 0;
struct dyString *dy = dyStringNew(0);
char *probeType = "RNA";
boolean gotPrimers = (rPrimer[0] != 0 && fPrimer[0] != 0);

touppers(fPrimer);
touppers(rPrimer);
touppers(seq);
if (gotPrimers || seq[0] != 0)
    {
    if (antibodyId != 0)
        errAbort("Probe defined by antibody and RNA line %d of %s",
		lf->lineIx, lf->fileName);
    }
else if (antibodyId)
    {
    probeType = "antibody";
    }

/* Handle probe type */
dyStringPrintf(dy, "select id from probeType where name = '%s'", probeType);
probeTypeId = sqlQuickNum(conn, dy->string);
if (probeTypeId == 0)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "insert into probeType values(default, '%s')", 
    	probeType);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    probeTypeId  = sqlLastAutoId(conn);
    }

dyStringClear(dy);
dyStringAppend(dy, "select id,gene,antibody,probeType,fPrimer,rPrimer,seq ");
dyStringAppend(dy, "from probe ");
dyStringPrintf(dy, "where gene=%d and antibody=%d", geneId, antibodyId);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int idOne = sqlUnsigned(row[0]);
    int geneOne = sqlUnsigned(row[1]);
    int antibodyOne = sqlUnsigned(row[2]);
    char *fPrimerOne = row[3];
    char *rPrimerOne = row[4];
    char *seqOne = row[5];
    int score = 1;
    if (antibodyId != 0)
        {
	if (antibodyOne == antibodyId)
	    score += 10;
	else
	    continue;
	}
    else
        {
	if (antibodyOne != 0)
	    continue;
	}
    if (gotPrimers && fPrimerOne[0] != 0 && rPrimerOne[0] != 0)
        {
	if (sameString(fPrimerOne, fPrimer) && 
	    sameString(rPrimerOne, rPrimer))
	    score += 2;
	else
	    continue;
	}
    if (seq[0] != 0  && seqOne[0] != 0)
        {
	if (sameString(seq, seqOne))
	    score += 4;
	else
	    continue;
	}
    if (score > bestScore)
	{
        probeId = idOne;
	bestScore = score;
	}
    }
sqlFreeResult(&sr);


if (probeId == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into probe set");
    dyStringPrintf(dy, " id=default,\n");
    dyStringPrintf(dy, " gene=%d,\n", geneId);
    dyStringPrintf(dy, " antibody=%d,\n", antibodyId);
    dyStringPrintf(dy, " probeType=%d,\n", probeTypeId);
    dyStringPrintf(dy, " fPrimer='%s',\n", fPrimer);
    dyStringPrintf(dy, " rPrimer='%s',\n", rPrimer);
    dyStringPrintf(dy, " seq='%s'\n", seq);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    probeId = sqlLastAutoId(conn);
    }
else
    {
    if (antibodyId != 0)
	{
	dyStringClear(dy);
	dyStringPrintf(dy, "update probe set antibody=%d", antibodyId);
	dyStringPrintf(dy, " where id=%d", probeId);
	uglyf("%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    if (gotPrimers)
	{
	dyStringClear(dy);
	dyStringAppend(dy, "update probe set ");
	dyStringPrintf(dy, "fPrimer = '%s', ", fPrimer);
	dyStringPrintf(dy, "rPrimer = '%s'", rPrimer);
	dyStringPrintf(dy, " where id=%d", probeId);
	uglyf("%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    if (seq[0] != 0)
        {
	dyStringClear(dy);
	dyStringAppend(dy, "update probe set seq = '");
	dyStringAppend(dy, seq);
	dyStringAppend(dy, "'");
	dyStringPrintf(dy, " where id=%d", probeId);
	uglyf("%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    }

dyStringFree(&dy);
return probeId;
}

int doImageFile(struct lineFile *lf, struct sqlConnection *conn, 
	char *fileName, int fullDir, int screenDir, int thumbDir,
	int submissionSetId, char *submitId, char *priority)
/* Update image file record if necessary and return image file ID. */
{
int imageFileId = 0;
struct dyString *dy = newDyString(0);

dyStringAppend(dy, "select id from imageFile where ");
dyStringPrintf(dy, "fileName='%s' and fullLocation=%d", 
	fileName, fullDir);
imageFileId = sqlQuickNum(conn, dy->string);
if (imageFileId == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into imageFile set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " fileName = '%s',\n", fileName);
    dyStringPrintf(dy, " priority = %s,\n", priority);
    dyStringPrintf(dy, " fullLocation = %d,\n", fullDir);
    dyStringPrintf(dy, " screenLocation = %d,\n", screenDir);
    dyStringPrintf(dy, " thumbLocation = %d,\n", thumbDir);
    dyStringPrintf(dy, " submissionSet = %d,\n", submissionSetId);
    dyStringPrintf(dy, " submitId = '%s'\n", submitId);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    imageFileId = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return imageFileId;
}

void doImageProbe(struct sqlConnection *conn,
	int imageId, int probeId, int probeColor, boolean replace)
/* Update image probe table if need be */
{
struct dyString *dy = dyStringNew(0);
boolean needUpdate = TRUE;
if (replace)
    {
    dyStringAppend(dy, "select count(*) from imageProbe where ");
    dyStringPrintf(dy, "image=%d and probe=%d and probeColor=%d",
    	imageId, probeId, probeColor);
    if (sqlQuickNum(conn, dy->string) != 0)
	needUpdate = FALSE;
    else
        dyStringClear(dy);
    }
if (needUpdate)
    {
    dyStringAppend(dy, "insert into imageProbe set");
    dyStringPrintf(dy, " image=%d,", imageId);
    dyStringPrintf(dy, " probe=%d,", probeId);
    dyStringPrintf(dy, " probeColor=%d\n", probeColor);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    }

dyStringFree(&dy);
}

void genePixLoad(char *setRaFile, char *itemTabFile)
/* genePixLoad - Load data into genePix database. */
{
struct hash *raHash = raReadSingle(setRaFile);
struct hash *rowHash;
struct lineFile *lf = lineFileOpen(itemTabFile, TRUE);
char *line, *words[256];
struct sqlConnection *conn = sqlConnect(database);
int rowSize;
int submissionSetId;
struct hash *fullDirHash = newHash(0);
struct hash *screenDirHash = newHash(0);
struct hash *thumbDirHash = newHash(0);
struct hash *treatmentHash = newHash(0);
struct hash *bodyPartHash = newHash(0);
struct hash *sliceTypeHash = newHash(0);
struct hash *probeColorHash = newHash(0);
struct hash *sectionSetHash = newHash(0);
struct dyString *dy = dyStringNew(0);

/* Read first line of tab file, and from it get all the field names. */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s appears to be empty", lf->fileName);
if (line[0] != '#')
    errAbort("First line of %s needs to start with #, and then contain field names",
    	lf->fileName);
rowHash = hashRowOffsets(line+1);
rowSize = rowHash->elCount;
if (rowSize >= ArraySize(words))
    errAbort("Too many fields in %s", lf->fileName);

/* Check that have all required fields */
    {
    char *fieldName;
    int i;

    for (i=0; i<ArraySize(requiredSetFields); ++i)
        {
	fieldName = requiredSetFields[i];
	if (!hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, setRaFile);
	}

    for (i=0; i<ArraySize(requiredItemFields); ++i)
        {
	fieldName = requiredItemFields[i];
	if (!hashLookup(rowHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, itemTabFile);
	}

    for (i=0; i<ArraySize(requiredFields); ++i)
        {
	fieldName = requiredFields[i];
	if (!hashLookup(rowHash, fieldName) && !hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s or %s", fieldName, setRaFile, itemTabFile);
	}
    }

/* Create/find submission record. */
submissionSetId = saveSubmissionSet(conn, raHash);

/* Process rest of tab file. */
while (lineFileNextRowTab(lf, words, rowSize))
    {
    /* Find/add fields that are in simple id/name type tables. */
    int fullDir = cachedId(conn, "fileLocation", "name", 
    	fullDirHash, "fullDir", raHash, rowHash, words);
    int screenDir = cachedId(conn, "fileLocation", "name", 
    	screenDirHash, "screenDir", raHash, rowHash, words);
    int thumbDir = cachedId(conn, "fileLocation", 
    	"name", thumbDirHash, "thumbDir", raHash, rowHash, words);
    int bodyPart = cachedId(conn, "bodyPart", 
    	"name", bodyPartHash, "bodyPart", raHash, rowHash, words);
    int sliceType = cachedId(conn, "sliceType", 
    	"name", sliceTypeHash, "sliceType", raHash, rowHash, words);
    int treatment = cachedId(conn, "treatment", 
    	"conditions", treatmentHash, "treatment", raHash, rowHash, words);
    int probeColor = cachedId(conn, "probeColor", 
        "name", probeColorHash, "probeColor", raHash, rowHash, words);
    
    /* Get other fields from input (either tab or ra file) */
    char *fileName = getVal("fileName", raHash, rowHash, words, NULL);
    char *taxon = getVal("taxon", raHash, rowHash, words, NULL);
    char *isEmbryo = getVal("isEmbryo", raHash, rowHash, words, NULL);
    char *age = getVal("age", raHash, rowHash, words, NULL);
    char *submitId = getVal("submitId", raHash, rowHash, words, "");
    char *sectionSet = getVal("sectionSet", raHash, rowHash, words, "");
    char *sectionIx = getVal("sectionIx", raHash, rowHash, words, "0");
    char *gene = getVal("gene", raHash, rowHash, words, "");
    char *locusLink = getVal("locusLink", raHash, rowHash, words, "");
    char *refSeq = getVal("refSeq", raHash, rowHash, words, "");
    char *uniProt = getVal("uniProt", raHash, rowHash, words, "");
    char *genbank = getVal("genbank", raHash, rowHash, words, "");
    char *priority = getVal("priority", raHash, rowHash, words, "200");
    char *journal = getVal("journal", raHash, rowHash, words, "");
    char *journalUrl = getVal("journalUrl", raHash, rowHash, words, "");
    char *fPrimer = getVal("fPrimer", raHash, rowHash, words, "");
    char *rPrimer = getVal("rPrimer", raHash, rowHash, words, "");
    char *seq = getVal("seq", raHash, rowHash, words, "");
    char *abName = getVal("abName", raHash, rowHash, words, "");
    char *abDescription = getVal("abDescription", raHash, rowHash, words, "");
    char *abTaxon = getVal("abTaxon", raHash, rowHash, words, "0");
    char *imagePos = getVal("imagePos", raHash, rowHash, words, "0");

    /* Some ID's we have to calculate individually. */
    int sectionId = 0;
    int geneId = 0;
    int antibodyId = 0;
    int probeId = 0;
    int imageFileId = 0;
    int imageId = 0;

    sectionId = doSectionSet(conn, sectionSetHash, sectionSet);
    geneId = doGene(lf, conn, gene, locusLink, refSeq, uniProt, genbank, taxon);
    antibodyId = doAntibody(conn, abName, abDescription, abTaxon);
    probeId = doProbe(lf, conn, geneId, antibodyId, fPrimer, rPrimer, seq);
    imageFileId = doImageFile(lf, conn, fileName, fullDir, screenDir, thumbDir,
    	submissionSetId, submitId, priority);

    /* Get existing image ID.  If it exists and we are not in replace mode
     * then die. */
    dyStringClear(dy);
    dyStringAppend(dy, "select id from image where ");
    dyStringPrintf(dy, "imageFile = %d and imagePos = %s", 
    	imageFileId, imagePos);
    imageId = sqlQuickNum(conn, dy->string);
    if (imageId != 0 && !(replace || multicolor))
        errAbort("Image %s in file %s already in database, aborting",
		imagePos, fileName);
    
    /* insert/update image. */
    dyStringClear(dy);
    if (imageId == 0)
	{
        dyStringAppend(dy, "insert into image set\n");
	dyStringAppend(dy, " id = default,\n");
	}
    else
        {
	dyStringAppend(dy, "update image set\n");
	}
    dyStringPrintf(dy, " imageFile = %d,\n", imageFileId);
    dyStringPrintf(dy, " imagePos = %s,\n", imagePos);
    dyStringPrintf(dy, " sectionSet = %d,\n", sectionId);
    dyStringPrintf(dy, " sectionIx = %s,\n", sectionIx);
    dyStringPrintf(dy, " taxon = %s,\n", taxon);
    dyStringPrintf(dy, " isEmbryo = %s,\n", isEmbryo);
    dyStringPrintf(dy, " age = %s,\n", age);
    dyStringPrintf(dy, " bodyPart = %d,\n", bodyPart);
    dyStringPrintf(dy, " sliceType = %d,\n", sliceType);
    dyStringPrintf(dy, " treatment = %d\n", treatment);
    if (imageId != 0)
        dyStringPrintf(dy, "where id = %d", imageId);
    uglyf("%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    if (imageId == 0)
	imageId = sqlLastAutoId(conn);

    doImageProbe(conn, imageId, probeId, probeColor, replace);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
database = optionVal("database", database);
replace = optionExists("replace");
multicolor = optionExists("multicolor");
genePixLoad(argv[1], argv[2]);
return 0;
}
