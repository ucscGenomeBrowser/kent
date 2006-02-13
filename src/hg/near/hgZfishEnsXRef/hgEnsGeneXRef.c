/* hgZfishEnsXRef - Create table that links Ensembl Transcript ID to IDs */
/* */
/* */
/* */
/* */
/* */
/* */
/* */
/* from other databases and a gene description. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "ensGeneXRef.h"
#include "sqlNum.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgZfishEnsXRef - Create table that links zebrafish Ensembl Transcript ID\n"
  "to ZFIN ID, UniProt ID, Gene Symbol, RefSeq accession, RefSeq protein\n" 
  "accession and a gene description.\n"
  "Input is from two files of database dumps from Ensembl's BioMart.\n"
  "Database is required to query refLink table for the Gene Symbol.\n"
  "usage:\n"
  "   hgZfishXRef file1 file2 gene_info gene2refseq output.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0}
};

struct ncbiGene /* Entrez Gene information */
{
char *geneId;      /* Entrez Gene ID */
char *geneSymbol;  /* Entrez Gene Symbol */
char *description; /* Gene Description */
char *mrnaRefSeq;  /* mRNA RefSeq accession */
char *protRefSeq;  /* protein RefSeq accession */
};

struct hashEl *addHashElUnique(struct hash *hash, char *name, void *val)
/* Adds new element to hash table. If not unique, remove old element */
/* before adding new one. Avoids replicate entries in the table */
{
if (hashLookup(hash, name) != NULL)
    /* if item in hash already, remove first */
    hashRemove(hash, name);
/* then add element to hash table */
return hashAdd(hash, name, val);
}

void createDescription(char *interProId, char *interProDesc, char **desc)
/* Creates a gene description using the InterPro domain. */
{
char descPrefix[32] = "Ensembl Predicted Gene:";
char newDesc[256], *descPtr, *fullDesc;
struct dyString *dy = dyStringNew(256);

descPtr = *desc;
/* if there is no description already, add the prefix */
if (descPtr == NULL)
    dyStringPrintf(dy, "%s", descPrefix);
else 
    dyStringPrintf(dy, "%s", descPtr);
/* Append the InterPro description and ID. */
if (!sameString(interProId, ""))
    {
    safef(newDesc, sizeof(newDesc), " %s (InterPro ID: %s),", interProDesc, interProId);
    dyStringAppend(dy, newDesc);
    }
*desc = cloneString(dy->string);
return;
}

void getGeneSymbol(struct ensGeneXRef **xRef, struct hash *idHash, struct hash *mrnaHash)
/* Retrieves Gene Symbol from the refLink table file for a given Entrez ID */
{
struct ensGeneXRef *gXRef = *xRef;
struct ncbiGene *gene;
char *geneSymbol = NULL;

/* check if this exists in the hash already */
if ((gXRef->geneId != NULL) && ((gene = hashFindVal(idHash, gXRef->geneId)) != NULL))
    {
    gXRef->geneSymbol = cloneString(gene->geneSymbol);
    }
/* if the Gene ID is NULL or it can not be found in the hash and also */
/* the refSeqAcc is not NULL then check the hash keyed by RefSeq accessions. */
else if ((gXRef->refSeq != NULL) && ((gene = hashFindVal(mrnaHash, gXRef->refSeq)) != NULL))
    {
    /* find entry by accession and get Gene Symbol and Gene ID */
    gXRef->geneSymbol = cloneString(gene->geneSymbol);
    gXRef->geneId = cloneString(gene->geneId);
    }
else
    fprintf(stderr, "There is no Gene Symbol for Entrez Gene ID, %s, or for mRNA RefSeq: %s in the Entrez Gene files.\n", gXRef->geneId, gXRef->refSeq);
}

