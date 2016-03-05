/* dbSnp.c - prepare details for snps from dbSnp */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "hdb.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"


#define FLANK  20                 /* Amount of flanking sequence on each side */
#define ALLELE 210                /* Maximum supported allele length */
#define REGION ((FLANK*2)+ALLELE) /* Maximum length of the region */

void usage()
/* Explain usage and exit. */
{
errAbort("dbSnp - Get the top strand allele from the current assembly\n"
	 "based on a specification file including the rs#s for the SNPs.\n"
	 "Use getObsHet to parse dbSnp XML files into correct input format.\n"
	 "Usage:\n  \tdbSnp database fileBase          >& logfile &\n"
	 "Example:\n\tdbSnp hg16     dbSnpRsHg16Snp119 >& dbSnpLog &\n"
	 "\nwhere\tfileBase.obs is read as input (from getObsHet), "
	 "\n\tfileBase.out is formatted for loading in hgFixed.dbSnpRsXX, and "
	 "\n\tfileBase.err shows errors.\n");
}

struct dbInfo
/* Info on a SNP from the database */
{
    struct dbInfo *next;
    char  *chrom;            /* Chromosome name - not allocated here. */
    int    chromPos;         /* Position of SNP. */
    char  *name;             /* Allocated in hash. */
    char   baseInAsm[2];     /* Base in assembly. */
    char   region[REGION+1]; /* SNP +/- FLANK bases */
};

struct fileInfo
/* Info on a SNP from the input file (XML digest). */
{
    /* information directly from the file */
    char   rsId[20];            /* reference Snp ID (name) */
    float  avgHet;              /* Average Heterozygosity Standard Error */
    float  avgHetSE;            /* Average Heterozygosity */
    char   valid[256];          /* validation status */
    char   allele1[ALLELE+1];   /* First allele */
    char   allele2[ALLELE+1];   /* Second allele */ 
    char   seq5[FLANK+1];       /* 5' flanking sequence */
    char   seq3[FLANK+1];       /* 3' flanking sequence */
    char   func[256];           /* functional classificaton */
    /* reconstructed observed and alternate alleles with reverse complements */
    char   observed[REGION+1];  /* Observed sequence in browser */
    char   alternate[REGION+1]; /* Alternate sequence in browser */
    char   obsrc[REGION+1];     /* reverse complement of observed */
    char   altrc[REGION+1];     /* reverse complement of alternate */
    /* lowercase versions for string comparison */
    char   obsLow[REGION+1];    /* lowercase version for comparison */
    char   altLow[REGION+1];    /* lowercase version for comparison */
    char   obsrcLow[REGION+1];  /* lowercase version for comparison */
    char   altrcLow[REGION+1];  /* lowercase version for comparison */
};

enum snpMatchType
/* How a snp from the XML files agrees with the assembly */
{
    smtAlleleError = -1, /* Oversized alleles */
    smtNoMatch     =  0, /* No match */
    smtObserved    =  1, /* Matches observed */
    smtAlternate   =  2, /* Matches alternate */
    smtObservedRC  =  3, /* Matches observed reverse complement */
    smtAlternateRC =  4, /* Matches alternate reverse complement*/
};

struct slName *getChromList()
/* Get list of all chromosomes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row = NULL;
struct slName *list = NULL;
struct slName *el = NULL;
char  *queryString = NOSQLINJ "select   chrom "
                     "from     chromInfo "
                     "where    chrom not like '%random' " 
                     "order by size desc";
sr = sqlGetResult(conn, queryString);
while ((row=sqlNextRow(sr)))
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
slReverse(&list);  /* Restore order */
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

void convertToLowercase(char *ptr)
/* convert and entire string to lowercase */
{
for( ; *ptr !='\0'; ptr++)
    *ptr=tolower(*ptr);
}

void convertToUppercase(char *ptr)
/* convert and entire string to uppercase */
{
for( ; *ptr !='\0'; ptr++)
    *ptr=toupper(*ptr);
}

long int addSnpsFromChrom(char *chromName, struct dnaSeq *seq,
			  char *snpTable, struct hash *snpHash)
/* Add all snps in table on chromosome to hash */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row = NULL;
struct dyString *query = newDyString(256);
char   rsId[20];
unsigned long int snpCount = 0;

sqlDyStringPrintf(query, "select chromStart, "
	       "       name "
	       "from   %s "
	       "where  chrom = '%s' "
	       "  and  source not like 'A%%' ", 
	       snpTable, chromName);
