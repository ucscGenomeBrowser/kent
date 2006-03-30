/*
  File: zfishBacClonesandSts.c
  Author: Rachel Harte
  Date: 5/24/2005
  Description: Using a the list of names of zebrafish BAC clones that are in 
  the database tables for the BAC ends track, add information including 
  internal Sanger name, alternate name (alias), UniSTS ID, and accession. 
  FPC contig is not used for the tables as these are dynamic as the assembly 
  changes. The program assumes that all BAC clones in the markers file are 
  also in the clonemarkers file. There are more Sanger STS marker names in 
  the clonemarkers file than appear in the markers file. There are also 
  Sanger STS names that map to multiple BAC clones. In this case, 
  etID9511.14 has 35 BAC clones associated to it so there are 35 different 
  external and internal BAC name pairs that are associated with this 
  Sanger STS name. Each Sanger STS name has multiple aliases - the most is
  216 (etID22623.3) and all but 4 have under 50.
  Each Sanger STS name may have more than one UniSTS ID, the maximum in 
  this dataset is three. These are printed out as a comma separated list 
  in the bacXRef.tab file.
  Output:
  bacAlias.tab: aliases (STS aliases for STS associated with BAC clones) 
  and Sanger STS name.
  bacXRef.tab: BAC clone external name, BAC clone internal (Sanger name), 
  chromosomes to which BAC clone is mapped (in pairs or singles tables), 
  Genbank accession for BAC clone, Sanger STS name, relationship (method of
  finding STS), uniSTS ID(s) and primers for STS. 
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "errabort.h"
#include "hash.h"
#include "jksql.h"
#include "fuzzyFind.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "psl.h"
#include "fa.h"
#include "psl.h"
#include "options.h"

#define NUMALIASES 250
#define NUMCHROMS 10
#define NUMSANGER 5
#define NUMPREFIXES 9
#define MAXSANGER 50

static char const rcsid[] = "$Id: zfishBacClonesandSts.c,v 1.12 2006/03/30 06:14:42 hartera Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

struct bac /* BAC clone information */
{
char *extName; /* external name for BAC clone */
char *intName; /* internal Sanger sequencing name for BAC clone */
char **chrom; /* chroms to which BAC end singles of pairs for BAC clone are mapped by BLAT */
char *acc; /* Genbank accession for BAC clone */
};

struct alias  /* STS aliases associated with BAC clone */
{
char *sangerName; /* Sanger STS name */
char **extName; /* external name for BAC clone */
char **intName; /* internal Sanger sequencing name for BAC clone */
int *relation;  /* relationship between BAC and STS - method of STS design */
char **uniStsId;  /* UniSTS IDs for these STS aliases */
char **aliases; /* STS aliases */
char *primer1;  /* STS 5' primer */
char *primer2;  /* STS 3' primer */
};

struct sanger
{
char *sangerName[MAXSANGER]; /* list of Sanger STS names */
};

struct hash *bacHash;
struct hash *extNameHash;
struct hash *aliasHash; 
struct hash *sangerByExtNameHash;

char *intNamePrefix[] = {"zC", "ZC", "zK", "zKp", "bZ", "dZ", "bY", "CHORI73_", "bP"};
char *extNamePrefix[] = {"CH211-", "CH211-", "DKEY-", "DKEYP-", "RP71-", "BUSM1-", "XX-", "CH73-", "CT7-"};

char *translateName(char *name, boolean toInt)
/* translates external name to internal name format or vice versa */
{
char *transName = NULL;
boolean prefixFound = FALSE;
int i;
        
for (i = 0; i < NUMPREFIXES && (!prefixFound); i++)
    {
    if (toInt)
        {
        if (startsWith(extNamePrefix[i], name))
            {
            transName = replaceChars(name, extNamePrefix[i], intNamePrefix[i]);
            prefixFound = TRUE;          
            }
        }
    else
        {
        if (startsWith(intNamePrefix[i], name))
            {
            transName = replaceChars(name, intNamePrefix[i], extNamePrefix[i]);
            prefixFound = TRUE;          
            }
        }
    }
if (prefixFound)
    return transName;
else
    return NULL;
}