struct hash *readIdAndDescFile(char *fileName)
/* Reads a file with IDs and description for Ensembl transcripts. Stores */
/* this information in a hash which is returned. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[5], *tId, *desc, *uniProtId, *zfinId;
char *interProId = NULL, *interProDesc = NULL, *xRefDesc = NULL;
struct ensGeneXRef *xRef = NULL;
struct hash *hash = newHash(16);
while (lineFileChopNextTab(lf, words, 6))
    {
    /* skip over header line */
    if (!startsWith("Ensembl", words[0]))
        {
        fflush(stdout);
        tId = cloneString(words[0]);
        desc = cloneString(words[1]);
        uniProtId = cloneString(words[2]);
        zfinId = cloneString(words[3]);
        interProId = cloneString(words[4]);
        interProDesc = cloneString(words[5]);

        /* check if this exists in the hash already */
        if ((xRef = hashFindVal(hash, tId)) == NULL)
            {
            /* allocate memory for xRef structure */
            AllocVar(xRef);
            /* add database IDs and description to the xRef structure */
            xRef->next = NULL;
            xRef->ensGeneId = cloneString(tId);
            xRef->zfinId = cloneString(zfinId);
            xRef->uniProtId = cloneString(uniProtId);
            xRef->geneId = NULL;
            xRef->geneSymbol = NULL;
            xRef->refSeq = NULL;
            xRef->protAcc = NULL;
            xRefDesc = NULL;
            }
        else  
            { /* entry in hash exists for this transcript */
            xRefDesc = xRef->description;
            }
        /* If there is no description field, then one must be created */
        /* using the InterPro ID and InterPro domain description. */
        /* Additional InterPro descriptions are appended to existing ones */

        if (sameString(desc, ""))
            {
            desc = cloneString(xRefDesc);
            createDescription(interProId, interProDesc, &desc);
            }
        xRef->description = cloneString(desc);
        /* Add structure to hash keyed by the transcript ID */
        /* and delete struct if present already before adding new version. */
        addHashElUnique(hash, cloneString(xRef->ensGeneId), xRef); 
        }
    }
lineFileClose(&lf);
return hash;
}

void readEntrezGeneFiles(char *geneFile, char *refSeqFile, 
       struct hash **hash1, struct hash **hash2)
/* Read in Entrez Gene files and store contents. */
{
struct lineFile *lf = lineFileOpen(geneFile, TRUE);
struct lineFile *rf = lineFileOpen(refSeqFile, TRUE);
char *words[10], *geneId, *symbol, *mrnaAcc, *protAcc, *desc;
struct ncbiGene *g, *g2;
struct hash *idHash = *hash1;
struct hash *refSeqHash = *hash2;

while (lineFileChopNextTab(lf, words, 9))
    {
    geneId = cloneString(words[1]);
    symbol = cloneString(words[2]);
    desc = cloneString(words[8]);
    /* key is locusLink ID - must be a string */
    /* check if this exists in the hash already */
    if ((g = hashFindVal(idHash, geneId)) == NULL)
        {
        /* allocate memory for xRef structure */
        AllocVar(g);
        AllocVar(g2);
        g->geneId = cloneString(geneId);
        g->geneSymbol = cloneString(symbol);
        g->description = cloneString(desc);
        g->mrnaRefSeq = NULL;
        g->protRefSeq = NULL;
        addHashElUnique(idHash, geneId, g); 
        }
    else  
        /* entry in hash exists for this transcript */
        fprintf(stderr, "There is already an entry for this gene ID, %s.\n", geneId);
    }
lineFileClose(&lf);
geneId = NULL;
while (lineFileChopNextTab(rf, words, 6))
    {
    geneId = cloneString(words[1]);
    chopSuffix(words[3]);
    mrnaAcc = cloneString(words[3]);
    protAcc = cloneString(words[5]);
    /* key is locusLink ID - must be a string */
    /* check if this exists in the hash already */
    if ((g = hashFindVal(idHash, geneId)) != NULL)
        {
        AllocVar(g2);
        g->mrnaRefSeq = cloneString(mrnaAcc);
        g->protRefSeq = cloneString(protAcc); 
        g2->geneId = cloneString(g->geneId);
        g2->geneSymbol = cloneString(g->geneSymbol);
        g2->description = cloneString(g->description);
        g2->mrnaRefSeq = cloneString(mrnaAcc);
        g2->protRefSeq = cloneString(protAcc); 
        /* add back to hash keyed by Gene IDs and add also to a hash */
        /* keyed by RefSeq accession. */
        addHashElUnique(idHash, geneId, g); 
        addHashElUnique(refSeqHash, mrnaAcc, g2); 
        }
    else
        fprintf(stderr, "This locusLinkID, %s, has not been stored already\n", geneId);
    }
lineFileClose(&rf);
}