sr = sqlGetResult(conn, query->string);
while ((row=sqlNextRow(sr)))
    {
    struct dbInfo *db;

    strcpy(rsId, row[1]);
    AllocVar(db);
    hashAddSaveName(snpHash, rsId+2, db, &db->name+2);
    db->chromPos = atoi(row[0]);
    strncpy(db->baseInAsm, seq->dna+(db->chromPos),1);
    strncpy(db->region,seq->dna+(db->chromPos)-FLANK,REGION);
    convertToLowercase(db->region);
    snpCount++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
freeDyString(&query);
return snpCount;
}

struct hash *makeSnpInfoHash(char *database)
/* Make hash of info for all SNPs */
{
struct slName    *chromList = getChromList();
struct slName    *chrom;
struct hash      *snpHash = newHash(24); /* Make hash 2^24 (16M) big */
unsigned long int snpMapCount = 0;
unsigned long int snpMapCumul = 0;
struct dnaSeq *seq = NULL;
/* walk through list of chromosomes, get seq, get snps, save in hash */
for (chrom = chromList; chrom; chrom = chrom->next)
    {
    printf("Chrom %s:\tloading seq ... ",chrom->name);fflush(stdout);
    seq = hLoadChrom(chrom->name);
    printf("and SNPs ... ");fflush(stdout);
    snpMapCount = addSnpsFromChrom(chrom->name, seq, "snpMap", snpHash);
    snpMapCumul += snpMapCount;
    printf("(%ld) ... done.\n",snpMapCount);fflush(stdout);
    freeDnaSeq(&seq);
    }
printf("Total SNPs loaded: %ld from snpMap.\n", snpMapCumul);
return snpHash;
}

void parseInputLine(char *row[9], struct fileInfo *fi, FILE *e)
/* parse the observed SNP data from dbSnp*/
{
strcpy(fi->rsId,    row[0]);
fi->avgHet   = atof(row[1]);
fi->avgHetSE = atof(row[2]);
strcpy(fi->valid,   row[3]);
strcpy(fi->allele1, row[4]);
strcpy(fi->allele2, row[5]);
if (strcmp(fi->allele1,"(NOT_BIALLELIC)"))
    {
    strcpy(fi->seq5,    row[6]);
    strcpy(fi->seq3,    row[7]);
    }
else
    {
    ZeroVar(fi->seq5);
    ZeroVar(fi->seq3);
    }
strcpy(fi->func,    row[8]);

/* post warnings if alleles are too long */
if (strlen(row[4])>ALLELE) 
    fprintf(e, "PROBLEM WITH ALLELE1 in %s: %s (%d characters)\n",
	    fi->rsId, row[4], strlen(row[4]));
if (strlen(row[5])>ALLELE) 
    fprintf(e, "PROBLEM WITH ALLELE2 in %s: %s (%d characters)\n",
	    fi->rsId, row[5], strlen(row[5]));
}

void makeObsAndAlt(struct fileInfo *fi)
/* combine the observed and alternate alleles from the flanking sequence  */
{
convertToUppercase(fi->allele1);
convertToUppercase(fi->allele2);
convertToLowercase(fi->seq5);
convertToLowercase(fi->seq3);

/* remove gap ('-') characters */
if (startsWith(fi->allele1,"-")) 
    ZeroVar(fi->allele1);
if (startsWith(fi->allele2,"-")) 
    ZeroVar(fi->allele2);

/* combine alleles with flanking regions */
ZeroVar(fi->observed);
ZeroVar(fi->alternate);
snprintf(fi->observed, sizeof(fi->observed), "%s%s%s",
	 fi->seq5,fi->allele1,fi->seq3);
snprintf(fi->alternate,sizeof(fi->alternate),"%s%s%s",
	 fi->seq5,fi->allele2,fi->seq3);
}

boolean checkForInDel(struct fileInfo *fi)
/* check to see if an allele was written as LARGEDELETION or LARGEINSERTION */
{
if (strstr(fi->allele1,"large") || strstr(fi->allele2,"large"))
    return FALSE;
else
    return TRUE;
}

void getWobbleBase(char *dna, char* wobble)
/* return the first wobble base not found in the input string */
{
char iupac[]="nxbdhryumwskv";
int  offset = strcspn(dna, iupac);

strncpy(wobble, dna+offset, 1);
}

int getGapLen(char *allele)
/* correction for the gap length on the observed strand */
{
if (strlen(allele)==0 || startsWith(allele,"-")) 
    return 1; 
else
    return 0;
}

void makeRc(struct fileInfo *fi)
/* make reverse complements of observed and alternate */
{
strcpy(fi->obsrc, fi->observed);
strcpy(fi->altrc, fi->alternate);

reverseComplement(fi->obsrc, strlen(fi->obsrc));
reverseComplement(fi->altrc, strlen(fi->altrc));
}

