/* hgLoadEranModules - Load regulatory modules from Eran Segal. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "obscure.h"
#include "hdb.h"
#include "hgRelate.h"
#include "binRange.h"
#include "esMotif.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"

static char const rcsid[] = "$Id: hgLoadEranModules.c,v 1.4 2008/09/03 19:19:50 markd Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadEranModules - Load regulatory modules from Eran Segal\n"
  "usage:\n"
  "   hgLoadEranModules database module_assignments.tab motif_modules.tab modules_motifs.gxm modules_motif_positions.tab gene_positions.tab\n"
  "where the parameters are:\n"
  "   database - a genome database such as sacCer1 or hg16\n"
  "   module_assignments.tab - a two column file: gene, module\n"
  "   motif_modules.tab - a tab separated file with one line for each module\n"
  "          The field of each line is the module, subsequent fields are\n"
  "          the motifs that make up that module\n"
  "   modules_motifs.gxm - A XML file with the motif weight matrices\n"
  "   modules_motif_positions.tab - A big table with genes for rows and \n"
  "          motifs for columns that says the position of each motif relative\n"
  "          to the start of gene\n"
  "   gene_positions.tab - A tab separated files with columns:\n"
  "       geneName,chromosome,txStart,txEnd,strand,upSize,downSize\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *tmpDir = ".";

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
verbose(1, "Loaded %s table\n", table);
return hashTwoColumnFile(fileName);
}

struct genomePos
/* A part of genome. */
    {
    struct genomePos *next;
    char *name;		/* Gene name, not allocated here. */
    char *chrom;	/* Chromosome, not allocated here. */
    int start,end;	/* Range, zero based, half open. */
    char strand;	/* + or - */
    int score;		/* Possibly score. */
    char *motif;	/* Possibly motif name. Not allocated here. */
    };

int genomePosCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start. */
{
const struct genomePos *a = *((struct genomePos **)va);
const struct genomePos *b = *((struct genomePos **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}


struct hash *loadGenePositions(char *database, struct sqlConnection *conn, char *fileName)
/* Read in 7 column file and convert to hash of gene
 * positions. */
{
struct hash *hash = newHash(16);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[7];
int count = 0;
struct genomePos *posList = NULL, *pos;

while (lineFileRow(lf, row))
    {
    int geneStart,geneEnd,upSize,downSize;

    AllocVar(pos);
    hashAddSaveName(hash, row[0], pos, &pos->name);
    slAddHead(&posList, pos);
    pos->chrom = hgOfficialChromName(database, row[1]);
    if (pos->chrom == NULL)
        errAbort("Unrecognized chromosome %s line %d of %s",
		row[1], lf->lineIx, lf->fileName);
    geneStart = lineFileNeedNum(lf, row, 2);
    geneEnd = lineFileNeedNum(lf, row, 3);
    pos->strand = row[4][0];
    if (pos->strand != '+' && pos->strand != '-')
        errAbort("Unrecognized strand %s line %d of %s",
		row[4], lf->lineIx, lf->fileName);
    upSize = lineFileNeedNum(lf, row, 5);
    downSize = lineFileNeedNum(lf, row, 6);
    if (pos->strand == '+')
        {
	pos->start = geneStart - upSize;
	pos->end = geneStart + downSize;
	}
    else	
        {
	pos->start = geneEnd - downSize;
	pos->end = geneEnd + upSize;
	}
    ++count;
    }
verbose(1, "%d genes in %s\n", count, fileName);
return hash;
}

struct genomePos *convertMotifPos(char *textPos, struct genomePos *upstreamPos, 
	struct dnaMotif *motif, struct lineFile *lf)
/* Take a semi-colon separated list of positions relative to given upstreamPos
 * and convert it a list of genomePos. */
{
int relStart, start,end;
char *s, *e;
char relStrand, strand;
struct genomePos *posList = NULL, *pos;

for (s = textPos; s != NULL; s = e)
    {
    e = strchr(s, ';');
    if (e != NULL)
        *e++ = 0;
    if (!isdigit(s[0]) && !(s[0] == '-' && isdigit(s[1])))
        errAbort("Expecting semicolon separated list of numbers line %d of %s",
	    lf->lineIx, lf->fileName);
    relStart = atoi(s);
    relStrand = '+';
    if (relStart < 0)
        {
	relStrand = '-';
	relStart = -relStart;
	}
    if (upstreamPos->strand == '+')
	{
	strand = relStrand;
	start = upstreamPos->start + relStart;
	end = start + motif->columnCount;
	}
    else
        {
	strand = (relStrand == '+' ? '-' : '+');	/* Flip strand */
	end = upstreamPos->end - relStart;
	start = end - motif->columnCount;
	}
    AllocVar(pos);
    pos->chrom = upstreamPos->chrom;
    pos->start = start;
    pos->end = end;
    pos->name = upstreamPos->name;
    pos->score = 1000;	/* Not yet calculated. */
    pos->strand = strand;
    pos->motif = motif->name;
    slAddHead(&posList, pos);
    }
return posList;
}

char *fixMotifName(char *in, char *out, int outSize)
/* Fix motif name to make it consistent.  (Convert
 * 'Cluster XXX' to 'Motif_XXX' */
{
if (startsWith("Cluster", in))
    safef(out, outSize, "Motif_%s", in+8);
else
    safef(out, outSize, "%s", in);
return out;
}


void loadGeneToMotif(struct sqlConnection *conn, char *fileName, char *table,
	struct hash *geneToModuleHash, struct hash *moduleAndMotifHash,
	struct hash *motifHash, struct hash *positionsHash,
	char *regionTable)
/* Load file which is a big matrix with genes for rows and motifs for
 * columns.  There is a semicolon-separated list of numbers in the matrix 
 * where a gene has the motif, and an empty (tab separated) field
 * where there is no motif.  The numbers are relative to the
 * region associated with the gene in the positionsHash. 
 * Only load bits of this where motif actually occurs in module associated 
 * with gene. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
FILE *f = hgCreateTabFile(tmpDir, table);
char *motifNames[32*1024], *row[32*1024];
int motifCount, rowSize, i;
char *gene, *module;
int geneCount = 0, total = 0;
struct dyString *dy = dyStringNew(512);
struct genomePos *motifPosList = NULL, *motifPosForGene;
struct genomePos *regionPosList = NULL, *regionPos;

/* Read first line, which is labels. */
if (!lineFileNextReal(lf, &line))
    errAbort("Empty file %s", fileName);
subChar(line, ' ', '_');
motifCount = chopLine(line, motifNames);
if (motifCount >= ArraySize(motifNames))
    errAbort("Too many motifs line 1 of %s", fileName);
lineFileExpectAtLeast(lf, 2, motifCount);
motifNames[0] = NULL;
for (i=1; i<motifCount; ++i)
    {
    char name[64];
    motifNames[i] = cloneString(fixMotifName(motifNames[i],name,sizeof(name)));
    if (!hashLookup(motifHash, motifNames[i]))
        errAbort("Motif %s is in %s but not modules_motifs.gxm",
		motifNames[i], fileName);
    }

/* Read subsequent lines. */
while ((rowSize = lineFileChopTab(lf, row)) != 0)
    {
    lineFileExpectWords(lf, motifCount, rowSize);
    gene = row[0];
    module = hashFindVal(geneToModuleHash, gene);
    if (module == NULL)
	{
        warn("WARNING: Gene %s in line %d of %s but not module_assignments.tab", 
		gene, lf->lineIx, lf->fileName);
	continue;
	}
    regionPos = NULL;
    for (i=1; i<rowSize; ++i)
        {
	if (row[i][0] != 0)
	    {
	    if (hashLookup2(moduleAndMotifHash, module, motifNames[i]))
		{
		regionPos = hashFindVal(positionsHash, gene);
		if (regionPos == NULL)
		    {
		    warn("WARNING: %s in %s but not gene_positions.tab",
		    	gene, fileName);
		    i = rowSize; continue;
		    }
		
		motifPosForGene = convertMotifPos(row[i], regionPos, 
			hashMustFindVal(motifHash, motifNames[i]), lf);
		motifPosList = slCat(motifPosForGene, motifPosList);
		++total;
		}
	    }
	}
    if (regionPos != NULL)
        {
	slAddHead(&regionPosList, regionPos);
	}
    ++geneCount;
    }
lineFileClose(&lf);

/* Output sorted table of all motif hits. */
    {
    struct genomePos *pos;
    slSort(&motifPosList, genomePosCmp);
    for (pos = motifPosList; pos != NULL; pos = pos->next)
	{
	int start = pos->start;
	int end = pos->end;
	if (start < 0) start = 0;
	fprintf(f, "%d\t", binFromRange(start, end));
	fprintf(f, "%s\t", pos->chrom);
	fprintf(f, "%d\t%d\t", start, end);
	fprintf(f, "%s\t", pos->motif);
	fprintf(f, "%d\t", pos->score);
	fprintf(f, "%c\t", pos->strand);
	fprintf(f, "%s\n", pos->name);
	}
    dyStringPrintf(dy,
    "CREATE TABLE  %s (\n"
    "    bin smallInt unsigned not null,\n"
    "    chrom varChar(255) not null,\n"
    "    chromStart int not null,\n"
    "    chromEnd int not null,\n"
    "    name varchar(255) not null,\n"
    "    score int not null,\n"
    "    strand char(1) not null,\n"
    "    gene varchar(255) not null,\n"
    "              #Indices\n"
    "    INDEX(gene(12)),\n"
    "    INDEX(name(16)),\n"
    "    INDEX(chrom(8),bin)\n"
    ")\n",  table);
    sqlRemakeTable(conn, table, dy->string);
    verbose(1, "%d genes, %d motifs, %d motifs in genes\n",
	    geneCount, motifCount-1, total);
    hgLoadTabFile(conn, tmpDir, table, &f);
    // hgRemoveTabFile(tmpDir, table);
    verbose(1, "Loaded %s table\n", table);
    slFreeList(&motifPosList);
    }

/* Now output sorted table of upstream regions. */
    {
    FILE *f = hgCreateTabFile(tmpDir, regionTable);
    struct genomePos *pos;
    dyStringClear(dy);
    dyStringPrintf(dy,
    "CREATE TABLE  %s (\n"
    "    bin smallInt unsigned not null,\n"
    "    chrom varChar(255) not null,\n"
    "    chromStart int not null,\n"
    "    chromEnd int not null,\n"
    "    name varchar(255) not null,\n"
    "    score int not null,\n"
    "    strand char(1) not null,\n"
    "              #Indices\n"
    "    INDEX(name(16)),\n"
    "    INDEX(chrom(8),bin)\n"
    ")\n",  regionTable);
    sqlRemakeTable(conn, regionTable, dy->string);
    slSort(&regionPosList, genomePosCmp);
    for (pos = regionPosList; pos != NULL; pos = pos->next)
	{
	int start = pos->start;
	int end = pos->end;
	if (start < 0) start = 0;
	fprintf(f, "%d\t", binFromRange(start, end));
	fprintf(f, "%s\t", pos->chrom);
	fprintf(f, "%d\t%d\t", start, end);
	fprintf(f, "%s\t", pos->name);
	fprintf(f, "%d\t", pos->score);
	fprintf(f, "%c\n", pos->strand);
	}
    hgLoadTabFile(conn, tmpDir, regionTable, &f);
    // hgRemoveTabFile(tmpDir, regionTable);
    }
}


struct hash *loadModuleToMotif(struct sqlConnection *conn, char *fileName, 
	char *table)
/* Load up file which has a line per module.  The first word is the module
 * number, the rest of the tab-separated fields are motif names. 
 * Return hash keyed by module&motif. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *module, *motif;
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
verbose(1, "%d modules, %d motifs in modules\n",
	moduleCount, motifCount);
hgLoadTabFile(conn, tmpDir, table, &f);
hgRemoveTabFile(tmpDir, table);
verbose(1, "Loaded %s table\n", table);
lineFileClose(&lf);
return hash;
}


struct hash *loadMotifWeights(struct sqlConnection *conn, char *fileName, 
	char *table)
/* Load in XML weight motif file and save it in tab-separated format
 * and in hash keyed by motif name. */
{
struct esmMotifs *motifs = esmMotifsLoad(fileName);
struct esmMotif *motif;
FILE *f = hgCreateTabFile(tmpDir, table);
struct dyString *dy = dyStringNew(512);
struct hash *hash = newHash(16);

for (motif = motifs->esmMotif; motif != NULL; motif = motif->next)
    {
    struct esmWeights *weights = motif->esmWeights;
    int posCount = slCount(weights->esmPosition);
    struct esmPosition *pos;
    struct dnaMotif *dm;
    char name[64];


    fixMotifName(motif->Name, name, sizeof(name));
    AllocVar(dm);
    dm->name = cloneString(name);
    dm->columnCount = posCount;
    AllocArray(dm->aProb, posCount);
    AllocArray(dm->cProb, posCount);
    AllocArray(dm->gProb, posCount);
    AllocArray(dm->tProb, posCount);
    for (pos = weights->esmPosition; pos != NULL; pos = pos->next)
        {
	char *row[5];
	double odds[4], sumOdds = 0;
	int i;

	int ix = pos->Num;
	int rowSize = chopString(pos->Weights, ";", row, ArraySize(row));
	if (rowSize != 4)
	    errAbort("Expecting 4 values for weights in position %d of Motif %s",
               pos->Num, motif->Name);
	if (ix >= posCount)
	    errAbort("Num %d out of range in Motif %s", ix, motif->Name);
	for (i=0; i<4; ++i)
	    {
	    odds[i] = exp(atof(row[0]));
	    sumOdds += odds[i];
	    }
	dm->aProb[ix] = odds[0]/sumOdds;
	dm->cProb[ix] = odds[1]/sumOdds;
	dm->gProb[ix] = odds[2]/sumOdds;
	dm->tProb[ix] = odds[3]/sumOdds;
	}
    dnaMotifTabOut(dm, f);
    hashAdd(hash, dm->name, dm);
    }
dyStringPrintf(dy,
"CREATE TABLE %s (\n"
"    name varchar(16) not null,	# Motif name.\n"
"    columnCount int not null,	# Count of columns in motif.\n"
"    aProb longblob not null,	# Probability of A's in each column.\n"
"    cProb longblob not null,	# Probability of C's in each column.\n"
"    gProb longblob not null,	# Probability of G's in each column.\n"
"    tProb longblob not null,	# Probability of T's in each column.\n"
"              #Indices\n"
"    PRIMARY KEY(name)\n"
")\n", table);
sqlRemakeTable(conn, table, dy->string);
hgLoadTabFile(conn, tmpDir, table, &f);
hgRemoveTabFile(tmpDir, table);
verbose(1, "Processed %d motifs into %s\n", slCount(motifs->esmMotif), table);
return hash;
}

void crossCheck(struct sqlConnection *conn,
	char *motifWeights, char *geneToModule, char *geneToMotif, char *moduleToMotif)
/* Do sanity check after loading.  */
{
struct hash *motifHash = newHash(0);
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

/* Load up motif hash */
safef(query, sizeof(query), "select name from %s", motifWeights);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(motifHash, row[0], NULL);
    }
sqlFreeResult(&sr);

/* Load up moduleToMotif table and note how many times
 * a motif is used more than once just for curiousity
 * (this is not an error condition). */
safef(query, sizeof(query), "select module,motif from %s", moduleToMotif);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *module = cloneString(row[0]);
    char *motif = cloneString(row[1]);
    if (!hashLookup(motifHash, motif))
        {
	warn("Motif %s is in %s but not %s", motif, 
		moduleToMotif, motifWeights);
	++fatalErrorCount;
	}
    hashAdd(moduleToMotifHash, module, motif);
    if (hashLookup(motifToModuleHash, motif))
        ++reusedMotifCount;
    hashAdd(motifToModuleHash, motif, module);
    safef(modMot, sizeof(modMot), "%s %s", module, motif);
    hashAdd(modMotHash, modMot, NULL);
    }
