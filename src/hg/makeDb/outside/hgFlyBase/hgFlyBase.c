/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
 *   d - biological role of gene product
 *   e - gene name
 *   f - cellular compartment of which gene product is a component
 *   i - synonyms (just for genes, not for alleles)
 *   r - wild-type biological role
 *   p - phenotypes of genes
 *   A - allele with subfields
 *   E - reference with subfields
 *   F - function of gene product
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
 *    fbGo <flybase ID><go ID><go aspect>
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


char *tabDir = ".";
boolean doLoad;
char *geneTable = "bdgpGene";
boolean doTranscript = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFlyBase - Parse FlyBase genes.txt file and turn it into a couple of tables\n"
  "usage:\n"
  "   hgFlyBase database genes.txt\n"
  "options:\n"
  "   -geneTable=tbl - Use tbl instead of default %s\n"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  , geneTable
  );
}

static struct optionSpec options[] = {
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"geneTable", OPTION_STRING},
   {NULL, 0},
};

char *ungreekTable[] = {
   "agr", "alpha",
   "bgr", "beta",
   "dgr", "delta",
   "egr", "epsilon",
   "eegr", "eta",
   "ggr", "gamma",
   "igr", "iota",
   "ohgr", "omega",
   "thgr", "theta",
   "zgr", "zeta",
};

char *hideTagTable[] = {
   "up", "/up",
   "down", "/down",
};

char *spelledGreek(char *abbr)
/* Convert from 'agr;' to 'alpha' or return NULL */
{
int i;
for (i=0; i<ArraySize(ungreekTable); i += 2)
    if (sameWord(ungreekTable[i], abbr))
        return ungreekTable[i+1];
return NULL;
}

char *ungreek(char *s)
/* Get rid of greek characters and unwanted tags. */
{
struct dyString *dy = newDyString(0);
char *result;
char c;
while ((c = *s++) != 0)
    {
    boolean special = FALSE;
    if (c == '&')
        {
	char *e = strchr(s, ';');
	if (e != NULL)
	    {
	    int size = e - s;
	    if (size <= 4)
	        {
		char abbr[5];
		char *english;
		memcpy(abbr, s, size);
		abbr[size] = 0;
		english = spelledGreek(abbr);
		if (english != NULL)
		    {
		    dyStringAppend(dy, english);
		    special = TRUE;
		    s = e+1;
		    if (s[0] == '\'')
		       ++s;
		    }
		}
	    }
	}
    else if (c == '<')
        {
	char *e = strchr(s, '>');
	if (e != NULL)
	    {
	    int size = e - s;
	    if (size < 8)
	        {
		int i;
		char tag[8];
		memcpy(tag, s, size);
		tag[size] = 0;
		for (i=0; i<ArraySize(hideTagTable); ++i)
		    {
		    if (sameWord(tag, hideTagTable[i]))
		        {
			special = TRUE;
			s = e+1;
			break;
			}
		    }
		}
	    }
	}
    if (!special)
        dyStringAppendC(dy, c);
    }
result = cloneString(dy->string);
dyStringFree(&dy);
return result;
}

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
NOSQLINJ "CREATE TABLE fbGene (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    geneSym varchar(255) not null,	# Short gene symbol\n"
"    geneName varchar(255) not null,	# Gene name - up to a couple of words\n"
"              #Indices\n"
"    PRIMARY KEY(geneId(11)),\n"
"    INDEX(geneSym(8)),\n"
"    INDEX(geneName(12))\n"
")\n");

sqlRemakeTable(conn, "fbTranscript", 
"#Links FlyBase gene IDs and BDGP transcripts\n"
NOSQLINJ "CREATE TABLE fbTranscript (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    transcriptId varchar(255) not null,	# BDGP Transcript ID\n"
"              #Indices\n"
"    PRIMARY KEY(transcriptId(11)),\n"
"    INDEX(transcriptId(11))\n"
")\n");

sqlRemakeTable(conn, "fbSynonym", 
"#Links all the names we call a gene to it's flybase ID\n"
NOSQLINJ "CREATE TABLE fbSynonym (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    name varchar(255) not null,	# A name (synonym or real\n"
"              #Indices\n"
"    INDEX(geneId(11)),\n"
"    INDEX(name(12))\n"
")\n");

sqlRemakeTable(conn, "fbAllele", 
"#The alleles of a gene\n"
NOSQLINJ "CREATE TABLE fbAllele (\n"
"    id int not null,	# Allele ID\n"
"    geneId varchar(255) not null,	# Flybase ID of gene\n"
"    name varchar(255) not null,	# Allele name\n"
"              #Indices\n"
"    PRIMARY KEY(id),\n"
"    INDEX(geneId(11))\n"
")\n");

