
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "hdb.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"

#define FLANK 20
#define REGION (FLANK*2+1) // 41

struct snpInfo
/* Various info on a SNP all in one place. */
{
  struct snpInfo *next;
  char  *chrom;            /* Chromosome name - not allocated here. */
  int    chromPos;         /* Position of SNP. */
  char  *name;             /* Allocated in hash. */
  char   baseInAsm[2];     /* Base in assembly. */
  char   region[REGION+1]; /* SNP +/- FLANK bases */
};

struct dbSnpAlleles
/* More info on a SNP. */
{
  struct dbSnpAlleles *next;
  int rsId;             /* reference Snp ID (name) */
  float avgHetSE;       /* Average Heterozygosity */
  float avgHet;         /* Average Heterozygosity Standard Error */
  char valid[20];       /* validation status */
  char base1[2];        /* Base in first allele */
  char base2[2];        /* Base in second allele */ 
  char base2rc[2];      /* Reverse Complement of base in second allele */
  char base1rc[2];      /* Reverse Complement of base in first allele */
  char ref[REGION+1];   /* reference SNP +/- FLANK bases */
  char alt[REGION+1];   /* alternate SNP +/- FLANK bases */
  char refrc[REGION+1]; /* reference SNP +/- FLANK bases rev comp */
  char altrc[REGION+1]; /* alternate SNP +/- FLANK bases rev comp */
};

struct slName *getChromList()
/* Get list of all chromosomes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row=NULL;
struct slName *list = NULL, *el;
char   queryString[]="select chrom from chromInfo where chrom not like '%M%' "
    "and chrom not like '%N%' and chrom not like '%random'";
//sprintf(queryString,"select chrom from chromInfo where chrom='chr%s'",chr);
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

void convertToLowercaseN(char *ptr, int n)
{
int i;
for( i=0;i<n && *ptr !='\0';i++,ptr++ )
    {
    *ptr=tolower(*ptr);
    }
}

long int addSnpsFromChrom(char *chromName, struct dnaSeq *seq,
			  char *snpTable, struct hash *snpHash)
/* Add all snps in table on chromosome to hash */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row=NULL;
char query[512];
long int snpCount=0;

sprintf(query, "select chromStart,name from %s where chrom = '%s'",
	snpTable, chromName);
sr = sqlGetResult(conn, query);
while ((row=sqlNextRow(sr)))
    {
    struct snpInfo *si;
    AllocVar(si);
    hashAddSaveName(snpHash, row[1], si, &si->name);
    si->chromPos = atoi(row[0]);
    strncpy(si->baseInAsm, seq->dna+(si->chromPos),1);
    strncpy(si->region,seq->dna+(si->chromPos)-FLANK,REGION);
    convertToLowercaseN(si->region,41);
    snpCount++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return snpCount;
}

struct hash *makeSnpInfoHash(int isHuman)
/* Make hash of info for all SNPs */
{
struct slName *chromList = getChromList();
struct slName *chrom;
struct hash *snpHash = newHash(20); /* Make hash 2^20 (1M) big */
long int snpNihCount=0;
long int snpTscCount=0;


for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct dnaSeq *seq = hLoadChrom(chrom->name);
    snpNihCount=addSnpsFromChrom(chrom->name, seq, "snpNih", snpHash);
    if (isHuman) snpTscCount=addSnpsFromChrom(chrom->name, seq, "snpTsc", snpHash);
    freeDnaSeq(&seq);
    }
printf("\n");
return snpHash;
}

int encodeSingleBase(char *base)
{
if (!strncmp(base,"a",1)) return 0;
if (!strncmp(base,"c",1)) return 1;
if (!strncmp(base,"g",1)) return 2;
if (!strncmp(base,"t",1)) return 3;
return -1;
}

int encodeDoubleBase(char *base)
{
if (!strncmp(base,"ac",2)) return 0;
if (!strncmp(base,"ag",2)) return 1;
if (!strncmp(base,"at",2)) return 2;
if (!strncmp(base,"cg",2)) return 3;
if (!strncmp(base,"ct",2)) return 4;
if (!strncmp(base,"gt",2)) return 5;
if (!strncmp(base,"ca",2)) return 0;
if (!strncmp(base,"ga",2)) return 1;
if (!strncmp(base,"ta",2)) return 2;
if (!strncmp(base,"gc",2)) return 3;
if (!strncmp(base,"tc",2)) return 4;
if (!strncmp(base,"tg",2)) return 5;
return -1;
}