boolean stringIsAnInteger(char *name)
/* checks whether a string is an integer */
{
boolean isNumeric = TRUE;
int i;

for (i = 0; name[i] != '\0'; i++)
    {
    if (!isdigit(name[i]))
        isNumeric = FALSE;
    } 
return isNumeric;
}

void readContigs(struct lineFile *cgf)
{
struct bac *b = NULL;
char *words[4], *name = NULL, *extName = NULL;
char sep = '|';
int i;

/* BAC structs keyed by external name */
bacHash = newHash(16);
/* external names keyed by internal names */
extNameHash = newHash(16);

while (lineFileChopCharNext(cgf, sep, words, 5))
    {
    name = cloneString(words[1]);
    extName = cloneString(words[2]);
    if ((b = hashFindVal(bacHash, extName)) == NULL)
        {
        /* allocate memory for bac struct */
        AllocVar(b);
        /* add BAC info to struct */
        b->intName = cloneString(name);
        b->extName = cloneString(extName);
        AllocArray(b->chrom, (sizeof(char *) * NUMCHROMS));
        for (i = 0; i < NUMCHROMS; i++)
            {
            b->chrom[i] = NULL;
            }
        b->acc = NULL;
        hashAdd(bacHash, extName, b);
        hashAdd(extNameHash, name, extName);
        }
    else
        fprintf(stderr, "The BAC clone %s is assigned to more than one contig\n", extName);
    }
}

void readCloneNames(struct lineFile *clf)
/* read internal BAC clone names and Sanger sts names */
{
struct alias *a = NULL;
struct sanger *s = NULL;
char *words[4], *name = NULL, *sanger = NULL, *extName = NULL;
int i, rel;
char sep = '|';
boolean found = FALSE, posFound = FALSE;

/* alias hash is keyed by Sanger sts name */
aliasHash = newHash(16);
/* hash of Sanger names keyed by external name */
sangerByExtNameHash = newHash(16);

/* Read in all rows */
while (lineFileChopCharNext(clf, sep, words, 5))
    {
    name = cloneString(words[0]);
    sanger = cloneString(words[1]);
    if (!sameString(words[2], ""))
        rel = sqlUnsigned(words[2]);
    else
        rel = 3;
    /* find external name for this internal name from the extNameHash */
    if ((extName = hashFindVal(extNameHash, name)) == NULL)
        {
        /* if not found in BAC hash, then need to use internal name to make extName */
        extName = translateName(name, FALSE);
        }
    if ((a = hashFindVal(aliasHash, sanger)) == NULL)
        {
        /* allocate memory for alias struct */
        AllocVar(a); 
        /* allocate memory for UniSTS IDs, aliases, internal and external names and relations */
        /* and initialize the arrays */
        AllocArray(a->uniStsId, (sizeof(char *) * NUMSANGER));
        AllocArray(a->aliases, (sizeof(char *) * NUMALIASES));
        AllocArray(a->extName, (sizeof(char *) * MAXSANGER));
        AllocArray(a->intName, (sizeof(char *) * MAXSANGER));
        AllocArray(a->relation, (sizeof(int) * MAXSANGER));

        for (i = 0; i < NUMSANGER; i++)
            {
            a->uniStsId[i] = NULL;
            }
        for (i = 0; i < MAXSANGER; i++)
            {
            a->extName[i] = NULL;
            a->intName[i] = NULL;
            a->relation[i] = -1;
            }
        for (i = 0; i < NUMALIASES; i++)
            {
            a->aliases[i] = NULL;
            }
        }
    /* find empty slot in arrays to add external and internal names */
    posFound = FALSE;
    for (i = 0; i < NUMALIASES && (!posFound); i++)
        {
        if (a->extName[i] == NULL)
            {
            posFound = TRUE;
            a->extName[i] = cloneString(extName);
            if (a->intName[i] == NULL)
                a->intName[i] = cloneString(name);
            else
                errAbort("For marker %s, the empty slot in the intName array is not the same as that for the extName array in the alias struct\n", extName);
            if (a->relation[i] == -1)
                a->relation[i] = rel;
            else 
                errAbort("For marker %s, the empty slot in the relation array is not the same as that for the extName array in the alias struct\n", extName);
            }
        }
   
    a->sangerName = cloneString(sanger);
    a->primer1 = NULL;
    a->primer2 = NULL;
    /* add this alias struct to the hash keyed by sanger name */
    hashAdd(aliasHash, sanger, a);
    /* add sanger name to hash keyed by external name */
    if ((s = hashFindVal(sangerByExtNameHash, extName)) == NULL)
        {
        /* allocate memory for struct with array of Sanger names */
        AllocVar(s);
        /* initialize the array */
        for (i = 0; i < MAXSANGER; i++)
            {
            s->sangerName[i] = NULL;
            }
        }
    found = FALSE;
    for (i = 0; i < MAXSANGER && (!found); i++)
        {
        if (s->sangerName[i] == NULL)
            {
            found = TRUE;
            s->sangerName[i] = cloneString(sanger);
            }
        }
  /* add this list of sanger names to a hash keyed by external name, extName */
    hashAdd(sangerByExtNameHash, extName, s);
    }
}

