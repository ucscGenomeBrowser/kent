/* hgFlyBase - Parse FlyBase genes.txt file and turn it into a couple of 
 * tables.  See http://flybase.org/.data/docs/refman/refman-B.html for a 
 * description of these files.  We don't try too hard to extract and 
 * normalize every piece of data here,  just the particularly valuable parts 
 * we can't find elsewhere. In particular we want to get information on 
 * wild-type function, phenotypes, and synonyms.
 *
 * Flybase records are separated by a line with a '#'.  The fields
 * are line separated and each begin with a two character code that
 * defines the line type.  Most fields start with '*' and a letter.
 * The first field is always '*a' and contains the gene symbol.
 * A particularly important and complex field is the '*E' field.
 * This describes a literature reference and data derived from it.
 * Subfields of the *E field start with % instead of * and are followed
 * by a single letter which means the same as when the field is
 * standalone.
 *
 * The '*A' field, indicating an allele, is also important and
 * constains subfields.  This subfields start with '$'.  If an
 * *A field has a $E subfield, the $E's sub-sub-fields start with
 * a '&'.
 *
 * From this we want to extract the following field types:
 *   z - flybase ID
 *   a - gene symbol
 *   e - gene name
 *   i - synonyms (just for genes, not for alleles)
 *   r - wild-type biological role
 *   p - phenotypes of genes
 *   A - allele with subfields
 *   E - reference with subfields
 *   
 * We'll make this into the following tables:
 *    fbGene <flybase ID><gene symbol><gene name>
 *    fbSynonym <flybase ID><synonom>  (This includes gene symbol and name)
 *    fbAllele <allele ID><flybase ID><allele name>
 *    fbRef <reference ID><text> (These are not cleaned up from flybase)
 *    fbRole <flybase ID><allele ID><reference ID><text>
 *           The allele or reference ID's may be zero indicating
 *           the annotation belongs the gene as a whole or flybase
 *           respectively.
 *    fbPhenotype <flybase ID><allele ID><reference ID><text>
 * Note that we are ignoring the GO terms since we'll get them
 * from SwissProt, where the data is in a somewhat more regular
 * format. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "portable.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgFlyBase.c,v 1.2 2003/10/27 09:49:18 kent Exp $";

char *tabDir = ".";
boolean doLoad;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFlyBase - Parse FlyBase genes.txt file and turn it into a couple of tables\n"
  "usage:\n"
  "   hgFlyBase database genes.txt\n"
  "options:\n"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  );
}

static struct optionSpec options[] = {
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {NULL, 0},
};

struct dyString *suckSameLines(struct lineFile *lf, char *line)
/* Suck up lines concatenating as long as they begin with the same
 * first two characters as initial line. */
{
struct dyString *dy = dyStringNew(0);
char c1 = line[0], c2 = line[1];
dyStringAppend(dy, line+3);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] != c1 || line[1] != c2)
        {
	lineFileReuse(lf);
	break;
	}
    dyStringAppend(dy, line+2);
    }
return dy;
}


char *naForNull(char *s)
/* Return n/a if s is null, else return s. */
{
if (s == NULL)
    return cloneString("n/a");
else
    return s;
}

struct ref
/* Keep track of reference. */
    {
    int id;	/* Reference ID. */
    };