int encodeAllBases(char bases[4])
{
int code=0;
code += 24*encodeSingleBase(bases+0);
code +=  6*encodeSingleBase(bases+3);
code +=    encodeDoubleBase(bases+1);
return code;
}

void addAsmBase(char *database, char *input, char *output)
/* Add base in assembly to input. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[8];
char  bases[5]="    \0";
struct hash *snpHash = NULL;
long int counter[4];
long int offset=0;
long int counts[96];
long int counts5[24];
long int counts3[24];
int      index=0;
int      i=0;
int      isHuman=0;

for (i=0;i<96;i++) counts[i]=0;
for (i=0;i<4;i++) counter[i]=0;
hSetDb(database);
if (strncmp(database,"hg",2)==0) isHuman=1;

snpHash = makeSnpInfoHash(isHuman);

while (lineFileRow(lf, row))
    {
    char   seq5[FLANK+1];
    char   seq3[FLANK+1];
    int    len5;
    int    len3;
    char   ref[REGION+1]="                                         \0";
    char   alt[REGION+1]="                                         \0";
    char   refrc[REGION+1]="                                         \0";
    char   altrc[REGION+1]="                                         \0";
    char   alternate[REGION+1]="                                         \0";
    struct snpInfo *si;
    struct dbSnpAlleles *dbSnp;
    AllocVar(dbSnp);
    
    counter[0]++; 
    for (  i = 0; i < (FLANK+1); i++) 
	{
	strcpy(seq5+i,"\0");
	strcpy(seq3+i,"\0");
	}
    for (  i = 0; i < 2; i++) 
	{
	strcpy(dbSnp->base1+i,"\0");
	strcpy(dbSnp->base2+i,"\0");
	}
    
    /* store values from file in local structures */
    dbSnp->rsId     = atoi(row[0]);   
    dbSnp->avgHet   = atof(row[1]);   
    dbSnp->avgHetSE = atof(row[2]);   
    strcpy ( dbSnp->valid, row[3]);  
    strncpy( dbSnp->base1, row[4], 2);
    strncpy( dbSnp->base2, row[5], 2);

    /* manipulate to create full flanking regions */
    strncpy( seq5, row[6], FLANK);
    strncpy( seq3, row[7], FLANK);
    len5=strlen(seq5);
    len3=strlen(seq3);
    sprintf(dbSnp->ref, "%s%s%s",seq5, dbSnp->base1, seq3  );
    sprintf(dbSnp->alt, "%s%s%s",seq5, dbSnp->base2, seq3  );
    strncpy(dbSnp->refrc, dbSnp->ref, REGION);
    strncpy(dbSnp->altrc, dbSnp->alt, REGION);
    reverseComplement(dbSnp->refrc,strlen(dbSnp->refrc));
    reverseComplement(dbSnp->altrc,strlen(dbSnp->altrc));

    /* copy to temporary location for case conversion and comparison */
    strncpy(ref,   dbSnp->ref,   REGION);
    strncpy(alt,   dbSnp->alt,   REGION);
    strncpy(refrc, dbSnp->refrc, REGION);
    strncpy(altrc, dbSnp->altrc, REGION);
    convertToLowercaseN(ref,     REGION);
    convertToLowercaseN(alt,     REGION);
    convertToLowercaseN(refrc,   REGION);
    convertToLowercaseN(altrc,   REGION);
    
    si = hashFindVal(snpHash, row[0]);
    if ( si == NULL ) 
	counter[1]++; // rsId not found in database
    else
	{
	/* encode match */
	i = 0; // no match
	if ( !strncmp( ref+offset,  si->region+offset, REGION-1-2*offset) ) 
	    i += 1000; // file matches database
	if ( !strncmp( alt+offset,  si->region+offset, REGION-1-2*offset) ) 
	    i += 100;  // file matches alternate in database
	if ( !strncmp( refrc+offset,si->region+offset, REGION-1-2*offset) ) 
	    i += 10;   // file matches revComp of database
	if ( !strncmp( altrc+offset,si->region+offset, REGION-1-2*offset) ) 
	    i += 1;    // file matches revComp of alternate in database
	
	/* test cases */
	if ( len5 + len3 != 2*FLANK ) 
	    counter[2]++; // short flanking sequence
	else if (i == 0) 
	    counter[3]++; // database sequence does not match file sequence
	else // ( (len5+len3==2*FLANK) && i!=0 ) 
	    {
	    /*  select alternate sequence for writing */
	    if (i==1000)
		strncpy(alternate, alt+offset, REGION);
	    else if (i==100)
		strncpy(alternate, ref+offset, REGION);
	    else if (i==10)
		strncpy(alternate, altrc+offset, REGION);
	    else if (i==1)
		strncpy(alternate, refrc+offset, REGION);
	    else
		strncpy(alternate,
			"XXXXXXXXXXXXXXXXXXXX-XXXXXXXXXXXXXXXXXXXX",41);

	    /* print results to output file */
	    fprintf(f,"%i\t%f\t%f\t%s\t%s\t%s\t%s\t%s\n",
		    dbSnp->rsId, dbSnp->avgHet, dbSnp->avgHetSE, 
		    dbSnp->valid, dbSnp->base1, dbSnp->base2, 
		    si->region, alternate);

	    /* increment SNP flank counter */
	    strncpy( bases,   seq5+19, 1);     /* 5' flank base */
	    strncpy( bases+1, row[4],   1);    /* first allele */
	    strncpy( bases+2, row[5],   1);    /* second allele */
	    strncpy( bases+3, seq3,     1);    /* 3' flank base */
	    convertToLowercaseN(bases,4);      /* prepare for strcmp */	    
	    index = encodeAllBases(bases);     /* get type of mutation */
	    counts[index]++;                   /* keep track of mutation type */
	    }
	}
    }