void readMarkers(struct lineFile *mkf)
/* Read in Sanger sts name, UniSTS ID and aliases */
/* All Sanger names in this file are found in the Clone marker file */
{
struct bac *b = NULL;
struct alias *a = NULL;
char *words[6], *sanger[NUMSANGER], *stsIdandAliases[NUMALIASES], *extName = NULL;
char *firstAlias = NULL, **aliases = NULL, *pr1 = NULL, *pr2 = NULL;
int sangerCount = 0, nameCount = 0, i, j, k;
char sep = '|';
boolean isId = TRUE;

/* Read in all rows */
while (lineFileChopCharNext(mkf, sep, words, 6))
    {
    sangerCount = chopByChar(words[1], ';', sanger, ArraySize(sanger));
    nameCount = chopByChar(words[2], ';', stsIdandAliases, ArraySize(stsIdandAliases));
    pr1 = cloneString(words[3]);
    pr2 = cloneString(words[4]);

    /* process each sanger name found */
    for (i = 0; i < sangerCount; i++)
        {
        /* use sanger name to find alias struct in hash */
        if ((a = hashFindVal(aliasHash, sanger[i])) != NULL)
            {
            /* if string is numeric, then it is an integer ID so do not add to array */
            k = 0;
            for (j = 0; j < nameCount; j++)
                {
                isId = stringIsAnInteger(stsIdandAliases[j]);
                if (!isId)
                    {
                    a->aliases[k] = cloneString(stsIdandAliases[j]);
                    k++;
                    }
                }
                
            /* store primer sequences */
            a->primer1 = cloneString(pr1);
            a->primer2 = cloneString(pr2);
            }
        else
            fprintf(stderr, "Can not find sanger name, %s, in aliasHash\n", sanger[i]);
        }
    }
}

void readBacNames(struct lineFile *nmf)
/* Read BAC external names from file - these are BAC clones that aligned */
/* and are in the database. Also read in the chromosome(s) to which they aligned */
{
struct bac *b = NULL;
char *words[52], *extName = NULL, *chrom = NULL, *intName = NULL;
int i;
boolean found = FALSE;

/* Read in all rows */
/* list of BAC clones from database may include names not found in other files */
while (lineFileChopTab(nmf, words))
    {
    extName = cloneString(words[0]);
    chrom = cloneString(words[1]);
    if ((b = hashFindVal(bacHash, extName)) == NULL)
        {
        /* allocate memory for BAC clone struct */
        AllocVar(b);
        b->extName = cloneString(extName);
        intName = translateName(extName, TRUE);
        if (intName != NULL)
            b->intName = cloneString(intName);
        else
            b->intName = NULL;
        AllocArray(b->chrom, (sizeof(char *) * NUMCHROMS));
        for (i = 0; i < NUMCHROMS; i++)
            {
            b->chrom[i] = NULL;
            }
        b->acc = NULL;
        /* add this new entry to the extNameHash, added to bacHash later */
        if (intName != NULL)
            hashAdd(extNameHash, intName, extName);
        }
    found = FALSE;
    for (i = 0; i < NUMCHROMS && (!found); i++)
        {
        if (b->chrom[i] == NULL)
            {
            found = TRUE;
            b->chrom[i] = cloneString(chrom);
            }
        }

    if (!found)
        errAbort("No room left in chrom array to add chrom for %s\n", extName);
    /* add BAC info to hash keyed by external name */
    hashAdd(bacHash, extName, b);
    }
}

