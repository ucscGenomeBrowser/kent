/* hgLoadEranModules - Load regulatory modules from Eran Segal. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadEranModules.c,v 1.2 2004/01/24 02:22:21 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadEranModules - Load regulatory modules from Eran Segal\n"
  "usage:\n"
  "   hgLoadEranModules database module_assignments.tab motif_attributes.tab motif_modules.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hashEl *hashLookup2(struct hash *hash, char *key1, char *key2)
/* Return value associated with key1&key2 */
{
char buf[64];
safef(buf, sizeof(buf), "%s&%s", key1, key2);
return hashLookup(hash, buf);
}

void hashAdd2(struct hash *hash, char *key1, char *key2, void *val)
/* Add value associated with key1&key2 to hash */
{
char buf[64];
safef(buf, sizeof(buf), "%s&%s", key1, key2);
hashAdd(hash, buf, val);
}

struct hash *hashTwoColumnFile(char *fileName, int hashSize)
/* Return hash from two-column file keyed by first column with values
 * of second column. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(hashSize);
char *row[2];
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], cloneString(row[1]));
lineFileClose(&lf);
return hash;
}


struct hash *loadGeneToModule(struct sqlConnection *conn, char *fileName, char *table)
/* Load up simple two-column file into a lookup type table. */
{
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy,
"CREATE TABLE  %s (\n"
"    gene varchar(255) not null,\n"
"    module int not null,\n"
"              #Indices\n"
"    PRIMARY KEY(gene(12)),\n"
"    INDEX(module)\n"
")\n", table);
sqlRemakeTable(conn, table, dy->string);
sqlLoadTabFile(conn, fileName, table, 0);
printf("Loaded %s table\n", table);
return hashTwoColumnFile(fileName, 0);
}

void loadGeneToMotif(struct sqlConnection *conn, char *fileName, char *table,
	struct hash *geneToModuleHash, struct hash *moduleAndMotifHash)
/* Load file which is a big matrix with genes for rows and motifs for
 * columns.  There is a 1 in the matrix where a gene has the motif.
 * The first column and the first row are labels. 
 *
 * Only load bits of this where motif actually occurs in module associated with
 * gene. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *tmpDir = ".";
FILE *f = hgCreateTabFile(tmpDir, table);
char *motifNames[32*1024], *row[32*1024];
int motifCount, rowSize, i;
char *gene, *module;
int geneCount = 0, total = 0;
struct dyString *dy = dyStringNew(512);

/* Read first line, which is labels. */
if (!lineFileNextReal(lf, &line))
    errAbort("Empty file %s", fileName);
subChar(line, ' ', '_');
motifCount = chopLine(line, motifNames);
if (motifCount >= ArraySize(motifNames))
    errAbort("Too many motifs line 1 of %s", fileName);
lineFileExpectAtLeast(lf, 2, motifCount);
for (i=0; i<motifCount; ++i)
    motifNames[i] = cloneString(motifNames[i]);

/* Read subsequent lines. */
while ((rowSize = lineFileChop(lf, row)) != 0)
    {
    lineFileExpectWords(lf, motifCount, rowSize);
    gene = row[0];
    module = hashFindVal(geneToModuleHash, gene);
    if (module == NULL)
        errAbort("Gene %s in line %d of %s but not geneToModuleHash\n", gene, 
		lf->lineIx, lf->fileName);
    for (i=1; i<rowSize; ++i)
        {
	if (row[i][0] == '1')
	    {
	    if (hashLookup2(moduleAndMotifHash, module, motifNames[i]))
		{
		fprintf(f, "%s\t%s\n", gene, motifNames[i]);
		++total;
		}
	    }
	}
    ++geneCount;
    }
dyStringPrintf(dy,
"CREATE TABLE  %s (\n"
"    gene varchar(255) not null,\n"
"    motif varchar(255) not null,\n"
"              #Indices\n"
"    INDEX(gene(12)),\n"
"    INDEX(motif(16))\n"
")\n",  table);
sqlRemakeTable(conn, table, dy->string);
printf("%d genes, %d motifs, %d motifs in genes\n",
	geneCount, motifCount-1, total);
hgLoadTabFile(conn, tmpDir, table, &f);
hgRemoveTabFile(tmpDir, table);
printf("Loaded %s table\n", table);
lineFileClose(&lf);
}


struct hash *loadModuleToMotif(struct sqlConnection *conn, char *fileName, 
	char *table)