void makeLow(struct fileInfo *fi)
/* make lowercase versions for string comparison */
{
strcpy(fi->obsLow,   fi->observed);
strcpy(fi->altLow,   fi->alternate);
strcpy(fi->obsrcLow, fi->obsrc);
strcpy(fi->altrcLow, fi->altrc);

convertToLowercase(fi->obsLow);
convertToLowercase(fi->altLow);
convertToLowercase(fi->obsrcLow);
convertToLowercase(fi->altrcLow);
}

enum snpMatchType findMatch(struct fileInfo *fi, char *region)
/* find which reconsructed allele matches the assembly */
{
makeObsAndAlt(fi);
makeRc(fi);
makeLow(fi);
/* store string lengths to reduce calls to strlen() */
int allele1len = strlen(fi->allele1);
int allele2len = strlen(fi->allele2);
int seq5len    = strlen(fi->seq5);
int seq3len    = strlen(fi->seq3);
int obslen     = strlen(fi->observed);
int altlen     = strlen(fi->alternate);
int gaplen     = getGapLen(fi->allele1);

if (allele1len>ALLELE) 
    return smtAlleleError; /* Allele too big to fit in database */
/* start comparisons to determine reference and alternate alleles in browser */
else if (!strncmp(fi->obsLow, region+FLANK-seq5len+gaplen, obslen-allele1len))
    return smtObserved; /* database matches observed */
else if (!strncmp(fi->altLow, region+FLANK-seq5len, altlen-allele2len))
    { /* database matches alternate - swap observed and alternate */
    swapBytes(fi->observed, fi->alternate, REGION+1);
    return smtAlternate;
    }
else if (!strncmp(fi->obsrcLow, region+FLANK-seq3len+gaplen, obslen-allele1len))
    { /* database matches observed RC */
    strcpy(fi->observed, fi->obsrc);
    strcpy(fi->alternate,fi->altrc);
    return smtObservedRC;
    }
else if (!strncmp(fi->altrcLow, region+FLANK-seq3len, altlen-allele2len))
    { /* database matches alternate RC */
    strcpy(fi->observed, fi->altrc);
    strcpy(fi->alternate,fi->obsrc);
    return smtAlternateRC;
    }
else  /* database sequence does not match file sequence */
    return smtNoMatch;
}

void printErr(struct fileInfo *fi, char *dna, FILE *e, char *database)
/* print to the error file */
{
char   *extraTab = " ";
boolean noInDel = TRUE; /* Ignore named indels? (LARGEINSERTION and LARGEDELETION) */
char   wobbleBase[2]; /* What wobble base is present, if any */
int    allele1len = 0;
int    allele2len = 0;
int    regionPrintLen = 2*FLANK;

/* find unsuported characters that led to failure to find a match */
noInDel = checkForInDel(fi);
ZeroVar(wobbleBase);
getWobbleBase(fi->obsLow, wobbleBase);

/* ignore the failures that we know about - indels and wobbles */
if (strlen(wobbleBase)>0 && noInDel) /* true wobble base */
    fprintf(e,"%s has a wobble '%s': %s\n", fi->rsId, wobbleBase, fi->observed);
else if (!noInDel) /* there is a large indel */
    fprintf(e,"%s has an large indel: %s\n", fi->rsId, fi->observed);
else /* print mismatch */
    {
    if (strlen(fi->rsId)<8)
	extraTab = strdup("\t");
    if( (allele1len=strlen(fi->allele1)) >= (allele2len=strlen(fi->allele2))) 
	regionPrintLen += allele1len;
    else
	regionPrintLen += allele2len;
    strncpy(dna+regionPrintLen,"\0",1);

    fprintf(e, "%s%s\t  obs:%s\t%s\t%s\n", 
	    fi->rsId, extraTab, fi->observed, fi->allele1, fi->allele2);
    fprintf(e, "\t\t  alt:%s\n", fi->alternate);
    fprintf(e, "\t\t %4s:%s\n", database, dna);
    fprintf(e, "\t\tobsrc:%s\n", fi->obsrc);
    fprintf(e, "\t\taltrc:%s\n", fi->altrc);
    }
}

