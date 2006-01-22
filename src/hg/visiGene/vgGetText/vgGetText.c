/* vgGetText - Get text for full text indexing for VisiGene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: vgGetText.c,v 1.2 2006/01/22 20:20:47 kent Exp $";

char *db = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgGetText - Get text for full text indexing for VisiGene\n"
  "usage:\n"
  "   vgGetText out.text\n"
  "options:\n"
  "   -db=xxx - Use another database (default visiGene)\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


/* Various hashes of simple two-column database tables. */
struct hash *bodyPart;
struct hash *cellType;
struct hash *cellSubtype;
struct hash *sliceType;
struct hash *fixation;
struct hash *embedding;
struct hash *permeablization;
struct hash *contributor;
struct hash *copyright;
struct hash *bac;
struct hash *sex;
struct hash *probeType;
struct hash *caption;
struct hash *probeColor;
struct hash *expressionPattern;

/* More simple hashes, but built from complex queries. */
struct hash *commonNameHash;
struct hash *binomialHash;
struct hash *strainHash;

/* More complex hashes - keyed by id as ascii number, with
 * values of structures in visiGene.h  */
struct hash *imageProbeHash;
struct hash *probeHash;
struct hash *geneHash;
struct hash *genotypeHash;
struct hash *submissionSetHash;
struct hash *antibodyHash;
struct hash *specimenHash;
struct hash *journalHash;
struct hash *imageFileHash;
struct hash *stageSchemeHash;
struct hash *expressionLevelHash;

struct stageScheme
/* Slightly cooked version of lifeStageScheme. */
    {
    struct stageScheme *next;
    char *name;		/* Scheme name. */
    struct lifeStage *stageList;
    };

struct hash *hashSizedForTable(struct sqlConnection *conn, char *table)
/* Return a hash sized appropriately to hold all of table. */
{
char query[256];
int tableSize;
safef(query, sizeof(query), "select count(*) from %s", table);
tableSize = sqlQuickNum(conn, query);
verbose(1, "%s has %d rows\n", table, tableSize);
return newHash(digitsBaseTwo(tableSize) + 1);
}

void *hashFindValFromInt(struct hash *hash, int key)
/* Return hash value using an integer key */
{
char buf[32];
safef(buf, sizeof(buf), "%d", key);
return hashFindVal(hash, buf);
}

void *hashMustFindValFromInt(struct hash *hash, int key)
/* Return hash value using an integer key */
{
char buf[32];
safef(buf, sizeof(buf), "%d", key);
return hashMustFindVal(hash, buf);
}

void fillInTwoColHash(struct sqlConnection *conn, char *query, struct hash *hash)
/* Fill in a hash from a two-column query.  First column is key,
 * second value. */
{
char **row;
struct sqlResult *sr;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], lmCloneString(hash->lm, row[1]));
sqlFreeResult(&sr);
}

struct hash *twoColHash(struct sqlConnection *conn, char *table)
/* Create a hash keyed by first column of table with second column
 * as values. */
/* Create a hash keyed by keyField, and with valField values */
{
struct hash *hash = hashSizedForTable(conn, table);
char query[256];
safef(query, sizeof(query), "select * from %s", table);
fillInTwoColHash(conn, query, hash);
return hash;
}

struct hash *makeStageSchemeHash(struct sqlConnection *conn)
/* Create hash of stageScheme's keyed by taxon. */
{
struct hash *taxonHash = hashNew(8);
struct hash *idHash = hashNew(8);
char asciiNum[16];
struct sqlResult *sr;
char **row, query[256];
struct stageScheme *schemeList = NULL, *scheme;
struct lifeStage *stage;

sr = sqlGetResult(conn, "select id,taxon,name from lifeStageScheme");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(scheme);
    slAddHead(&schemeList, scheme);
    scheme->name = cloneString(row[2]);
    hashAdd(taxonHash, row[1], scheme);
    hashAdd(idHash, row[0], scheme);
    }
sqlFreeResult(&sr);

sr = sqlGetResult(conn, "select * from lifeStage order by age");
while ((row = sqlNextRow(sr)) != NULL)
    {
    stage = lifeStageLoad(row);
    scheme = hashMustFindValFromInt(idHash, stage->lifeStageScheme);
    slAddHead(&scheme->stageList, stage);
    }
sqlFreeResult(&sr);

for (scheme = schemeList; scheme != NULL; scheme = scheme->next)
    {
    slReverse(&scheme->stageList);
    }
hashFree(&idHash);
return taxonHash;
}

void printStage(FILE *f, int taxon, float age)
/* Print out developmental stage if possible. */
{
struct stageScheme *scheme = hashFindValFromInt(stageSchemeHash, taxon);
if (scheme != NULL)
    {
    struct lifeStage *stage, *matchedStage = NULL;
    for (stage = scheme->stageList; stage != NULL; stage = stage->next)
        {
	if (stage->age <= age)
	    matchedStage = stage;
	else
	    break;
	}
    if (matchedStage != NULL)
        {
	fprintf(f, "%s %s\t%s", scheme->name, matchedStage->name, matchedStage->description);
	}
    }
}