/* print summary statistics */
printf("\nTotal SNPs: \t%ld;\nNot in %s: \t%ld;"
       "\nFlank < %ld bases: \t%ld;\n%s != %s: \t%ld\n",
       counter[0],database,counter[1],20-offset,counter[2],
       database, input, counter[3]);
printf("There should be %ld lines in the outupt file %s\n", 
       (counter[0]-counter[1]-counter[2]-counter[3]), output);
printf("\n        ac      ag      at      cg      ct      gt");

for (i=0;i<24;i++) 
    counts5[i]=counts3[i]=0;
for (i=0;i<96;i++) 
    {
    if ( i>0 && (i%24) == 0 ) printf("\n");
    if ( (i%6) == 0 ) printf("\n%i\t",i);
    printf("%ld\t", counts[i]);
    counts5[i%24]+=counts[i];
    counts3[i%6+(i/24*6)]+=counts[i];
    }
printf("\n\n5' dependencies\n        ac      ag      at      cg      ct      gt");
for (i=0;i<24;i++) 
    {
    if ( i>0 && (i%24) == 0 ) printf("\n");
    if ( i == 0 ) printf("\nA\t");
    else if ( i == 6 ) printf("\nT\t");
    else if ( i == 12 ) printf("\nG\t");
    else if ( i == 18 ) printf("\nC\t");
    printf("%ld\t", counts5[i]);
    }
printf("\n\n3' dependencies\n        ac      ag      at      cg      ct      gt");
for (i=0;i<24;i++) 
    {
    if ( i>0 && (i%24) == 0 ) printf("\n");
    if ( i == 0 ) printf("\nA\t");
    else if ( i == 6 ) printf("\nT\t");
    else if ( i == 12 ) printf("\nG\t");
    else if ( i == 18 ) printf("\nC\t");
    printf("%ld\t", counts3[i]);
    }
printf("\n");
}

void usage()
/* Explain usage and exit. */
{
errAbort("dbSnp - get the top strand base from the current assembly\n"
	 "based on a specification file including the rs#s for the SNPs\n"
	 "usage:\n  \tdbSnp database infile     outfile     >& logfile &\n"
	 "example:\n\tdbSnp hg11     dbSnpInput dbSnpOutput >& dnSnpLog &\n");
/* dbSnp hg13 chN.observed/observed   dbSnpOutputHg13 */
}

int main(int argc, char *argv[])
{
char *database;
char *input;
char *output;
if (argc != 4)
    {
    usage();
    return 1;
    }
database=argv[1];
input=argv[2];
output=argv[3];
addAsmBase(database, input, output);
return 0;
}
