/* txGeneXref - Make kgXref type table for genes.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "spDb.h"
#include "cdsPick.h"
#include "txInfo.h"
#include "txRnaAccs.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneXref - Make kgXref type table for genes.\n"
  "usage:\n"
  "   txGeneXref genomeDb tempDb uniProtDb gene.gp gene.info genes.picks genes.ev output.xref\n"
  "where:\n"
  "   genomeDb - a browser database (ex. hg19)\n"
  "   tempDb - the database where the gene data is getting built"
  "   uniProtDb - UniProt database containing data on the relevant proteins"
  "   gene.gp - the gene prediction file, containing the gene structures"
  "   gene.info - summary information on the gene structures"
  "   gene.picks - information on the CDS region picked for the gene, if any"
  "   gene.ev - evidence on which txWalk based the gene structure"
  "   output.xref - the output file"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void removePickVersions(struct cdsPick *pick)
/* Remove version suffixes from various pick fields. */
{
chopSuffix(pick->refProt);
chopSuffix(pick->refSeq);
}

struct hash *makeGeneToProtHash(char *fileName)
/* Create hash that links gene name to protein name. 
 * Feed this in extended gene pred.*/
{
struct hash *hash = newHash(18);
char *row[11];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRowTab(lf, row))
    hashAdd(hash, row[0], cloneString(row[10]));
lineFileClose(&lf);
return hash;
}


boolean isTrna(char *sourceAcc)
/* isTrna - determine if the gene is a tRNA gene predictioon */
{
/* If the gene is a tRNA gene prediction from the Lowe lab,
 * then its name will contain the word 'tRNA' immediately after
 * a period. */
if (stringIn(".tRNA", sourceAcc))
    return(TRUE);
else
    return(FALSE);
}

boolean isRfam(char *sourceAcc)
/* isRfam - determine if the sequence comes from Rfam */
{
/* If the sequence comes from Rfam, its source accession should begin
 * with the letters RF, and then should have a string of digits
 * followed by a semicolon (and subsequent characters).  No other
 * known accessions match this pattern. */
/* Note: At this time (9/15/11), Rfam accessions include five digits after 
 * the RF.  But I wouldn't want to bet on that never changing... */
int ii;
if (strlen(sourceAcc) >= 3) 
    {
    if (sourceAcc[0] == 'R' && sourceAcc[1] == 'F' && isdigit(sourceAcc[2]))
	{
	for (ii = 3; ii < strlen(sourceAcc); ii++) 
	    {
	    if (!isdigit(sourceAcc[ii]) && sourceAcc[ii] != ';')
		{
		break;
		}
	    if (sourceAcc[ii] == ';') 
		{
		return(TRUE);
		}
	    }
	}
	    
    }
    return(FALSE);
}

void getNameDescrFromRefSeq(char *refseq, struct sqlConnection *gConn, char **geneSymbol, char **description)
/* Given a RefSeq accession, get the gene symbol and description */
{    
char query[256];
struct sqlResult *sr;
sqlSafef(query, sizeof(query), "select name,product from hgFixed.refLink where mrnaAcc='%s'", refseq);
sr = sqlGetResult(gConn, query);
char **row = sqlNextRow(sr);
if (row != NULL)
    *geneSymbol = cloneString(row[0]);
sqlFreeResult(&sr);
sqlSafef(query, sizeof(query), "select d.name from hgFixed.gbCdnaInfo g, hgFixed.description d where g.description = d.id and g.acc = '%s'", refseq);
sr = sqlGetResult(gConn, query);
row = sqlNextRow(sr);
if (row != NULL)
    *description = cloneString(row[0]);
sqlFreeResult(&sr);
}

static void getSymbolAndDescription(char *acc, struct sqlConnection *gConn, char **geneSymbol, char **description)
{
*geneSymbol = *description = NULL;

char query[1024];
sqlSafef(query, sizeof(query),
    "select geneName.name, description.name from hgFixed.gbCdnaInfo,hgFixed.geneName,hgFixed.description "
    "where hgFixed.description.id=hgFixed.gbCdnaInfo.description "
    "and hgFixed.geneName.id=hgFixed.gbCdnaInfo.geneName and hgFixed.gbCdnaInfo.acc = '%s'", acc);


struct sqlResult *sr = sqlGetResult(gConn, query);
char **row = sqlNextRow(sr);

if (row != NULL)
    {
    *geneSymbol = cloneString(row[0]);
    *description = cloneString(row[1]);
    }
sqlFreeResult(&sr);
}

