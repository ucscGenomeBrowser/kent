/* hgStanfordMicroarray - Load up from Stanford Microarray Database files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgStanfordMicroarray.c,v 1.1 2003/09/24 00:14:15 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"hgStanfordMicroarray - Load up from Stanford Microarray Database files\n"
"usage:\n"
"   hgStanfordMicroarray database table expTable dir\n"
"example:\n"
"   hgStanfordMicroarray hg16 fibroSerum fibroSerumExps spots\n"
"options:\n"
"   -swap - Use red channel for reference, not green\n"
"   -trimName=XXXX - Remove XXXX from experiment name. May include ? wildcard\n"
"   -trimTissue=XXXX - Remove XXXX from tissue.  May include ? wildcard.\n"
"   -suppress=XXXX - Suppress experiments of given name\n"
"   -dataField=XXXX - Field with log2ratio data. Default LOG_RAT2N_MEAN\n"
"   -geneField=XXXX - Field with gene name. Default NAME\n"
"   -url=XXXX - Url to put in expTable\n"
"   -credit=XXXX - Credits to put in expTable\n"
"   -ref=XXX - Reference to put in expTable\n");
}

boolean clSwap = FALSE;
char *clTrimName = NULL;
char *clTrimTissue = NULL;
char *clSuppress = NULL;
char *clDataField = "LOG_RAT2N_MEAN";
char *clGeneField = "NAME";
char *clUrl = "n/a";
char *clCredit = "n/a";
char *clRef = "n/a";

static struct optionSpec options[] = {
   {"swap", OPTION_BOOLEAN,},
   {"trimName", OPTION_STRING,},
   {"trimTissue", OPTION_STRING,},
   {"suppress", OPTION_STRING,},
   {"dataField", OPTION_STRING,},
   {NULL, 0},
};

#define missingData -10000

struct geneSpots
/* Data associated with one gene. */
    {
    struct geneSpots *next;
    char *name;		/* Gene name */
    double *spots;	/* Spot data. */
    };

struct geneSpots *geneSpotsNew(char *name, int maxSpots)
/* Allocate new geneSpots filled with missing data markers. */
{
struct geneSpots *gene;
int i;
double *spots;
AllocVar(gene);
gene->name = cloneString(name);
gene->spots = AllocArray(spots, maxSpots);
for (i=0; i<maxSpots; ++i)
    spots[i] = missingData;
return gene;
}

struct experiment
/* Info on an experiment. */
    {
    struct experiment *next;
    char *file;		/* File name. */
    char *name;		/* Experiment name. */
    char *tissue;	/* Tissue name. */
    };

boolean wildStartsWith(char *wild, char *s)
/* Return TRUE if s starts with wild, where wild
 * may include '?' chars (but not '*'). */
{
char w, c;
while ((w = *wild++) != 0)
    {
    if ((c = *s++) == 0)
        return FALSE;
    if (w == '?')
        continue;
    if (c != w)
        return FALSE;
    }
return TRUE;
}

void wildStripString(char *s, char *strip)
/* Remove all occurences of strip from s, where s can
 * include ? wildcards. */
{
char c, *in = s, *out = s;
int stripSize = strlen(strip);
char stripFirst = strip[0];

if (strchr(strip, '*'))
    errAbort("Sorry, no '*' allowed in strips.");

while ((c = *in) != 0)
    {
    c = *in;
    if (c == stripFirst)
        {
	if (wildStartsWith(strip, in))
	    {
	    in += stripSize;
	    continue;
	    }
	}
    *out = c;
    ++out;
    ++in;
    }
*out = 0;
}

void addOneXls(char *fileName, int maxExps, struct experiment **pExpList, 
	struct geneSpots **pGeneList, struct hash *geneHash, int *pExpCount)
/* Add data from one file to lists and hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, **row;
int columnCount, i;
int dataIx, geneIx, expIx = *pExpCount;

/* First read preamble - everything that starts with '!' */
    {
    char *nameField;
    int skipSize;
    struct experiment *exp;
    boolean gotHead = FALSE;
    boolean gotExp = FALSE;
    if (clSwap)
        nameField = "!Channel 1 Description=";
    else
        nameField = "!Channel 2 Description=";
    skipSize = strlen(nameField);
    AllocVar(exp);
    while (lineFileNextReal(lf, &line))
        {
	if (line[0] != '!')
	    break;
	if (startsWith(nameField, line))
	    {
	    gotHead = TRUE;
	    line = trimSpaces(line+skipSize);
	    if (clSuppress == NULL || !sameString(clSuppress, line))
	        {
		gotExp = TRUE;
		if (clTrimName != NULL)
		    {
		    wildStripString(line, clTrimName);
		    line = trimSpaces(line);
		    }
		exp->file = cloneString(fileName);
		exp->name = cloneString(line);
		if (clTrimTissue != NULL)
		    {
		    wildStripString(line, clTrimTissue);
		    line = trimSpaces(line);
		    }
		exp->tissue = cloneString(line);
		uglyf("%s\t%s\n", exp->name, exp->file);
		}
	    }
	}
    if (!gotHead)
        errAbort("%s doesn't seem to be a Stanford exp file.  No %s", 
		fileName, nameField);
    if (!gotExp)
        return;
    slAddHead(pExpList, exp);
    }

