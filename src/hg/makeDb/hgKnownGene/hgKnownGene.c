/* hgKnownGene - Load auxiliarry tables on Genie known genes. */
#include "common.h"
#include "obscure.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "hgRelate.h"
#include "jksql.h"
#include "genePred.h"
#include "gff.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKnownGene - Load auxiliarry tables on Genie known genes\n"
  "usage:\n"
  "   hgKnownGene database known.gtf\n");
}

struct hash *loadNameValTable(struct sqlConnection *conn, char *tableName, int hashSize)
/* Create a hash and load it up from table. */
{
char query[128];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(hashSize);

sprintf(query, "select id,name from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[1], intToPt(sqlUnsigned(row[0])));
    }
sqlFreeResult(&sr);
return hash;
}

static void readQuotedString(struct lineFile *lf, char *in, char *out, char **retNext)
/* Parse quoted string and abort on error. */
{
if (!parseQuotedString(in, out, retNext))
    errAbort("Line %d of %s\n", lf->lineIx, lf->fileName);
}

struct knownGene
/* Info on a known gene. */
    {
    struct knownGene *next;	/* Next in list. */
    char *name;			/* Name - one of below three. */
    char *transId;		/* Genie transcript ID. */
    char *geneName;		/* Gene name. */
    char *productName;		/* Product name. */
    char *proteinId;		/* Id of associated protein. */
    char *ngi;			/* Nucleotide GI. */
    char *pgi;			/* Protein GI. */
    };