void remakeTables(struct sqlConnection *conn)
/* Remake all our tables. */
{
sqlRemakeTable(conn, "fbGene", 
"#Links FlyBase IDs, gene symbols and gene names\n"
"CREATE TABLE fbGene (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    geneSym varchar(255) not null,	# Short gene symbol\n"
"    geneName varchar(255) not null,	# Gene name - up to a couple of words\n"
"              #Indices\n"
"    PRIMARY KEY(geneId(11)),\n"
"    INDEX(geneSym(8)),\n"
"    INDEX(geneName(12))\n"
")\n");

sqlRemakeTable(conn, "fbSynonym", 
"#Links all the names we call a gene to it's flybase ID\n"
"CREATE TABLE fbSynonym (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    name varchar(255) not null,	# A name (synonym or real\n"
"              #Indices\n"
"    INDEX(geneId(11)),\n"
"    INDEX(name(12))\n"
")\n");

sqlRemakeTable(conn, "fbAllele", 
"#The alleles of a gene\n"
"CREATE TABLE fbAllele (\n"
"    id int not null,	# Allele ID\n"
"    geneId varchar(255) not null,	# Flybase ID of gene\n"
"    name varchar(255) not null,	# Allele name\n"
"              #Indices\n"
"    PRIMARY KEY(id),\n"
"    INDEX(geneId(11))\n"
")\n");

sqlRemakeTable(conn, "fbRef", 
"#A literature or sometimes database reference\n"
"CREATE TABLE fbRef (\n"
"    id int not null,	# Reference ID\n"
"    text longblob not null,	# Usually begins with flybase ref ID, but not always\n"
"              #Indices\n"
"    PRIMARY KEY(id)\n"
")\n");

sqlRemakeTable(conn, "fbRole", 
"#Role of gene in wildType\n"
"CREATE TABLE fbRole (\n"
"    geneId varchar(255) not null,	# Flybase Gene ID\n"
"    fbAllele int not null,	# ID in fbAllele table or 0 if not allele-specific\n"
"    fbRef int not null,	# ID in fbRef table\n"
"    text longblob not null,	# Descriptive text\n"
"              #Indices\n"
"    INDEX(geneId(11))\n"
")\n");

sqlRemakeTable(conn, "fbPhenotype", 
"#Observed phenotype in mutant.  Sometimes contains gene function info\n"
"CREATE TABLE fbPhenotype (\n"
"    geneId varchar(255) not null,	# Flybase Gene ID\n"
"    fbAllele int not null,	# ID in fbAllele table or 0 if not allele-specific\n"
"    fbRef int not null,	# ID in fbRef table\n"
"    text longblob not null,	# Descriptive text\n"
"              #Indices\n"
"    INDEX(geneId(11))\n"
")\n");
}