/* Next go through the label line and look for the gene and data
 * fields. */
    {
    columnCount = countChars(line, '\t') + 1;
    AllocArray(row, columnCount);
    if (chopByChar(line, '\t', row, columnCount) != columnCount)
        errAbort("Malformed label line %d of %s", lf->lineIx, lf->fileName);
    geneIx = stringArrayIx(clGeneField, row, columnCount);
    if (geneIx < 0)
        errAbort("Can't find %s line %d of %s", 
		clGeneField, lf->lineIx, lf->fileName);
    dataIx = stringArrayIx(clDataField, row, columnCount);
    if (dataIx < 0)
        errAbort("Can't find %s line %d of %s", 
		clDataField, lf->lineIx, lf->fileName);
    }

/* Loop through each line in file.  Adding new genes and new data. */
    {
    while (lineFileNextReal(lf, &line))
        {
	char *name, *data;
	struct geneSpots *gene;
	if (chopByChar(line, '\t', row, columnCount) != columnCount)
	    errAbort("Expecting %d fields line %d of %s", 
	    	lf->lineIx, lf->fileName);
	data = row[dataIx];
	if (data[0] != 0)
	    {
	    name = row[geneIx];
	    gene = hashFindVal(geneHash, name);
	    if (gene == NULL)
	        {
		gene = geneSpotsNew(name, maxExps);
		hashAdd(geneHash, name, gene);
		slAddHead(pGeneList, gene);
		}
	    assert(expIx < maxExps);
	    gene->spots[expIx] = atof(data);
	    }
	}
    }

*pExpCount += 1;
lineFileClose(&lf);
}

void saveExps(char *fileName, struct experiment *expList)
/* Save out experiment list to tab file. */
{
struct experiment *exp;
FILE *f = mustOpen(fileName, "w");
int id = 0;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    fprintf(f, "%d\t", id++);
    fprintf(f, "%s\t", exp->name);
    fprintf(f, "%s\t", exp->name);
    fprintf(f, "%s\t", clUrl);
    fprintf(f, "%s\t", clRef);
    fprintf(f, "%s\t", clCredit);
    fprintf(f, "3\t");
    fprintf(f, "%s,%s,%s,\n", "n/a", "n/a", exp->tissue);
    }
carefulClose(&f);
}


void saveGenes(char *fileName, struct geneSpots *geneList, int expCount)
/* Save list of genes. */
{
struct geneSpots *gene;
FILE *f = mustOpen(fileName, "w");
int i;

for (gene = geneList; gene != NULL; gene = gene->next)
    {
    fprintf(f, "%s\t", gene->name);
    fprintf(f, "%d\t", expCount);
    for (i=0; i<expCount; ++i)
        fprintf(f, "%0.3f,", gene->spots[i]);
    fprintf(f, "\n");
    }

carefulClose(&f);
}

void hgStanfordMicroarray(char *database, char *table, char *expTable, 
	char *dir)
/* hgStanfordMicroarray - Load up from Stanford Microarray Database files. */
{
struct fileInfo *dirEl, *dirList = listDirX(dir, "*.xls", TRUE);
struct experiment *expList = NULL, *exp;
struct geneSpots *geneList = NULL, *gene;
struct hash *geneHash = newHash(17);
int maxExps = slCount(dirList);
int expCount = 0;

for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
    addOneXls(dirEl->name, maxExps, &expList, &geneList, geneHash, &expCount);
slReverse(&expList);
slReverse(&geneList);
uglyf("Got %d experiments, %d genes\n", slCount(expList), slCount(geneList));

saveExps("exp.tab", expList);
saveGenes("gene.tab", geneList, expCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
clSwap = optionExists("swap");
clTrimName = optionVal("trimName", NULL);
clTrimTissue = optionVal("trimTissue", NULL);
clSuppress = optionVal("suppress", NULL);
clDataField = optionVal("dataField", clDataField);
clGeneField = optionVal("geneField", clGeneField);
clUrl = optionVal("url", clUrl);
clCredit = optionVal("credit", clCredit);
clRef = optionVal("ref", clRef);
hgStanfordMicroarray(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