/* Load up file which has a line per module.  The first word is the module
 * number, the rest of the tab-separated fields are motif names. 
 * Return hash keyed by module&motif. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *module, *motif;
char *tmpDir = ".";
FILE *f = hgCreateTabFile(tmpDir, table);
struct dyString *dy = dyStringNew(512);
int motifCount = 0, moduleCount = 0;
struct hash *hash = newHash(18);

while (lineFileNextReal(lf, &line))
    {
    ++moduleCount;
    subChar(line, ' ', '_');
    module = nextWord(&line);
    while ((motif = nextWord(&line)) != NULL)
	{
	++motifCount;
        fprintf(f, "%s\t%s\n", module, motif);
	hashAdd2(hash, module, motif, NULL);
	}
    }
dyStringPrintf(dy,
"CREATE TABLE  %s (\n"
"    module int not null,\n"
"    motif varchar(255) not null,\n"
"              #Indices\n"
"    INDEX(module),\n"
"    INDEX(motif(16))\n"
")\n",  table);
sqlRemakeTable(conn, table, dy->string);
printf("%d modules, %d motifs in modules\n",
	moduleCount, motifCount);
hgLoadTabFile(conn, tmpDir, table, &f);
hgRemoveTabFile(tmpDir, table);
printf("Loaded %s table\n", table);
lineFileClose(&lf);
return hash;
}

void crossCheck(struct sqlConnection *conn,
	char *geneToModule, char *geneToMotif, char *moduleToMotif)
/* Do sanity check after loading.  */
{
struct hash *geneToModuleHash = newHash(16);
struct hash *moduleToGeneHash = newHash(16);
struct hash *moduleToMotifHash = newHash(16);
struct hash *motifToModuleHash = newHash(16);
struct hash *modMotHash = newHash(18);
char query[512], modMot[64];
struct sqlResult *sr;
char **row;
int reusedMotifCount = 0;
int fatalErrorCount = 0;


/* Load up moduleToMotif table and note how many times
 * a motif is used more than once just for curiousity
 * (this is not an error condition). */
safef(query, sizeof(query), "select module,motif from %s", moduleToMotif);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *module = cloneString(row[0]);
    char *motif = cloneString(row[1]);
    hashAdd(moduleToMotifHash, module, motif);
    if (hashLookup(motifToModuleHash, motif))
        ++reusedMotifCount;
    hashAdd(motifToModuleHash, motif, module);
    safef(modMot, sizeof(modMot), "%s %s", module, motif);
    hashAdd(modMotHash, modMot, NULL);
    }
sqlFreeResult(&sr);
printf("%d motifs reuses in modules\n", reusedMotifCount);

printf("Cross-checking tables\n");

/* Load up geneToModule table, and make sure that all modules actually
 * exist in moduleToMotif table. */
safef(query, sizeof(query), "select gene,module from %s", geneToModule);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = cloneString(row[0]);
    char *module = cloneString(row[1]);
    if (!hashLookup(moduleToMotifHash, module))
        {
	warn("Module %s is in %s but not %s", 
		module, geneToModule, moduleToMotif);
	++fatalErrorCount;
	}
    hashAdd(geneToModuleHash, gene, module);
    hashAdd(moduleToGeneHash, module, gene);
    }
sqlFreeResult(&sr);

/* Scan again through moduleToMotif table and make sure that
 * all modules are present in geneToModule. */
safef(query, sizeof(query), "select module from %s", moduleToMotif);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *module = row[0];
    if (!hashLookup(moduleToGeneHash, module))
        {
	warn("Module %s is in %s but not %s",
	    module, moduleToMotif, geneToModule);
	++fatalErrorCount;
	}
    }
sqlFreeResult(&sr);

/* Scan through geneToMotif table checking things. */
safef(query, sizeof(query), "select gene,motif from %s", geneToMotif);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = row[0];
    char *motif = row[1];
    char *module = hashFindVal(geneToModuleHash, gene);
    if (module == NULL)
        {
	warn("Gene %s is in %s but not %s", geneToMotif, geneToModule);
	++fatalErrorCount;
	}
    safef(modMot, sizeof(modMot), "%s %s", module, motif);
    if (hashLookup(modMotHash, modMot) == NULL)
        {
	warn("Gene %s has motif %s, but that motif isn't in %s\n",
	    gene, motif, module);
	++fatalErrorCount;
	}
    }
sqlFreeResult(&sr);

printf("%d errors\n", fatalErrorCount);
}

void hgLoadEranModules(char *database, char *geneToModule, 
	char *geneToMotif, char *moduleToMotif)
/* hgLoadEranModules - Load regulatory modules from Eran Segal. */
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *moduleAndMotifHash = loadModuleToMotif(conn, moduleToMotif, "esRegModuleToMotif");
struct hash *geneToModuleHash = loadGeneToModule(conn, geneToModule, "esRegGeneToModule");
loadGeneToMotif(conn, geneToMotif, "esRegGeneToMotif", geneToModuleHash, moduleAndMotifHash);
crossCheck(conn, "esRegGeneToModule", "esRegGeneToMotif", "esRegModuleToMotif");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hgLoadEranModules(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