void txGeneXref(char *genomeDb, char *tempDb, char *uniProtDb, 
		char *genePredFile, char *infoFile, char *pickFile, 
		char *evFile, char *outFile)
/* txGeneXref - Make kgXref type table for genes.. */
{
/* Load picks into hash.  We don't use cdsPicksLoadAll because empty fields
 * cause that autoSql-generated routine problems. */
struct hash *pickHash = newHash(18);
struct hash *geneToProtHash = makeGeneToProtHash(genePredFile);
struct cdsPick *pick;
struct lineFile *lf = lineFileOpen(pickFile, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    pick = cdsPickLoad(row);
    removePickVersions(pick);
    hashAdd(pickHash, pick->name, pick);
    }

/* Load evidence into hash */
struct hash *evHash = newHash(18);
struct txRnaAccs *ev, *evList = txRnaAccsLoadAll(evFile);
for (ev = evList; ev != NULL; ev = ev->next)
    hashAdd(evHash, ev->name, ev);

/* Open connections to our databases */
struct sqlConnection *gConn = sqlConnect(genomeDb);
struct sqlConnection *tConn = sqlConnect(tempDb);
struct sqlConnection *uConn = sqlConnect(uniProtDb);

/* Read in info file, and loop through it to make out file. */
struct txInfo *info, *infoList = txInfoLoadAll(infoFile);
FILE *f = mustOpen(outFile, "w");
for (info = infoList; info != NULL; info = info->next)
    {
    char *kgID = info->name;
    char *mRNA = "";
    char *spID = "";
    char *spDisplayID = "";
    char *geneSymbol = NULL;
    char *refseq = "";
    char *protAcc = "";
    char *description = NULL;
    char *rfamAcc = "";
    char *tRnaName = "";
    char query[256];
    char *proteinId = hashMustFindVal(geneToProtHash, info->name);
    boolean isAb = sameString(info->category, "antibodyParts");
    pick = hashFindVal(pickHash, info->name);
    ev = hashFindVal(evHash, info->name);

    /* Fill in gene symbol and description from an overlapping refseq if possible. */
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query), "select value from knownToRefSeq where name='%s'", kgID);
    sr = sqlGetResult(tConn, query);
    char **row = sqlNextRow(sr);
    if (row != NULL)
	{
	getNameDescrFromRefSeq(row[0], gConn, &geneSymbol, &description);
	}
    sqlFreeResult(&sr);

    if (pick != NULL)
       {
       /* Fill in the relatively straightforward fields. */
       refseq = pick->refSeq;
       if (info->orfSize > 0)
	    {
	    protAcc = pick->refProt;
	    spID = proteinId;
	    if (sameString(protAcc, spID))
		spID = pick->uniProt;
	    if (spID[0] != 0)
	       spDisplayID = spAnyAccToId(uConn, spID);
	    }

       if (refseq[0] != 0 && (geneSymbol == NULL || description == NULL))
           {
	   getNameDescrFromRefSeq(refseq, gConn, &geneSymbol, &description);
	   }

       /* If need be try uniProt for gene symbol and description. */
       if (spID[0] != 0 && (geneSymbol == NULL || description == NULL))
           {
	   char *acc = spLookupPrimaryAcc(uConn, spID);
	   if (description == NULL)
	       description = spDescription(uConn, acc);
	   if (geneSymbol == NULL)
	       {
	       struct slName *nameList = spGenes(uConn, acc);
	       if (nameList != NULL)
		   geneSymbol = cloneString(nameList->name);
	       slFreeList(&nameList);
	       }
	   }

       }

    /* If it's an Rfam, split out the tokens from the semicolon-delimited 
     * sourceAcc.  The first token will be the Rfam accession, the second 
     * token will be used as the gene symbol, and the description will be
     * derived from the third symbol (contig ID) and Rfam accession */
    if (isRfam(info->sourceAcc)) 
	{
        struct slName *accessionTokens = slNameListFromString(info->sourceAcc, ';');
	assert(accessionTokens != NULL);
	rfamAcc = cloneString(accessionTokens->name);
	assert(accessionTokens->next != NULL);
	char *accession = replaceChars(accessionTokens->next->name, "-", "_");
	geneSymbol = replaceChars(accession, "Alias=", "");
	freeMem(accession);
	if (isalpha(*geneSymbol))
	    *geneSymbol = toupper(*geneSymbol);
	assert(accessionTokens->next->next != NULL);
	char *contigCoordinates = replaceChars(accessionTokens->next->next->name,
					       "Note=", "");
	struct dyString *dyTmp = dyStringCreate("Rfam model %s hit found at contig region %s", 
						rfamAcc, contigCoordinates);
	description = dyStringCannibalize(&dyTmp);
	freeMem(contigCoordinates);
	slFreeList(&accessionTokens);
        }
    /* If it's a tRNA from the tRNA track, sourceAcc will have the following 
     * form: chr<C>.tRNA<N>-<AA><AC> where C identifies the chromosome, N 
     * identifies the tRNA gene on the chromosome (so C and N together identify
     * the tRNA track item), AA identifies the amino acid (or is Pseudo in the 
     * case of a predicted pseudogene), and AC is the anticodon.  Use "TRNA_<AC>"
     * as the gene symbol and a gene description of "transfer RNA <AA> (anticodon <AC>),
     * and put the sourceAcc in the tRnaName field */
    else if (isTrna(info->sourceAcc))
	{
	tRnaName = cloneString(info->sourceAcc);
	description = (char *) needMem(50);
	if (stringIn("Pseudo", tRnaName)) 
	    {
	    char antiCodon[4];
	    antiCodon[3] = 0;
	    (void) strncpy(antiCodon, strchr(tRnaName, '-') + strlen("Pseudo") + 1,3);
	    geneSymbol = cloneString("TRNA_Pseudo");
	    sprintf(description, "transfer RNA pseudogene (anticodon %s)",
		    &antiCodon[0]);
	    }
	else 
	    {
	    char aminoAcid[4], antiCodon[4];
	    (void) strncpy(antiCodon, strchr(tRnaName, '-') + 4, 3);
	    (void) strncpy(aminoAcid, strchr(tRnaName, '-') + 1, 3);
	    aminoAcid[3] = 0;
	    antiCodon[3] = 0;
	    geneSymbol = catTwoStrings("TRNA_", aminoAcid);
	    sprintf(description, "transfer RNA %s (anticodon %s)", 
		    &aminoAcid[0], &antiCodon[0]);
	    }
	}
    else 
	{
	/* If it's an antibody fragment use that as name. */
	if (isAb)
	    {
	    geneSymbol = cloneString("abParts");
	    description = cloneString("Parts of antibodies, mostly variable regions.");
	    isAb = TRUE;
	    }
	
	if (ev == NULL)
	    {
	    mRNA = cloneString("");
	    if (!isAb)
		{
		errAbort("%s is %s but not %s\n", info->name, infoFile, evFile);
		}
	    }
	else
	    {
	    mRNA = cloneString(ev->primary);
	    chopSuffix(mRNA);
	    }
	}

    if (geneSymbol == NULL || description == NULL)
	getSymbolAndDescription(mRNA, gConn, &geneSymbol, &description);

    if (geneSymbol == NULL || description == NULL)
	{
	int i;
	for (i=0; (geneSymbol == NULL) && (i < ev->accCount); ++i)
	    {
	    char *acc = ev->accs[i];
	    chopSuffix(acc);
	    getSymbolAndDescription(acc, gConn, &geneSymbol, &description);
	    }
        }

    if ((geneSymbol == NULL) || sameString(geneSymbol, "n/a") )
        geneSymbol = mRNA;
    if (description == NULL)
        description = mRNA;

    /* Get rid of some characters that will cause havoc downstream. */
    stripChar(geneSymbol, '\'');
    subChar(geneSymbol, '<', '[');
    subChar(geneSymbol, '>', ']');

    /* Abbreviate geneSymbol if too long */
    if (strlen(geneSymbol) > 40)
        strcpy(geneSymbol+37, "...");

    fprintf(f, "%s\t", kgID);
    fprintf(f, "%s\t", mRNA);
    fprintf(f, "%s\t", spID);
    fprintf(f, "%s\t", spDisplayID);
    fprintf(f, "%s\t", geneSymbol);
    fprintf(f, "%s\t", refseq);
    fprintf(f, "%s\t", protAcc);
    fprintf(f, "%s\t", description);
    fprintf(f, "%s\t", rfamAcc);
    fprintf(f, "%s\n", tRnaName);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 9)
    usage();
txGeneXref(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], 
	   argv[7], argv[8]);
return 0;
}
