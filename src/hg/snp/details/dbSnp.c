
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

static char const rcsid[] = "$Id: dbSnp.c,v 1.5 2003/05/06 07:22:35 kate Exp $";

#define FLANK  20
#define ALLELE 20
#define REGION ((FLANK*2)+ALLELE)

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
  int    rsId;       /* reference Snp ID (name) */
  float  avgHet;     /* Average Heterozygosity Standard Error */
  float  avgHetSE;   /* Average Heterozygosity */
  char   valid[20];  /* validation status */
  char   allele1[ALLELE+1];   /* First allele */
  char   allele2[ALLELE+1];   /* Second allele */ 
  char   observed[REGION+1];  /* Observed sequence in browser */
  char   alternate[REGION+1]; /* Observed sequence in browser */
};

struct slName *getChromList()
/* Get list of all chromosomes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row=NULL;
struct slName *list = NULL, *el;
char   queryString[]="select chrom from chromInfo where chrom not like '%M%' "
  " and chrom not like '%N%' and chrom not like '%random'";

/* for debug */
/* sprintf(queryString,"select chrom from chromInfo where chrom='chr22'"); */

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
    *ptr=tolower(*ptr);
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
sprintf(query, "select chromStart, name from %s where chrom = '%s'", snpTable, chromName);
sr = sqlGetResult(conn, query);
while ((row=sqlNextRow(sr)))
    {
    struct snpInfo *si;
    AllocVar(si);
    hashAddSaveName(snpHash, row[1], si, &si->name);
    si->chromPos = atoi(row[0]);
    strncpy(si->baseInAsm, seq->dna+(si->chromPos),1);
    strncpy(si->region,seq->dna+(si->chromPos)-FLANK,REGION);
    convertToLowercaseN(si->region,REGION);
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
return snpHash;
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
int      seq5len=0;
int      seq3len=0;
int      allele1len=0;
int      allele2len=0;
int      obslen=0;
int      altlen=0;
int      flankFail=0;
char     seq5[FLANK+1];
char     seq3[FLANK+1];
char     obs[REGION+1];
char     alt[REGION+1];
char     obsrc[REGION+1];
char     altrc[REGION+1];
struct   snpInfo *si;
struct   dbSnpAlleles *dbSnp;

for (i=0;i<96;i++) counts[i] = 0;
for (i=0;i<4;i++) counter[i] = 0;
hSetDb(database);
if ( strncmp(database,"hg",2) == 0 )  isHuman = 1;
snpHash = makeSnpInfoHash(isHuman);
AllocVar(dbSnp);
while (lineFileRow(lf, row)) /* process one snp at a time */
  {
    /* reinitialize */
    seq5len=seq3len=allele1len=allele2len=obslen=altlen=flankFail=0;
    counter[0]++; 
    for ( i = 0; i <= FLANK; i++) 
      {
	strcpy(seq5+i,"\0");
	strcpy(seq3+i,"\0");
      }
    for ( i = 0; i <= ALLELE; i++) 
      {
	strcpy(dbSnp->allele1+i,"\0");
	strcpy(dbSnp->allele2+i,"\0");
      }
    for ( i = 0; i <= REGION; i++) 
      {
	strcpy(dbSnp->observed+i,"\0");
	strcpy(dbSnp->alternate+i,"\0");
	strcpy(obs+i,"\0");
	strcpy(alt+i,"\0");
	strcpy(obsrc+i,"\0");
	strcpy(altrc+i,"\0");
      }

    /* store values from file in local structures */
    dbSnp->rsId     = atoi(row[0]);
    si = hashFindVal(snpHash, row[0]);
    if ( si == NULL )
      counter[1]++; /* rsId not found in database */
    else
      {
	/* get the boring stuff to pass through to output */
	dbSnp->avgHet   = atof(row[1]);
	dbSnp->avgHetSE = atof(row[2]);
	strncpy( dbSnp->valid, row[3], strlen(row[3]));  
	strncpy( dbSnp->valid+strlen(row[3]), "\0", 1);  
	allele1len = strlen(row[4]);
	allele2len = strlen(row[5]);
	strncpy( dbSnp->allele1, row[4], allele1len);
	strncpy( dbSnp->allele2, row[5], allele2len);
	convertToLowercaseN(dbSnp->allele1, allele1len);
	convertToLowercaseN(dbSnp->allele2, allele2len);
	
	/* manipulate to create full flanking regions */
	seq5len = strlen(row[6]);
	seq3len = strlen(row[7]);
	if ( seq5len < FLANK || seq3len < FLANK ) 
	  counter[2]++; /* short flanking sequence */
	strncpy( seq5, row[6], seq5len );
	strncpy( seq3, row[7], seq3len );
	convertToLowercaseN(seq5, seq5len);
	convertToLowercaseN(seq3, seq3len);

	/* generate the observed and alternate sequences from dbSnp     */
	/* while dealing with indels, where blank allele is represented */
	/* as a "-" and generates an incorrect length                   */
	if (strncmp(dbSnp->allele1,"-",1))
	  {
	    sprintf(obs,"%s%s%s",seq5,dbSnp->allele1,seq3);
	    strncpy(obs+seq5len+seq3len+allele1len,"\0", 1);
	  }
	else
	  {
	    sprintf(obs,"%s%s",seq5,seq3);
	    strncpy(obs+seq5len+seq3len,"\0", 1);
	    allele1len=0;
	  }
	if (strncmp(dbSnp->allele2,"-",1))
	  {
	    sprintf(alt,"%s%s%s",seq5,dbSnp->allele2,seq3);
	    strncpy(obs+seq5len+seq3len+allele2len,"\0", 1);
	  }
	else
	  {
	    sprintf(alt,"%s%s",seq5,seq3);
	    strncpy(alt+seq5len+seq3len,"\0", 1);
	    allele2len=0;
	  }

	/* generate reverse complements of observed and alternate alleles */
	obslen=strlen(obs);
	altlen=strlen(alt);
	strncpy(obsrc, obs, obslen );
	strncpy(altrc, alt, altlen );
	reverseComplement(obsrc, obslen);
	reverseComplement(altrc, altlen);

	/* start comparisons to determine reference and alternate alleles in browser */
	if (!strncmp(obs  +allele1len, si->region+FLANK-seq5len+1, obslen-allele1len))
	  { /* database matches observed */
	    strncpy(dbSnp->observed,obs,obslen);
	    strncpy(dbSnp->alternate,alt,altlen);
	  }
	else if (!strncmp(alt  +allele2len, si->region+FLANK-seq5len+1, altlen-allele2len))
	  { /* database matches alternate */
	    strncpy(dbSnp->observed,alt,altlen);
	    strncpy(dbSnp->alternate,obs,obslen);
	  }
	else if (!strncmp(obsrc+allele1len, si->region+FLANK-seq3len+1, obslen-allele1len))
	  { /* database matches observed RC */
	    strncpy(dbSnp->observed,obsrc,obslen);
	    strncpy(dbSnp->alternate,altrc,altlen);
	  }
	else if (!strncmp(altrc+allele2len, si->region+FLANK-seq3len+1, altlen-allele2len))
	  { /* database matches alternate RC */
	    strncpy(dbSnp->observed,altrc,altlen);
	    strncpy(dbSnp->alternate,obsrc,obslen);
	  }
	else 
	  { /* database sequence does not match file sequence */
	    counter[3]++;
	    flankFail = 1;
	  }

	if (flankFail == 0) /* match was found between database and file */
	  fprintf(f,"%i\t%f\t%f\t%s\t%s\t%s\t%s\t%s\n",
		  dbSnp->rsId, dbSnp->avgHet, dbSnp->avgHetSE,
		  dbSnp->valid, dbSnp->allele1, dbSnp->allele2,
		  dbSnp->observed, dbSnp->alternate);
      }
    }
 
/* print summary statistics */
printf("Total SNPs:              \t%ld\n",           counter[0]);
printf("Not in %s:               \t%ld\n", database, counter[1]);
printf("Flank < %d bases:        \t%ld\n", 20,       counter[2]);
printf("%s != %s:\t%ld\t(mismatches in flanking sequences)\n", database,  input, counter[3]);
printf("There should be %ld lines in the output file %s.\n",counter[0]-counter[1]-counter[3],output);
}

void usage()
/* Explain usage and exit. */
{
errAbort("dbSnp - Get the top strand allele from the current assembly\n"
	 "based on a specification file including the rs#s for the SNPs.\n"
	 "Use getObsHet to parse dbSnp XML files into correct input format.\n"
	 "Usage:\n  \tdbSnp database infile     outfile     >& logfile &\n"
	 "Example:\n\tdbSnp hg13     dbSnpInput dbSnpOutput >& dnSnpLog &\n");
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