void hashSimpleTables(struct sqlConnection *conn)
/* Create hashes for all simple two column tables. */
{
bodyPart = twoColHash(conn, "bodyPart");
cellType = twoColHash(conn, "cellType");
cellSubtype = twoColHash(conn, "cellSubtype");
sliceType = twoColHash(conn, "sliceType");
fixation = twoColHash(conn, "fixation");
embedding = twoColHash(conn, "embedding");
permeablization = twoColHash(conn, "permeablization");
contributor = twoColHash(conn, "contributor");
copyright = twoColHash(conn, "copyright");
bac = twoColHash(conn, "bac");
sex = twoColHash(conn, "sex");
probeType = twoColHash(conn, "probeType");
caption = twoColHash(conn, "caption");
probeColor = twoColHash(conn, "probeColor");
expressionPattern = twoColHash(conn, "expressionPattern");
commonNameHash = twoColHash(conn, "uniProt.commonName");
}

void hashComplexTables(struct sqlConnection *conn)
/* Make hashes for more complicated tables. */
{
struct sqlResult *sr;
char query[512], **row;
struct gene *gene;
struct genotype *genotype;
struct submissionSet *submissionSet;
struct antibody *antibody;
struct specimen *specimen;
struct journal *journal;
struct imageProbe *ip;

imageFileHash = hashSizedForTable(conn, "imageFile");
sr = sqlGetResult(conn, "select * from imageFile");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(imageFileHash, row[0], imageFileLoad(row));
sqlFreeResult(&sr);

probeHash = hashSizedForTable(conn, "probe");
sr = sqlGetResult(conn, "select * from probe");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(probeHash, row[0], probeLoad(row));
sqlFreeResult(&sr);

geneHash = hashSizedForTable(conn, "gene");
sr = sqlGetResult(conn, "select * from gene");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(geneHash, row[0], geneLoad(row));
sqlFreeResult(&sr);

genotypeHash = hashSizedForTable(conn, "genotype");
sr = sqlGetResult(conn, "select * from genotype");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(genotypeHash, row[0], genotypeLoad(row));
sqlFreeResult(&sr);

submissionSetHash = hashSizedForTable(conn, "submissionSet");
sr = sqlGetResult(conn, "select * from submissionSet");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(submissionSetHash, row[0], submissionSetLoad(row));
sqlFreeResult(&sr);

antibodyHash = hashSizedForTable(conn, "antibody");
sr = sqlGetResult(conn, "select * from antibody");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(antibodyHash, row[0], antibodyLoad(row));
sqlFreeResult(&sr);

specimenHash = hashSizedForTable(conn, "specimen");
sr = sqlGetResult(conn, "select * from specimen");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(specimenHash, row[0], specimenLoad(row));
sqlFreeResult(&sr);

journalHash = hashSizedForTable(conn, "journal");
sr = sqlGetResult(conn, "select * from journal");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(journalHash, row[0], journalLoad(row));
sqlFreeResult(&sr);

imageProbeHash = hashSizedForTable(conn, "imageProbe");
sr = sqlGetResult(conn, "select * from imageProbe");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(imageProbeHash, row[1], imageProbeLoad(row)); /* Key on image, not id. */
sqlFreeResult(&sr);

expressionLevelHash = hashSizedForTable(conn, "expressionLevel");
sr = sqlGetResult(conn, "select * from expressionLevel");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(expressionLevelHash, row[0], expressionLevelLoad(row));
sqlFreeResult(&sr);

binomialHash = hashSizedForTable(conn, "uniProt.taxon");
fillInTwoColHash(conn, "select id,binomial from uniProt.taxon", binomialHash);

strainHash = hashSizedForTable(conn, "strain");
fillInTwoColHash(conn, "select id,name from strain", strainHash);
}

struct imageProbe *ipForImage(struct sqlConnection *conn, char *image)
/* Get list of probes associated with image. */
{
struct hashEl *hel;
struct imageProbe *ipList = NULL, *ip;

for (hel = hashLookup(imageProbeHash, image); hel != NULL; 
	hel = hashLookupNext(hel))
    {
    ip = hel->val;
    slAddHead(&ipList, ip);
    }
slReverse(&ipList);
return ipList;
}

void printExpression(FILE *f, int imageProbe)
/* Print all positive expression associated with imageProbe. */
{
struct hashEl *hel;
char ipAscii[16];
safef(ipAscii, sizeof(ipAscii), "%d", imageProbe);
for (hel = hashLookup(expressionLevelHash, ipAscii); hel != NULL; 
	hel = hashLookupNext(hel))
    {
    struct expressionLevel *exp = hel->val;
    if (exp->level > 0)
        {
	char *s;
	s = hashFindValFromInt(bodyPart, exp->bodyPart);
	if (s != NULL)
	    fprintf(f, "%s ", s);
	s = hashFindValFromInt(cellType, exp->cellType);
	if (s != NULL)
	    fprintf(f, "%s ", s);
	s = hashFindValFromInt(cellSubtype, exp->cellSubtype);
	if (s != NULL)
	    fprintf(f, "%s ", s);
	}
    }
}