sqlRemakeTable(conn, "fbRef", 
"#A literature or sometimes database reference\n"
NOSQLINJ "CREATE TABLE fbRef (\n"
"    id int not null,	# Reference ID\n"
"    text longblob not null,	# Usually begins with flybase ref ID, but not always\n"
"              #Indices\n"
"    PRIMARY KEY(id)\n"
")\n");

sqlRemakeTable(conn, "fbRole", 
"#Role of gene in wildType\n"
NOSQLINJ "CREATE TABLE fbRole (\n"
"    geneId varchar(255) not null,	# Flybase Gene ID\n"
"    fbAllele int not null,	# ID in fbAllele table or 0 if not allele-specific\n"
"    fbRef int not null,	# ID in fbRef table\n"
"    text longblob not null,	# Descriptive text\n"
"              #Indices\n"
"    INDEX(geneId(11))\n"
")\n");

sqlRemakeTable(conn, "fbPhenotype", 
"#Observed phenotype in mutant.  Sometimes contains gene function info\n"
NOSQLINJ "CREATE TABLE fbPhenotype (\n"
"    geneId varchar(255) not null,	# Flybase Gene ID\n"
"    fbAllele int not null,	# ID in fbAllele table or 0 if not allele-specific\n"
"    fbRef int not null,	# ID in fbRef table\n"
"    text longblob not null,	# Descriptive text\n"
"              #Indices\n"
"    INDEX(geneId(11))\n"
")\n");

sqlRemakeTable(conn, "fbGo", 
"#Links FlyBase gene IDs and GO IDs/aspects\n"
NOSQLINJ "CREATE TABLE fbGo (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    goId varchar(255) not null,	# GO ID\n"
"    aspect varchar(255) not null,      # P (process), F (function) or C (cellular component)"
"              #Indices\n"
"    INDEX(geneId(11)),\n"
"    INDEX(goId(10))\n"
")\n");

sqlRemakeTable(conn, "fbUniProt", 
"#Links FlyBase gene IDs and UniProt IDs/aspects\n"
NOSQLINJ "CREATE TABLE fbUniProt (\n"
"    geneId varchar(255) not null,	# FlyBase ID\n"
"    uniProtId varchar(255) not null,	# UniProt ID\n"
"              #Indices\n"
"    INDEX(geneId(11)),\n"
"    INDEX(uniProtId(6))\n"
")\n");
}

struct geneAlt
/* Gene and list of isoforms. */
    {
    struct geneAlt *next;
    char *fbName;		/* Flybase gene name. */
    char *bdgpName;		/* BDGP Gene name. */
    struct slName *isoformList;	/* List of BDGP isoforms. */
    };

void getAllSplices(char *database, FILE *f)
/* Write out table linking flybase genes with BDGP transcripts --
 * unfortunately bdgpGeneInfo lacks -R* transcript/isoform identifiers,
 * so strip those off of bdgpGene.name. 
 * This is not necessary with flyBaseGene/flyBase2004Xref where -R*'s 
 * are preserved.
*/
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[256], **row;
struct geneAlt *altList = NULL, *alt;
struct hash *bdgpHash = newHash(16);	/* Keyed by bdgp gene id. */
struct slName *n;

/* First build up list of all genes with flybase and bdgp ids. */
sqlSafef(query, sizeof(query), "select bdgpName,flyBaseId from bdgpGeneInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(alt);
    alt->bdgpName = cloneString(row[0]);
    alt->fbName = cloneString(row[1]);
    slAddHead(&altList, alt);
    hashAdd(bdgpHash, alt->bdgpName, alt);
    }
sqlFreeResult(&sr);
slReverse(&altList);

/* Now associate splicing variants. */
sqlSafef(query, sizeof(query), "select name from %s", geneTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *s = row[0];
    char *e = rStringIn("-R", s);
    int size = e ? (e - s) : strlen(s);
    char bdgpGene[16];
    if (size >= sizeof(bdgpGene))
        errAbort("'%s' too big", s);
    memcpy(bdgpGene, s, size);
    bdgpGene[size] = 0;
    alt = hashMustFindVal(bdgpHash, bdgpGene);
    n = slNameNew(s);
    slAddTail(&alt->isoformList, n);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);

for (alt = altList; alt != NULL; alt = alt->next)
    {
    for (n = alt->isoformList; n != NULL; n = n->next)
	fprintf(f, "%s\t%s\n", alt->fbName, n->name);
    }