void readAccessions(struct lineFile *acf)
/* Read internal names and accessions: one marker has one accession (if any) and vice versa */
{
struct bac *b = NULL;
char *words[52], *name = NULL, *acc = NULL, *extName = NULL;
int i;

/* Read in all rows */
while (lineFileChopTab(acf, words))
    {
    name = cloneString(words[0]);
    acc = cloneString(words[1]);
    if ((extName = hashFindVal(extNameHash, name)) != NULL)
        {
        if ((b = hashFindVal(bacHash, extName)) != NULL)
            b->acc = cloneString(acc);
        else
            errAbort("External name %s found for internal name %s but no BAC clone entry in the bacHash\n", extName, name);
        }
    }
}

void readUniStsIds(struct lineFile *usf)
/* Read names and UniSTS IDs */
{
char *words[52], *sanger[NUMSANGER], *uniStsId = NULL;
struct alias *al = NULL;
int i = 0, sangerCount, j;
boolean found = FALSE;

/* Read in all rows */
while (lineFileChopTab(usf, words))
    {
    /* the Sanger ID may be a list separted by a semicolon */
    sangerCount = chopByChar(words[0], ';', sanger, ArraySize(sanger));
    uniStsId = cloneString(words[1]);
    for (j = 0; j < sangerCount; j++)
        {
        if ((al = hashFindVal(aliasHash, sanger[j])) != NULL)
            {
           /* add this UniSTS ID to the alias structure if this value is NULL */
            found = FALSE;
            for (i = 0; i < NUMSANGER && (!found); i++)
                {
                if (al->uniStsId[i] == NULL)
                    {
                    found = TRUE;
                    al->uniStsId[i] = cloneString(uniStsId);
                    }
                }
            /* if the UniStsId read here is not the same as that stored from markers file, then report */
            } 
        else
            fprintf(stderr, "This Sanger name, %s, is not found in the alias hash\n", sanger[j]);
        }
    }
}

void writeAliasTable(FILE *alias)
/* print out tabbed file for loading into the alias table, bacCloneAlias */
{
struct hashEl *aliasList = NULL, *aliasEl = NULL;
struct hashEl *bacList = NULL, *bacEl = NULL;
struct alias *al = NULL;
struct bac *bac = NULL;
char *name = NULL;
int i, j;

aliasList = hashElListHash(aliasHash);
bacList = hashElListHash(bacHash);
if (aliasList != NULL)
    {
    /* walk through list of hash elements */
    for (aliasEl = aliasList; aliasEl != NULL; aliasEl = aliasEl->next)
        {
        al = (struct alias *)aliasEl->val;
        name = cloneString(aliasEl->name);
        /* print out row for Sanger STS name and each alias */
        for (i = 0; i < NUMALIASES && (al->aliases[i] != NULL); i++) 
            {
            fprintf(alias, "%s\t%s\n", al->aliases[i], al->sangerName);
            }
        fflush(alias);
        }
    }
else
    errAbort("The hash of BAC clones aliases is empty or there was an error retrieving the list of entries in the hash\n");
/* those BAC clones without an entry in the alias hash have no sangerName */
/* so do not print to this table */
hashElFreeList(&aliasList);
hashElFreeList(&bacList);
}