sqlFreeResult(&sr);
verbose(1, "%d motifs reuses in modules\n", reusedMotifCount);

verbose(1, "Cross-checking tables\n");

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
safef(query, sizeof(query), "select gene,name from %s", geneToMotif);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = row[0];
    char *motif = row[1];
    char *module = hashFindVal(geneToModuleHash, gene);
    if (module == NULL)
        {
	warn("Gene %s is in %s but not %s", gene, geneToMotif, geneToModule);
	++fatalErrorCount;
	}
    safef(modMot, sizeof(modMot), "%s %s", module, motif);
    if (hashLookup(modMotHash, modMot) == NULL)
        {
	warn("Gene %s has motif %s, but that motif isn't in %s",
	    gene, motif, module);
	++fatalErrorCount;
	}
    if (!hashLookup(motifHash, motif))
        {
	warn("Motif %s is in %s but not %s", motif, 
		geneToMotif, motifWeights);
	++fatalErrorCount;
	}
    }
sqlFreeResult(&sr);

verbose(1, "%d errors\n", fatalErrorCount);
}


void hgLoadEranModules(char *database, 
	char *geneToModule, char *moduleToMotif, char *motifWeights,
	char *motifPositions, char *genePositions)
/* hgLoadEranModules - Load regulatory modules from Eran Segal. */
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *positionsHash = loadGenePositions(database, conn, genePositions);
struct hash *moduleAndMotifHash = loadModuleToMotif(conn, moduleToMotif, "esRegModuleToMotif");
struct hash *geneToModuleHash = loadGeneToModule(conn, geneToModule, "esRegGeneToModule");
struct hash *motifHash = loadMotifWeights(conn, motifWeights, "esRegMotif");
loadGeneToMotif(conn, motifPositions, "esRegGeneToMotif", geneToModuleHash, moduleAndMotifHash, motifHash, positionsHash, "esRegUpstreamRegion");
crossCheck(conn, "esRegMotif", "esRegGeneToModule", "esRegGeneToMotif", "esRegModuleToMotif");
}