void hgFlyBase(char *database, char *genesFile)
/* hgFlyBase - Parse FlyBase genes.txt file and turn it into a couple of 
 * tables. */
{
char *tGene = "fbGene";
char *tSynonym = "fbSynonym";
char *tAllele = "fbAllele";
char *tRef = "fbRef";
char *tRole = "fbRole";
char *tPhenotype = "fbPhenotype";
FILE *fGene = hgCreateTabFile(tabDir, tGene);
FILE *fSynonym = hgCreateTabFile(tabDir, tSynonym);
FILE *fAllele = hgCreateTabFile(tabDir, tAllele);
FILE *fRef = hgCreateTabFile(tabDir, tRef);
FILE *fRole = hgCreateTabFile(tabDir, tRole);
FILE *fPhenotype = hgCreateTabFile(tabDir, tPhenotype);
struct lineFile *lf = lineFileOpen(genesFile, TRUE);
struct hash *refHash = newHash(19);
int nextRefId = 0;
int nextAlleleId = 0;
char *line, sub, type, *rest;
char *geneSym = NULL, *geneName = NULL, *geneId = NULL;
int recordCount = 0;
struct slName *synList = NULL, *syn;
int curAllele = 0, curRef = 0;
struct ref *ref = NULL;
struct sqlConnection *conn;

/* Make dummy reference for flybase itself. */
fprintf(fRef, "0\tFlyBase\n");

/* Loop through parsing and writing tab files. */
while (lineFileNext(lf, &line, NULL))
    {
    sub = line[0];
    if (sub == '#')
	{
	/* End of record. */
	++recordCount;
	if (geneId == NULL)
	    errAbort("Record without *z line ending line %d of %s",
		lf->lineIx, lf->fileName);

	/* Write out synonyms. */
	if (geneSym != NULL)
	    slNameStore(&synList, geneSym);
	if (geneName != NULL)
	    slNameStore(&synList, geneName);
	for (syn = synList; syn != NULL; syn = syn->next)
	    fprintf(fSynonym, "%s\t%s\n", geneId, syn->name);

	/* Write out gene record. */
	geneSym = naForNull(geneSym);
	geneName = naForNull(geneName);
	fprintf(fGene, "%s\t%s\t%s\n", geneId, geneSym, geneName);

	/* Clean up. */
	freez(&geneSym);
	freez(&geneName);
	freez(&geneId);
	slFreeList(&synList);
	ref = NULL;
	curRef = curAllele = 0;
	continue;
	}
    else if (sub == 0)
       errAbort("blank line %d of %s, not allowed in gene.txt",
	    lf->lineIx, lf->fileName);
    else if (isalnum(sub))
       errAbort("line %d of %s begins with %c, not allowed",
	    lf->lineIx, lf->fileName, sub);
    type = line[1];
    rest = trimSpaces(line+2);
    if (sub == '*' && type == 'a')
	geneSym = cloneString(rest);
    else if (sub == '*' && type == 'e')
        geneName = cloneString(rest);
    else if (sub == '*' && type == 'z')
	{
        geneId = cloneString(rest); 
	if (!startsWith("FBgn", geneId))
	    errAbort("Bad FlyBase gene ID %s line %d of %s", geneId, 
		lf->lineIx, lf->fileName);
	}
    else if (type == 'i' && (sub == '*') || (sub == '$'))
	slNameStore(&synList, rest);
    else if (sub == '*' && type == 'A')
        {
	if (geneId == NULL)
	    errAbort("Allele before geneId line %d of %s", 
	    	lf->lineIx, lf->fileName);
	curAllele = ++nextAlleleId;
	fprintf(fAllele, "%d\t%s\t%s\n", curAllele, geneId, rest);
	if (!sameString(rest, "classical") &&
	    !sameString(rest, "in vitro") &&
	    !sameString(rest, "wild-type") )
	    {
	    fprintf(fSynonym, "%s\t%s\n", geneId, rest);
	    }
	}
    else if (type == 'E')
        {
	ref = hashFindVal(refHash, rest);
	if (ref == NULL)
	    {
	    AllocVar(ref);
	    ref->id = ++nextRefId;
	    hashAdd(refHash, rest, ref);
	    subChar(rest, '\t', ' ');
	    fprintf(fRef, "%d\t%s\n", ref->id, rest);
	    }
	curRef = ref->id;
	}
    else if ((type == 'k' || type == 'r' || type == 'p') && sub != '@')
        {
	FILE *f = (type == 'r' ? fRole : fPhenotype);
	struct dyString *dy = suckSameLines(lf, line);
	subChar(dy->string, '\t', ' ');
	if (geneId == NULL)
	    errAbort("Expecting *z in record before line %d of %s",
	    	lf->lineIx, lf->fileName);
	fprintf(f, "%s\t%d\t%d\t%s\n", geneId, curAllele, curRef, dy->string);
	dyStringFree(&dy);
	}
    }
printf("Processed %d records in %d lines\n", recordCount, lf->lineIx);
lineFileClose(&lf);

conn = sqlConnect(database);
remakeTables(conn);

if (doLoad)
    {
    printf("Loading %s\n", tGene);
    hgLoadTabFile(conn, tabDir, tGene, &fGene);
    printf("Loading %s\n", tSynonym);
    hgLoadTabFile(conn, tabDir, tSynonym, &fSynonym);
    printf("Loading %s\n", tAllele);
    hgLoadTabFile(conn, tabDir, tAllele, &fAllele);
    printf("Loading %s\n", tRef);
    hgLoadTabFile(conn, tabDir, tRef, &fRef);
    printf("Loading %s\n", tRole);
    hgLoadTabFile(conn, tabDir, tRole, &fRole);
    printf("Loading %s\n", tPhenotype);
    hgLoadTabFile(conn, tabDir, tPhenotype, &fPhenotype);
    hgRemoveTabFile(tabDir, tGene);
    hgRemoveTabFile(tabDir, tSynonym);
    hgRemoveTabFile(tabDir, tAllele);
    hgRemoveTabFile(tabDir, tRef);
    hgRemoveTabFile(tabDir, tRole);
    hgRemoveTabFile(tabDir, tPhenotype);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
doLoad = !optionExists("noLoad");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
hgFlyBase(argv[1], argv[2]);
return 0;
}