freeHash(&bdgpHash);
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
char *tTranscript = "fbTranscript";
char *tGo = "fbGo";
char *tUniProt = "fbUniProt";
FILE *fGene = hgCreateTabFile(tabDir, tGene);
FILE *fSynonym = hgCreateTabFile(tabDir, tSynonym);
FILE *fAllele = hgCreateTabFile(tabDir, tAllele);
FILE *fRef = hgCreateTabFile(tabDir, tRef);
FILE *fRole = hgCreateTabFile(tabDir, tRole);
FILE *fPhenotype = hgCreateTabFile(tabDir, tPhenotype);
FILE *fTranscript = NULL;
FILE *fGo = hgCreateTabFile(tabDir, tGo);
FILE *fUniProt = hgCreateTabFile(tabDir, tUniProt);
struct lineFile *lf = lineFileOpen(genesFile, TRUE);
struct hash *refHash = newHash(19);
int nextRefId = 0;
int nextAlleleId = 0;
char *line, sub, type, *rest, *s;
char *geneSym = NULL, *geneName = NULL, *geneId = NULL;
int recordCount = 0;
struct slName *synList = NULL, *syn;
int curAllele = 0, curRef = 0;
struct ref *ref = NULL;
struct sqlConnection *conn;
struct hash *goUniqHash = newHash(18);

/* Make table from flybase genes to BGDP transcripts. */
if (doTranscript)
    {
    fTranscript = hgCreateTabFile(tabDir, tTranscript);
    getAllSplices(database, fTranscript);
    }

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
	s = naForNull(geneSym);
	geneSym = ungreek(s);
	freeMem(s);
	s = naForNull(geneName);
	geneName = ungreek(s);
	if (! sameString(s, "n/a"))
	    freeMem(s);
	if (geneSym != NULL && !sameString(geneSym, "n/a"))
	    slNameStore(&synList, geneSym);
	if (geneName != NULL && !sameString(geneName, "n/a"))
	    slNameStore(&synList, geneName);
	for (syn = synList; syn != NULL; syn = syn->next)
	    {
	    s = ungreek(syn->name);
	    fprintf(fSynonym, "%s\t%s\n", geneId, s);
	    freeMem(s);
	    }

	/* Write out gene record. */
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
    else if (type == 'i' && (sub == '*' || sub == '$'))
	{
	if (strlen(rest) > 2)	/* Avoid short useless ones. */
	    slNameStore(&synList, rest);
	}
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
	    slNameStore(&synList, rest);
	    }
	}
    else if (sub == '*' && type == 'm')
	{
	if (geneId == NULL)
	    errAbort("*m protein ID before geneId line %d of %s", 
	    	lf->lineIx, lf->fileName);
	if (startsWith("UniProt", rest))
	    {
	    char *ptr = strchr(rest, ':');
	    if (ptr != NULL)
		ptr++;
	    else
		errAbort("Trouble parsing UniProt ID %s like %d of %s",
			 rest, lf->lineIx, lf->fileName);
	    fprintf(fUniProt, "%s\t%s\n", geneId, ptr);
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
    else if (type == 'd' || type == 'f' || type == 'F')
	{
	FILE *f = fGo;
	char aspect = (type == 'd') ? 'P' : (type == 'f') ? 'C' : 'F';
	char *goId = rest;
	char *p = strstr(goId, " ; ");
	char assoc[128];
	if (p == NULL)
	    continue;
	else
	    goId = firstWordInLine(p + 3);
	safef(assoc, sizeof(assoc), "%s.%s", geneId, goId);
	if (hashLookup(goUniqHash, assoc) == NULL)
	    {
	    hashAddInt(goUniqHash, assoc, 1);
	    fprintf(f, "%s\t%s\t%c\n", geneId, goId, aspect);
	    }
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
    if (doTranscript)
	{
	printf("Loading %s\n", tTranscript);
	hgLoadTabFile(conn, tabDir, tTranscript, &fTranscript);
	}
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
    printf("Loading %s\n", tGo);
    hgLoadTabFile(conn, tabDir, tGo, &fGo);
    printf("Loading %s\n", tUniProt);
    hgLoadTabFile(conn, tabDir, tUniProt, &fUniProt);
    hgRemoveTabFile(tabDir, tGene);
    if (doTranscript)
	hgRemoveTabFile(tabDir, tTranscript);
    hgRemoveTabFile(tabDir, tSynonym);
    hgRemoveTabFile(tabDir, tAllele);
    hgRemoveTabFile(tabDir, tRef);
    hgRemoveTabFile(tabDir, tRole);
    hgRemoveTabFile(tabDir, tPhenotype);
    hgRemoveTabFile(tabDir, tGo);
    hgRemoveTabFile(tabDir, tUniProt);
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
geneTable = optionVal("geneTable", geneTable);
doTranscript = sameString(geneTable, "bdgpGene");
hgFlyBase(argv[1], argv[2]);
return 0;
}