void printBacInfo(FILE *xRef, struct bac *bac)
/* print out BAC clone information apart from names */
{
int i;

if (bac != NULL)
    {
    for (i = 0; i < NUMCHROMS && (bac->chrom[i] != NULL); i++)  
        {
        /* print list of chromosomes to which BAC clone is mapped */
        fprintf(xRef, "%s,", bac->chrom[i]);
        }
 
    if (bac->chrom[0] == NULL)
        fprintf(xRef, "\\N");
    /* print Genbank accession if available */
    if (bac->acc != NULL)
        fprintf(xRef, "\t%s\t", bac->acc);
    else
        fprintf(xRef, "\t\\N\t");
    }
else
    fprintf(xRef,"\\N\t\\N\t");
}

void printXRefTable (FILE *xRef, struct bac *bac, struct alias *al, int nameIndex)
/* print out tabbed file for bacCloneXRef table */
{
int i, j;
boolean printOnce = FALSE;

/* print cross-reference table: */
/* extName, intName, chromosome(s) mapped to by BLAT, Genbank accession */ 
/* Sanger STS name, relationship (method of finding STS),*/
/* UniSTS ID(s), 5' (left) primer, 3' (right) primer */

if (al != NULL)
    {
    /* if nameIndex >= 0 then print information just for the extName and intName */
    /* at that index in those arrays else print information for all BACs in arrays */
    for (j = 0; j < MAXSANGER && (al->extName[j] != NULL); j++)
        {
        if (nameIndex >= 0)
            {
            j = nameIndex;
            printOnce = TRUE;
            }
        fprintf(xRef, "%s\t%s\t", al->extName[j], al->intName[j]);
        printBacInfo(xRef, bac);
        if (al->sangerName != NULL)
            fprintf(xRef, "%s\t", al->sangerName);
        else
            fprintf(xRef, "\\N\t");
        fprintf(xRef, "%d\t", al->relation[j]);
       /* go through array and print the UniSTS IDs in a comma separated list */
        for (i = 0; i < NUMSANGER && (al->uniStsId[i] != NULL); i++)
            {
            fprintf(xRef, "%s,", al->uniStsId[i]);
            }
        if (al->uniStsId[0] == NULL)
            fprintf(xRef, "\\N");
        fprintf(xRef, "\t");

        if (al->primer1 != NULL)
            fprintf(xRef, "%s\t%s\n", al->primer1, al->primer2);
        else
            fprintf(xRef, "\\N\t\\N\n");
        if (printOnce)
            j = MAXSANGER - 1;
        }
    }
else
    {
    fprintf(xRef, "%s\t%s\t", bac->extName, bac->intName);
    printBacInfo(xRef, bac);
    fprintf(xRef, "\\N\t0\t\\N\t\\N\t\\N\n");
    }
fflush(xRef);
}

void writeXRefTable(FILE *xRef)
/* print out tabbed files for loading into XRef table, bacCloneXRef */
{
struct hashEl *bacList = NULL, *bacEl = NULL;
struct bac *bac = NULL;
struct alias *al = NULL, *alias = NULL;
struct hashEl *aliasList = NULL, *aliasEl = NULL;
struct sanger *s = NULL;
int j, i = 0, index = -1;

bacList = hashElListHash(bacHash);
aliasList = hashElListHash(aliasHash);

if (bacList != NULL)
    {
    /* walk through list of hash elements */
    for (bacEl = bacList; bacEl != NULL; bacEl = bacEl->next)
        {
        bac = (struct bac *)bacEl->val;
        /* get the sanger names struct for same BAC clone */
       if ((s = hashFindVal(sangerByExtNameHash, bac->extName)) != NULL)
            {
         /* find alias struct for each sanger name and print out row of table */
            for (j = 0; j < MAXSANGER && (s->sangerName[j] != NULL); j++)
                {
                al = hashFindVal(aliasHash, s->sangerName[j]);
            /* find alias extName array index that corresponds to the extName */
                for (i = 0; i < MAXSANGER && (al->extName[i] != NULL); i++)
                     {
                     if (sameString(bac->extName, al->extName[i]))
                         index = i;
                     }
                printXRefTable(xRef, bac, al, index);
                }
            }
       else 
            {
            printXRefTable(xRef, bac, NULL, index);
            }
       }
       fflush(stderr);
       fflush(xRef);
   }
else
    errAbort("The hash of BAC clones is empty or there was an error retrieving the list of entries in the hash\n");
if (aliasList != NULL)
    {
    /* walk through list of hash elements */
    for (aliasEl = aliasList; aliasEl != NULL; aliasEl = aliasEl->next)
        {
        alias = (struct alias *)aliasEl->val;
        /* if this extName is not found in the BAC clones hash */
        for (j = 0; j < MAXSANGER && (alias->extName[j] != NULL); j++)
            {          
            if ((bac = hashFindVal(bacHash, alias->extName[j])) == NULL)
                {
                /* send index in alias of extName and intName to be printed */
                printXRefTable(xRef, NULL, alias, j);
                }
            }
          fflush(xRef);
        }
    }
else
    errAbort("The hash of BAC clones aliases is empty or there was an error retrieving the list of entries in the hash\n");
}