#ifdef TESTING
void convert(double logOdds[4])
{
int i;
double sum = 0;
for (i=0; i<4; ++i)
    {
    double odds = exp(logOdds[i]);
    sum += odds;
    }
uglyf("sum %f\n", sum);
for (i=0; i<4; ++i)
    {
    double odds = exp(logOdds[i]);
    printf("%4.3f\t %4.3f\t%4.3f\n", logOdds[i], odds, odds/sum);
    }
printf("\n");
}

void test()
{
int i;
static double w[][4] = 
{
      {0.11,0.24,-0.79,0.18},
      {-0.17,0.91,-0.45,-0.67},
      {0.93,-5.75,-0.45,0.12},
      {-0.75,0.18,-0.45,0.63},
      {1.13,-5.75,0.70,-6.03},
      {-6.10,-5.75,-2.34,1.83},
      {-2.65,2.11,-5.80,-6.03},
      {1.40,-5.75,-0.13,-2.57},
      {-6.10,0.91,-5.80,1.11},
      {-2.65,-0.79,-0.45,1.31},
      {0.69,-0.39,-0.85,-0.09},
      {-0.75,-0.39,-2.34,1.21},
      {0.93,-1.35,-0.45,-0.35},
      {-0.17,-0.07,-0.13,0.31},
      {1.81,-5.75,-5.80,-6.03},
      {-6.10,2.17,-5.80,-6.03},
      {-6.10,-5.75,2.11,-6.03},
      {0.55,-5.75,0.13,0.31},
      {0.81,-0.79,-0.13,-0.67},
      {0.46,-0.01,-0.79,-0.03},
      {-0.11,0.46,0.19,-0.61},
      {0.11,-0.33,-1.35,0.69},

};
for (i=0; i<=21; ++i)
    convert(w[i]);
}
#endif /* TESTING */

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
hgLoadEranModules(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
