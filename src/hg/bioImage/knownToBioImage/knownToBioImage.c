/* knownToBioImage - Create knownToBioImage table by riffling through various other knownTo tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hgRelate.h"
#include "bioImage.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "knownToBioImage - Create knownToBioImage table by riffling through various other knownTo tables\n"
  "usage:\n"
  "   knownToBioImage database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct prioritizedImage 
    {
    int imageId;	/* ID of image */
    float priority;	/* Image priority - smaller is more urgent */
    };

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
sqlDyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    PRIMARY KEY(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void addPrioritizedImage(struct hash *hash, int id, float priority, char *key)
/* Add image to hash, replacing what's already there if we have a better priority */
{
struct prioritizedImage *pi = hashFindVal(hash, key);
if (pi == NULL)
    {
    AllocVar(pi);
    pi->imageId = id;
    pi->priority = priority;
    hashAdd(hash, key, pi);
    }
else if (pi->priority > priority)
    {
    pi->imageId = id;
    pi->priority = priority;
    }
}

void foldIntoHash(struct sqlConnection *conn, char *table, char *keyField, char *valField,
	struct hash *hash)
/* Add key/value pairs from table into hash */
{
struct sqlResult *sr;
char query[256];
char **row;
sqlSafef(query, sizeof(query), "select %s,%s from %s", keyField, valField, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
}

int bestImage(char *kgId, struct hash *kgToHash, struct hash *imageHash)
/* Return best image id if possible, otherwise 0 */
{
struct hashEl *extId = hashLookup(kgToHash, kgId);

while (extId != NULL)
    {
    struct prioritizedImage *pi = hashFindVal(imageHash, extId->val);
    if (pi != NULL)
	return pi->imageId;
    extId = hashLookupNext(extId);
    }
return 0;
}

void knownToBioImage(char *database)
/* knownToBioImage - Create knownToBioImage table by riffling through various other knownTo tables. */
{
char *tempDir = ".";
char *outTable = "knownToBioImage";
FILE *f = hgCreateTabFile(tempDir, outTable);
struct sqlConnection *hConn = sqlConnect(database);
struct sqlConnection *iConn = sqlConnect("bioImage");
struct sqlResult *sr;
char **row;
struct hash *geneImageHash = newHash(18);
struct hash *locusLinkImageHash = newHash(18);
struct hash *refSeqImageHash = newHash(18);
struct hash *genbankImageHash = newHash(18);
struct hash *knownToLocusLinkHash = newHash(18);
struct hash *knownToRefSeqHash = newHash(18);
struct hash *knownToGeneHash = newHash(18);
struct slName *knownList = NULL, *known;
struct hash *dupeHash = newHash(17);

/* Go through and make up hashes of images keyed by various fields. */
sr = sqlGetResult(iConn, 
	"NOSQLINJ select id,priority,gene,locusLink,refSeq,genbank from image");
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]);
    float priority = atof(row[1]);
    addPrioritizedImage(geneImageHash, id, priority, row[2]);
    addPrioritizedImage(locusLinkImageHash, id, priority, row[3]);
    addPrioritizedImage(refSeqImageHash, id, priority, row[4]);
    addPrioritizedImage(genbankImageHash, id, priority, row[5]);
    }
uglyf("Made hashes of image: geneImageHash %d, locusLinkImageHash %d, refSeqImageHash %d, genbankImageHash %d\n", geneImageHash->elCount, locusLinkImageHash->elCount, refSeqImageHash->elCount, genbankImageHash->elCount);
sqlFreeResult(&sr);

/* Build up list of known genes. */
sr = sqlGetResult(hConn, "NOSQLINJ select name from knownGene");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (!hashLookup(dupeHash, name))
        {
	hashAdd(dupeHash, name, NULL);
	known = slNameNew(name);
	slAddHead(&knownList, known);
	}
    }
slReverse(&knownList);
sqlFreeResult(&sr);

/* Build up hashes from knownGene to other things. */
foldIntoHash(hConn, "knownToLocusLink", "name", "value", knownToLocusLinkHash);
foldIntoHash(hConn, "knownToRefSeq", "name", "value", knownToRefSeqHash);
foldIntoHash(hConn, "kgAlias", "kgID", "alias", knownToGeneHash);
foldIntoHash(hConn, "kgProtAlias", "kgID", "alias", knownToGeneHash);
uglyf("knownToLocusLink %d, knownToRefSeq %d, knownToGene %d\n", knownToLocusLinkHash->elCount, knownToRefSeqHash->elCount, knownToGeneHash->elCount);

/* Try and find an image for each gene. */
for (known = knownList; known != NULL; known = known->next)
    {
    char *name = known->name;
    int imageId = 0;

    imageId = bestImage(name, knownToLocusLinkHash, locusLinkImageHash);
    if (imageId == 0)
        imageId = bestImage(name, knownToRefSeqHash, refSeqImageHash);
    if (imageId == 0)
	{
	struct prioritizedImage *pi = hashFindVal(genbankImageHash, name);
	if (pi != NULL)
	    imageId = pi->imageId;
	}
    if (imageId == 0)
        imageId = bestImage(name, knownToGeneHash, geneImageHash);
    if (imageId != 0)
        {
	fprintf(f, "%s\t%d\n", name, imageId);
	}
    }

createTable(hConn, outTable);
hgLoadTabFile(hConn, tempDir, outTable, &f);
hgRemoveTabFile(tempDir, outTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
knownToBioImage(argv[1]);
return 0;
}
