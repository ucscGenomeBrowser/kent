/* vgGetText - Get text for full text indexing for VisiGene.
 * This is just the kgRef.description for known genes associated
 * with the visiGene genes. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: vgGetText.c,v 1.14 2008/08/27 23:00:40 kent Exp $";

char *db = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgGetText - Get text for full text indexing for VisiGene\n"
  "usage:\n"
  "   vgGetText out.text genomeDb [genomeDb2 ...]\n"
  "Where the genomeDb is typically the latest human, the genomeDb2 is the latest mouse, and\n"
  "as more relevant organisms get added perhaps add genomeDb3 and so forth.\n"
  "options:\n"
  "   -db=xxx - Use another database (default visiGene)\n"
  );
}

static struct optionSpec options[] = {
   {"db", OPTION_STRING},
   {NULL, 0},
};


/* More complex hashes - keyed by id as ascii number, with
 * values of structures in visiGene.h  */
struct hash *imageProbeHash;
struct hash *probeHash;
struct hash *geneHash;

/* Hash containing known gene text.  Keyed by knownGene id. */
struct hash *knownTextHash;

struct hash *nameToKnown;	/* Keyed by gene symbol, value knownGene id. */
struct hash *refSeqToKnown;
struct hash *uniProtToKnown;
struct hash *aliasToKnown;


struct hash *hashSizedForTable(struct sqlConnection *conn, char *table)
/* Return a hash sized appropriately to hold all of table. */
{
char query[256];
int tableSize;
safef(query, sizeof(query), "select count(*) from %s", table);
tableSize = sqlQuickNum(conn, query);
printf("%s has %d rows\n", table, tableSize);
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

void hashComplexTables(struct sqlConnection *conn)
/* Make hashes for more complicated tables. */
{
struct sqlResult *sr;
char **row;

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

imageProbeHash = hashSizedForTable(conn, "imageProbe");
sr = sqlGetResult(conn, "select * from imageProbe");
while ((row = sqlNextRow(sr)) != NULL)
     hashAdd(imageProbeHash, row[1], imageProbeLoad(row)); /* Key on image, not id. */
sqlFreeResult(&sr);
}

void makeKnownGeneHashes(int knownDbCount, char **knownDbs)
/* Create hashes containing info on known genes. */
{
int i;
knownTextHash = hashNew(18);
uniProtToKnown = hashNew(18);
refSeqToKnown = hashNew(18);
aliasToKnown = hashNew(19);
nameToKnown = hashNew(18);

for (i=0; i<knownDbCount; i += 1)
    {
    char *gdb = knownDbs[i];
    struct sqlConnection *conn = sqlConnect(gdb);
    struct sqlResult *sr;
    char **row;


    sr = sqlGetResult(conn, "select kgID,geneSymbol,spID,spDisplayID,refseq,description from kgXref");
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *kgID = cloneString(row[0]);
	touppers(kgID);
	touppers(row[1]);
	hashAdd(nameToKnown, row[1], kgID);
	hashAdd(uniProtToKnown, row[2], kgID);
	hashAdd(uniProtToKnown, row[3], kgID);
	hashAdd(refSeqToKnown, row[4], kgID);
	hashAdd(knownTextHash, kgID, cloneString(row[5]));
	}
    sqlFreeResult(&sr);

    sr = sqlGetResult(conn, "select kgID,alias from kgAlias");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *upc = cloneString(row[0]);
	touppers(upc);
        hashAdd(aliasToKnown, row[1], upc);
	}
    sqlFreeResult(&sr);

    sr = sqlGetResult(conn, "select kgID,alias from kgProtAlias");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *upc = cloneString(row[0]);
	touppers(upc);
        hashAdd(aliasToKnown, row[1], upc);
	}
    sqlFreeResult(&sr);
    }
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

void uniqPrintGene(char *kgID, struct hash *uniqHash, FILE *f)
/* If kgID not in uniqHash, then print out info associated with
 * it and add it to uniqHash */
{
if (!hashLookup(uniqHash, kgID))
    {
    char *text = hashMustFindVal(knownTextHash, kgID);
    fprintf(f, "%s ", text);
    hashAdd(uniqHash, kgID, NULL);
    }
}

boolean  printGeneFromHashOrAlias(char *sym, struct hash *hash, struct hash *aliasHash,
	struct hash *uniqHash, FILE *f)
/* Look up symbol in hash, and if not found there in aliasHash.
 * Then print out associated info uniquely */
{
char *kgID = NULL;
if (sym != NULL && sym[0] != 0)
    {
    kgID = hashFindVal(hash, sym);
    if (kgID == NULL)
	kgID = hashFindVal(aliasHash, sym);
    if (kgID != NULL)
	uniqPrintGene(kgID, uniqHash, f);
    }
return kgID != NULL;
}

void printGeneText(struct gene *gene, FILE *f)
/* Print extended text associated with gene. */
{
struct hash *uniqHash = hashNew(8);
boolean gotSomething = FALSE;

gotSomething |= printGeneFromHashOrAlias(gene->refSeq, refSeqToKnown, aliasToKnown, uniqHash, f);
gotSomething |= printGeneFromHashOrAlias(gene->uniProt, uniProtToKnown, aliasToKnown, uniqHash, f);
gotSomething |= printGeneFromHashOrAlias(gene->genbank, aliasToKnown, aliasToKnown, uniqHash, f);

if (gene->name[0] != 0)
    {
    char *upcName = cloneString(gene->name);
    struct hashEl *hel;
    touppers(upcName);
    for (hel = hashLookup(nameToKnown, upcName); hel != NULL; 
    	hel = hashLookupNext(hel))
	{
	char *kgID = hel->val;
	gotSomething = TRUE;
	uniqPrintGene(kgID, uniqHash, f);
	}
    if (!gotSomething)
        {
	printGeneFromHashOrAlias(upcName, aliasToKnown, aliasToKnown, uniqHash, f);
	}
    freeMem(upcName);
    }
hashFree(&uniqHash);
}

void imageText(struct sqlConnection *conn, char *image, FILE *f)
/* Write out text associated with image to file */
{
struct imageProbe *ipList, *ip;
fprintf(f, "%s", image);

/* Get list of probes associated with image. */
ipList = ipForImage(conn, image);

/* Write out gene info. */
fprintf(f, "\t");
for (ip = ipList; ip != NULL; ip = ip->next)
    {
    struct probe *probe = hashMustFindValFromInt(probeHash, ip->probe);
    struct gene *gene = hashFindValFromInt(geneHash, probe->gene);
    if (gene != NULL)
	printGeneText(gene, f);
    }

fprintf(f, "\n");
imageProbeFreeList(&ipList);
}

void vgGetText(char *outText, int knownDbCount, char **knownDbs)
/* vgGetText - Get text for full text indexing for VisiGene. */
{
FILE *f = mustOpen(outText, "w");
struct sqlConnection *conn = sqlConnect(db);
struct sqlConnection *imageConn = sqlConnect(db);
struct sqlResult *sr;
char **row;

verbose(2, "hashComplexTables\n");
hashComplexTables(conn);
verbose(2, "makeKnownGeneHashes\n");
makeKnownGeneHashes(knownDbCount, knownDbs);
sr = sqlGetResult(imageConn, 
    "select id from image");
while ((row = sqlNextRow(sr)) != NULL)
    {
    verbose(3, "imageText on %s\n", row[0]);
    imageText(conn, row[0], f);
    }
sqlFreeResult(&sr);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
db = optionVal("db", db);
if (argc < 3)
    usage();
vgGetText(argv[1], (argc-2), argv+2);
return 0;
}