int main(int argc, char *argv[])
{
struct lineFile *cgf, *clf, *mkf, *nmf, *acf, *usf;
FILE *bacAlias, *bacXRef; 
int verb = 0;
char *dir, errorFile[256], aliasFile[256], xRefFile[256];

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc != 8)
    {
fprintf(stdout, "USAGE: formatZfishSts [-verbose=<level>] <contig file> <clone file> <marker file> <list of BACs and chroms> <accessions> <UniSTS IDs file><output directory>\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);

fprintf(stdout, "Opening and reading files ...\n");
cgf = lineFileOpen(argv[1], TRUE );
clf = lineFileOpen(argv[2], TRUE);
mkf = lineFileOpen(argv[3], TRUE);
nmf = lineFileOpen(argv[4], TRUE);
acf = lineFileOpen(argv[5], TRUE);
usf = lineFileOpen(argv[6], TRUE);
dir = cloneString(argv[7]); 

sprintf(errorFile, "%s/error.log", dir);
sprintf(aliasFile, "%s/bacAlias.tab", dir);
sprintf(xRefFile, "%s/bacXRef.tab", dir);

fprintf(stdout, "files are error: %s, alias: %s and xRef: %s \n", errorFile, aliasFile, xRefFile);
stderr = mustOpen(errorFile, "w");
bacAlias = mustOpen(aliasFile, "w");
bacXRef = mustOpen(xRefFile, "w");

/* Read in contigs file with internal name, external name and FPC contig */
verbose(1, "Reading contig names file\n");
readContigs(cgf);

/* Read in internal BAC clone names and Sanger sts names  */
verbose(1, "Reading BAC clone names and Sanger sts names\n");
readCloneNames(clf);

/* Read in BAC clone information: Sanger name, UniSTS ID and aliases */
verbose(1, "Reading markers file\n");
readMarkers(mkf);

/* Read in file of BAC clones names and chroms mapped to by blat */
verbose(1, "Reading file of BAC clone external names\n");
readBacNames(nmf);

/* Read in internal names and corresponding GenBank accessions */
verbose(1, "Reading accessions file\n");
readAccessions(acf);

/*Read file of UniSTS IDs and BAC Clone IDs */
verbose(1, "Reading UniSTS IDs file\n");
readUniStsIds(usf);

/* Print out the STS marker information to a file */
verbose(1, "Creating output file\n");
fprintf(stdout, "Printing tab files for bacCloneAlias and bacCloneXRef tables ...\n");
writeAliasTable(bacAlias);
writeXRefTable(bacXRef);

lineFileClose(&nmf);
lineFileClose(&cgf);
lineFileClose(&clf);
lineFileClose(&mkf);
lineFileClose(&acf);
lineFileClose(&usf);
carefulClose(&bacXRef);
carefulClose(&bacAlias);
carefulClose(&stderr);

freeHashAndVals(&bacHash);
freeHashAndVals(&extNameHash);
freeHashAndVals(&aliasHash);
freeHashAndVals(&sangerByExtNameHash);

return(0);
}