void addGaps(struct fileInfo *fi)
/* reinsert gaps for better visual alignment in the details page */
{
/* magnitude and direction of size difference between two alleles */
int  alleleDiff = strlen(fi->observed)-strlen(fi->alternate);

if ( alleleDiff && /* indels only, doesn't work for segmental */
     (!strncmp(fi->allele1,"-",1) || !strncmp(fi->allele2,"-",1)))
    {
    char obsPtr[REGION+1];
    char altPtr[REGION+1];
    char temp[REGION+1];
    int  i = 0;
    int  j = 0;

    strcpy(obsPtr,fi->observed);
    strcpy(altPtr,fi->alternate);
    i=0;
    while (!strncmp(obsPtr+i,altPtr+i,1)) 
	i++;
    ZeroVar(temp);
    strncat(temp,obsPtr,i);
    for (j=0; j<abs(alleleDiff); j++) 
	strcat(temp,"-");
    if (alleleDiff<0)
	sprintf(fi->observed, "%s%s",temp,altPtr+i+j);
    else
	sprintf(fi->alternate,"%s%s",temp,obsPtr+i+j);
    }
}

void resetGaps(struct fileInfo *fi)
/* restore gaps that were removed */
{
if (!strlen(fi->allele1)) 
    strcpy(fi->allele1,"-");
if (!strlen(fi->allele2)) 
    strcpy(fi->allele2,"-");
}

void printOut(FILE *f, struct fileInfo *fi)
/* print details of successful alignment */
{
addGaps(fi);
fprintf(f,"%s\t%f\t%f\t%s\t%s\t%s\t%s\t%s\t%s\n",
	fi->rsId, fi->avgHet, fi->avgHetSE,
	fi->valid, fi->allele1, fi->allele2,
	fi->observed, fi->alternate, fi->func);
}

void getSnpDetails(char *database, char *input, char *output, char *errors)
/* determine which allele matches assembly and store in details file */
{
struct lineFile  *lf = lineFileOpen(input, TRUE); /* input file */
FILE             *f  = mustOpen(output, "w");     /* output file */
FILE             *e  = mustOpen(errors, "w");     /* error file */
char             *row[9]; /* number of fields in input file */
struct hash      *snpHash = NULL; /* stores all SNPs from the database */
unsigned long int inputSnpCount    = 0; /* snp and error counters */
unsigned long int notFoundInDb     = 0;
unsigned long int sequenceMismatch = 0;
unsigned long int largeAlleles     = 0;
struct dbInfo     *db; /* SNP information from the database */
struct fileInfo   *fi; /* SNP information from the input file */
enum snpMatchType matchType = smtNoMatch;

hSetDb(database);
snpHash = makeSnpInfoHash(database);
AllocVar(fi);
while (lineFileRow(lf, row)) /* process one snp at a time */
    {
    inputSnpCount++; 
    parseInputLine(row, fi, e);
    db = hashFindVal(snpHash, fi->rsId+2);
    if (!db) /* rsId not found in the database */
	fprintf(e,"%s not found in %s. (%ld)\n",fi->rsId, database, ++notFoundInDb);
    else /* rsId was found in the database */
	{
	matchType = findMatch(fi, db->region);
	resetGaps(fi);
	if (matchType == smtAlleleError)
	    {
	    largeAlleles++;
	    fprintf(e, "%s has an oversized allele (length > %d)",fi->rsId, ALLELE);
	    }
	else if (matchType == smtNoMatch) /* failed to find a match */
	    {
	    sequenceMismatch++;
	    printErr(fi, db->region+FLANK-(strlen(fi->seq5)), e, database);
	    }
	if (!strcmp(fi->allele1,"(NOT_BIALLELIC)"))
	    {
	    strcpy(fi->observed, "-");
	    strcpy(fi->alternate,"-");
	    }
	printOut(f, fi);
	} /* else - db was found in the hash from the database */
    } /* while - reading lines of the file */

/* print summary statistics */
printf("Database:              %s\n",   database);
printf("Input file:            %s\n",   input);
printf("Output file:           %s\n",   output);
printf("Error file:            %s\n\n", errors);
printf("Total SNPs:            %ld\n",  inputSnpCount);
printf("SNPs not in database:  %ld\n",  notFoundInDb);
printf("Flank Mismatches:      %ld\n",  sequenceMismatch);
printf("Large Alleles (>%3dbp) %ld\n",  ALLELE, largeAlleles);
/* printf("SNPs with details:     %ld\n",  inputSnpCount-notFoundInDb-sequenceMismatch-largeAlleles); */
printf("SNPs with details:     %ld\n",  inputSnpCount-notFoundInDb);
}

int main(int argc, char *argv[])
/* error check, process command line input, and call getSnpDetails */
{
char *database;
char  input[64];
char  output[64];
char  errors[64];
if (argc != 3)
    {
    usage();
    return 1;
    }
database = argv[1];
sprintf(input,  "%s.obs", argv[2]);
sprintf(output, "%s.out", argv[2]);
sprintf(errors, "%s.err", argv[2]);
getSnpDetails(database, input, output, errors);
return 0;
}