void readNcbiFile(char *ncbiFile, struct hash **hash, struct hash *geneByIdHash, struct hash *geneByRefSeqHash)
/* Read file with Entrez Gene ID, and RefSeq nucleotide and peptide */
/* accessions and add to the appropriate value in the hash. */
{
struct lineFile *lf = lineFileOpen(ncbiFile, TRUE);
char *words[4], *tId, *refSeqId, *refSeqPepId, *geneSymbol, *lLinkId;
unsigned geneId;
struct hash *geneHash = *hash;
struct ensGeneXRef *xRef;

while (lineFileChopNextTab(lf, words, 5))
    {
    /* skip over header line */
    if (!startsWith("Ensembl", words[0]))
        {
        fflush(stdout);
        tId = cloneString(words[0]);
        if (!sameString(words[1], ""))
            geneId = sqlUnsigned(words[1]);
        else
            geneId = 0;
        refSeqId = cloneString(words[2]);
        refSeqPepId = cloneString(words[3]);

        /* retrieve the appropriate record from the hash using transcript ID */
        if ((xRef = hashFindVal(geneHash, tId)) != NULL)
            {
            xRef->refSeq = cloneString(refSeqId);
            xRef->protAcc = cloneString(refSeqPepId);
            /* retrieve Gene Symbol from Entrez Gene files */
            if (geneId == 0)
                lLinkId = NULL;
            else
                {
                lLinkId = cloneString(words[1]);
                xRef->geneId = cloneString(lLinkId);
                }
            getGeneSymbol(&xRef, geneByIdHash, geneByRefSeqHash);
            addHashElUnique(geneHash, tId, xRef);
            }
        else
            fprintf(stderr, "Found transcript ID, %s, in ncbiFile but it was not stored from the description file.\n", tId);
        }
    }
lineFileClose(&lf);
return;
}

void printXRefTab(FILE *out, struct hash *hash)
/* Print the contents of the hash as tabbed output to output file. */
{
struct hashEl *geneHashList = NULL, *geneEl = NULL;
struct ensGeneXRef *x;

/* get contents of hash as a linked list */
geneHashList = hashElListHash(hash);
if (geneHashList != NULL)
    {
    /* walk through list of hash elements */
    for (geneEl = geneHashList; geneEl != NULL; geneEl = geneEl->next)
        {
        x = (struct ensGeneXRef *)geneEl->val;
        /* Print out cross-reference information in a tabbed file */
        fprintf(out, "%s", x->ensGeneId);
        if (x->zfinId == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->zfinId);
        if (x->uniProtId == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->uniProtId);
        if (x->geneId == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->geneId);
        if (x->geneSymbol == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->geneSymbol);
        if (x->refSeq == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->refSeq);
        if (x->protAcc == NULL)
            fprintf(out,"\t");
        else
            fprintf(out, "\t%s", x->protAcc);
        fprintf(out, "\t%s\n", x->description);
        fflush(out);
        }
    }
else
    {
    errAbort("Unable to retrieve the stored gene information\n");
    }
hashElFreeList(&geneHashList);
}

void hgZfishEnsXRef(char *idAndDescFile, char *ncbiFile, char *geneFile, char *refSeqFile, char *outFile)
/* Parse out the correct columns from the file to simple column format. */
{
struct hash *geneInfoHash = readIdAndDescFile(idAndDescFile);
struct hash *geneByIdHash = newHash(16);
struct hash *geneByRefSeqHash = newHash(16);
readEntrezGeneFiles(geneFile, refSeqFile, &geneByIdHash, &geneByRefSeqHash);
FILE *f = mustOpen(outFile, "w");
readNcbiFile(ncbiFile, &geneInfoHash, geneByIdHash, geneByRefSeqHash);
printXRefTab(f, geneInfoHash);
freeHashAndVals(&geneInfoHash);
freeHashAndVals(&geneByIdHash);
freeHashAndVals(&geneByRefSeqHash);
carefulClose(&f);
return; 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
hgZfishEnsXRef(argv[1],argv[2], argv[3], argv[4], argv[5]);
return 0;
}