#ifdef SOMETIMES
void fixupKnownNames(struct sqlConnection *conn, struct hash *kgHash)
/* Update gene names in genieKnown and knownGene tables. */
{
struct knownGene *kg;
struct sqlResult *sr;
char **row;
char query[256];
struct genePred *gpList = NULL, *gp;
int ix = 0, count;
char *table = "genieKnown";

sprintf(query, "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
slReverse(&gpList);
count = slCount(gpList);
uglyf("Got %d known genes in %s\n", count, table);

for (gp = gpList; gp != NULL; gp = gp->next)
    {
    if (++ix % 100 == 0)
        printf("Updating %d of %d (%s)\n", ix, count, gp->name);
    kg = hashFindVal(kgHash, gp->name);
    if (kg != NULL)
        {
	sprintf(query, "UPDATE %s SET name='%s' WHERE name='%s'",
		table, kg->name, gp->name);
	sqlUpdate(conn, query);
	}
    else
        {
	warn("Couldn't find %s\n", gp->name);
	}
    }
genePredFreeList(&gpList);
}
#endif

boolean okName(char *s)
/* Return TRUE if s is non-null and non-'na' */
{
if (s == NULL)
    return FALSE;
if (sameString(s, "n/a"))
    return FALSE;
if (strchr(s, '\''))
    return FALSE;
if (strchr(s, ' '))
    return FALSE;
return TRUE;
}

void fillInName(struct knownGene *kg)
/* Fill in name field from first of geneName, productName, or transId that
 * actually exists. */
{
if (okName(kg->geneName))
    kg->name = kg->geneName;
else if (okName(kg->productName))
    kg->name = kg->productName;
else if (okName(kg->transId))
    kg->name = kg->transId;
else
    {
    warn("Couldn't fill in known gene %s %s %s", kg->geneName, kg->productName, kg->transId);
    assert(FALSE);
    }
}

char *skipPastColon(char *s)
/* Return first character after colon if any, else
 * just start of string. */
{
char *e = strchr(s, ':');
if (e != NULL)
    return e+1;
else
    return s;
}

void hgKnownGeneInDb(char *fileName)
/* hgKnownGene - Load auxiliarry tables on Genie known genes. */
{
struct sqlConnection *conn = hgStartUpdate();
struct hash *productHash = loadNameValTable(conn, "productName", 16);
struct hash *geneHash = loadNameValTable(conn, "geneName", 16);
struct hash *kgHash = newHash(0);
FILE *productTab = hgCreateTabFile("productName");
FILE *geneTab = hgCreateTabFile("geneName");
FILE *knownInfoTab = hgCreateTabFile("knownInfo");
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *firstWords[16], *line;
int wordCount, lineSize;
char *type, *val;
struct hashEl *hel;
struct knownGene *kgList = NULL, *kg;

printf("Processing %s\n", fileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopTabs(line, firstWords);
    if (wordCount < 9)
        continue;
    if (sameWord("prim_trans", firstWords[2]))
        {
	char *s = firstWords[8];
	char *transId = NULL, *geneId = "n/a", *productName = "n/a", 
	    *geneName = "n/a", *pgi = "n/a", *ngi = "n/a", 
	    *proteinId = "n/a";
	   
	for (;;)
	   {
	   if ((type = nextWord(&s)) == NULL)
	       break;
	   s = skipLeadingSpaces(s);
	   if (s[0] == 0)
	       errAbort("Unpaired type/val on end of gtf line %d of %s", lf->lineIx, lf->fileName);
	   if (s[0] == '"' || s[0] == '\'')
	       {
	       val = s;
	       readQuotedString(lf, s, val, &s);
	       subChar(val, '\t', ' ');
	       }
	   else
	       val = nextWord(&s);
	   if (s != NULL)
	      {
	      s = strchr(s, ';');
	      if (s != NULL)
		 ++s;
	      }
	   if (sameString("gene_id", type))
	       geneId = val;
	   else if (sameString("pgi", type) || sameString("exp_pgi", type))
	       pgi = skipPastColon(val);
	   else if (sameString("gene_name", type))
	       geneName = val;
	   else if (sameString("product", type))
	       productName = val;
	   else if (sameString("transcript_id", type))
	       transId = val;
	   else if (sameString("ngi", type) || sameString("exp_ngi", type))
	       ngi = skipPastColon(val);
	   else if (sameString("protein_id", type) || sameString("exp_protein_acc", type))
	       proteinId = skipPastColon(val);
	   }
	if (transId != NULL)
	    {
	    HGID  productNameId, geneNameId;
	    if ((hel = hashLookup(geneHash, geneName)) != NULL)
	        geneNameId = ptToInt(hel->val);
	    else
	        {
		geneNameId = hgNextId();
		fprintf(geneTab, "%u\t%s\n", geneNameId, geneName);
		hashAdd(geneHash, geneName, intToPt(geneNameId));
		}
	    if ((hel = hashLookup(productHash, productName)) != NULL)
	        productNameId = ptToInt(hel->val);
	    else
	        {
		productNameId = hgNextId();
		fprintf(productTab, "%u\t%s\n", productNameId, productName);
		hashAdd(productHash, productName, intToPt(productNameId));
		}
	    AllocVar(kg);
	    kg->transId = cloneString(transId);
	    kg->geneName = cloneString(geneName);
	    kg->productName = cloneString(productName);
	    kg->ngi = cloneString(ngi);
	    kg->pgi = cloneString(pgi);
	    fillInName(kg);
	    slAddHead(&kgList, kg);
	    hashAddUnique(kgHash, kg->transId, kg);
	    fprintf(knownInfoTab, "%s\t%s\t%s\t%u\t%u\t%s\t%s\t%s\n",
	        kg->name, transId, geneId, geneNameId, productNameId, proteinId, ngi, pgi);	
	    }
	}
    }
slReverse(&kgList);
printf("Got %d known genes\n", slCount(kgList));
fclose(productTab);
fclose(geneTab);
fclose(knownInfoTab);
hgLoadTabFile(conn, "productName");
hgLoadTabFile(conn, "geneName");
hgLoadTabFile(conn, "knownInfo");
// fixupKnownNames(conn, kgHash);
hgEndUpdate(&conn, "Loaded known gene info from %s", fileName);
}

void hgKnownGene(char *database, char *fileName)
/* hgKnownGene - Load auxiliarry tables on Genie known genes. */
{
hgSetDb(database);
hgKnownGeneInDb(fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgKnownGene(argv[1], argv[2]);
return 0;
}