void imageText(struct sqlConnection *conn,
	char *image, char *submissionSet, char *imageFile,
	char *specimen, char *preparation, 
	FILE *f)
/* Write out text associated with image to file */
{
char query[512], **row;
struct sqlResult *sr;
struct imageProbe *ipList, *ip;
struct genotype *genotype = NULL;
struct submissionSet *set = hashMustFindVal(submissionSetHash, submissionSet);
struct specimen *spec = hashMustFindVal(specimenHash, specimen);
struct imageFile *file = hashMustFindVal(imageFileHash, imageFile);
struct journal *journal;
char *contributors;
char *s;
fprintf(f, "%s", image);

/* Get list of probes associated with image. */
ipList = ipForImage(conn, image);

if (spec->genotype != 0)
    genotype = hashMustFindValFromInt(genotypeHash, spec->genotype);

/* Write out all probe names in form we search for them. */
fprintf(f, "\t");
for (ip = ipList; ip != NULL; ip = ip->next)
    fprintf(f, "vgPrb_%d ", ip->probe);

/* Write out submit id. */
fprintf(f, "\t%s", file->submitId);

/* If it's a mutant, treat these genes as even more important
 * than the marker genes. */
fprintf(f, "\t");
if (genotype != NULL)
    fprintf(f, "%s", genotype->alleles);

/* Write out all gene symbols. */
fprintf(f, "\t");
for (ip = ipList; ip != NULL; ip = ip->next)
    {
    struct probe *probe = hashMustFindValFromInt(probeHash, ip->probe);
    struct gene *gene = hashFindValFromInt(geneHash, probe->gene);
    if (gene != NULL)
        fprintf(f, "%s ", gene->name);
    }

/* Write out gene accessions. */
fprintf(f, "\t");
for (ip = ipList; ip != NULL; ip = ip->next)
    {
    struct probe *probe = hashMustFindValFromInt(probeHash, ip->probe);
    struct gene *gene = hashFindValFromInt(geneHash, probe->gene);
    if (gene != NULL)
	{
	fprintf(f, "%s %s %s ", gene->refSeq, gene->genbank, gene->uniProt);
	}
    }

/* Write out authors */
contributors = cloneString(set->contributors);
fprintf(f, "\t");
fprintf(f, "%s ",  contributors);
stripChar(contributors, '.');
fprintf(f, "%s",  contributors);

/* Write out year of publication and title. */
fprintf(f, "\t%d", set->year);
fprintf(f, "\t%s", set->publication);

/* Write out journal. */
journal = hashFindValFromInt(journalHash, set->journal);
if (journal != NULL)
    fprintf(f, "\t%s", journal->name);

/* Write out stuff on specimen. */
fprintf(f, "\t%s", (char*)hashMustFindValFromInt(binomialHash, spec->taxon));
fprintf(f, "\t");
s = hashFindValFromInt(commonNameHash, spec->taxon);
if (s != NULL)
    fprintf(f, "%s", s);
fprintf(f, "\t%s", spec->name);
s = hashFindValFromInt(bodyPart, spec->bodyPart);
if (s != NULL)
    fprintf(f, "\t%s", s);
s = hashFindValFromInt(sex, spec->sex);
if (s != NULL)
    fprintf(f, "\t%s", s);
fprintf(f, "\t");
if (genotype != NULL)
    {
    char *strain = hashFindValFromInt(strainHash, genotype->strain);
    if (strain != NULL)
        fprintf(f, "%s", strain);
    }

/* Write out stage. */
fprintf(f, "\t");
printStage(f, spec->taxon, spec->age);

/* Write out tissues it's expressed in if any. */
fprintf(f, "\t");
for (ip = ipList; ip != NULL; ip = ip->next)
    printExpression(f, ip->id);

/* Write out caption. */
s = hashFindValFromInt(caption, file->caption);
fprintf(f, "\t");
if (s != NULL)
    {
    subChar(s, '\n', ' ');
    fprintf(f, "%s", s);
    }

fprintf(f, "\n");
imageProbeFreeList(&ipList);
}

void vgGetText(char *outText)
/* vgGetText - Get text for full text indexing for VisiGene. */
{
FILE *f = mustOpen(outText, "w");
struct sqlConnection *conn = sqlConnect(db);
struct sqlConnection *imageConn = sqlConnect(db);
struct sqlResult *sr;
char query[512], **row;

hashSimpleTables(conn);
hashComplexTables(conn);
stageSchemeHash = makeStageSchemeHash(conn);
uglyTime("loaded hashes");
sr = sqlGetResult(imageConn, 
    "select id,submissionSet,imageFile,specimen,preparation from image where id > 5950 limit 20000");
while ((row = sqlNextRow(sr)) != NULL)
    {
    imageText(conn, row[0], row[1], row[2], row[3], row[4], f);
    }
uglyTime("created saved text");
sqlFreeResult(&sr);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
db = optionVal("db", db);
if (argc != 2)
    usage();
uglyTime(NULL);
vgGetText(argv[1]);
return 0;
}
