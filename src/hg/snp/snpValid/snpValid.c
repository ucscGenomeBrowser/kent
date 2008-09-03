/* snpValid - Test validity of snpMap, dbSnpRsXx, and affy120KDetails, affy10KDetails. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "options.h"
#include "errCatch.h"
#include "ra.h"
#include "fa.h"
#include "nib.h"

#include "hdb.h"

/* we have defined it our self, use only needed fields, 
   so we don't need this.
#include "snp.h"
*/

#include "axt.h"

#include <time.h>

static char const rcsid[] = "";

/* Command line variables. */
char *db  = NULL;	/* DB from command line */

char *flankPath  = NULL; /* path to flank files *.seq.gz */

char *chr = NULL;	/* chr name from command line e.g "chr1" */

int threshold = 70;  /* minimum score for match */

int Verbose = 0;    /* set to 1 to see more detailed output 
(used upper case because var name collision in namespace) */

int maxFlank = 25;  /* maximum length of each flanking sequence, left and right. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpValid - Validate snp alignments\n"
  "usage:\n"
  "   snpValid db flankPath\n"
  "     db is the database, e.g. hg16.\n"
  "     flankPath is the path to the flank files *.seq.gz.\n"
  "options:\n"
  "   -chr=name - Use chrom name (e.g chr1) to limit validation, default all chromosomes.\n"
  "   -threshold=number - Use threshold as minimum score 0-99, default %u.\n"
  "   -verbose=number - Use verbose=1 to see all output mismatches, default %u.\n"
  "   -maxFlank=number - Use maxFlank as maximum length of each flank, default %u.\n"
  , threshold, Verbose, maxFlank);
}


static struct optionSpec options[] = {
   { "chr"      , OPTION_STRING },
   { "threshold", OPTION_INT    },
   { "verbose"  , OPTION_INT    },
   { "maxFlank" , OPTION_INT    },
   { NULL       , 0             },
};


/* ========================================================================== */

#define BASE_SNP_EX_NUM 21   /* next avail number we can use */

enum snpExceptionType {
    snpExNotExact,
    snpExMismatch,
    snpExNoFlanks,
    snpExWrongStrand,
    snpExCount
};

static char *snpExDesc[] = {
    "Not an exact match for locType=exact and size=1",
    "Mismatch below threshold.",
    "No flank data.",
    "Wrong Strand reported."
};

FILE *exf[snpExCount];

void openExOuts(char *db)
{
int i;
char fname[256];
for (i=0;i<snpExCount;i++)
    {
    safef(fname,sizeof(fname),"%ssnpException.%d.bed",db,i+BASE_SNP_EX_NUM);
    exf[i]=mustOpen(fname,"w");
    fprintf(exf[i],"# exceptionId:  %d\n",i+BASE_SNP_EX_NUM);
    fprintf(exf[i],"# query:        %s\n",snpExDesc[i]);
    }
}

void closeExOuts()
{
int i;
for (i=0;i<snpExCount;i++)
    {
    carefulClose(&exf[i]);
    }
}




/* --- save memory by defining just the fields needed from flank file  ---- */

struct flank
/* Information from snp */
    {
    struct flank *next;  	/* Next in singly linked list. */
    char *rsId;			/* reference snp (rs) identifier */
    char *leftFlank;		/* leftFlank  */
    char *rightFlank;		/* rightFlank */
    char *observed;		/* observed   */
    };

int slFlankCmp(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct flank *a = *((struct flank **)va);
const struct flank *b = *((struct flank **)vb);
return strcmp(a->rsId, b->rsId);
}

struct flank *flankLoad(char **row)
/* Load a flank from row fetched from linefile
 * Dispose of this with flankFree(). */
{
struct flank *ret;

AllocVar(ret);
ret->rsId       = cloneString(row[0]);
ret->leftFlank  = cloneString(row[1]);
ret->rightFlank = cloneString(row[2]);
ret->observed   = cloneString(row[3]);
return ret;
}

void flankFree(struct flank **pEl)
/* Free a single dynamically allocated flank such as created
 * with flankLoad(). */
{
struct flank *el;

if ((el = *pEl) == NULL) return;
freeMem(el->rsId);
freeMem(el->leftFlank);
freeMem(el->rightFlank);
freeMem(el->observed);
freez(pEl);
}

void flankFreeList(struct flank **pList)
/* Free a list of dynamically allocated snp's */
{
struct flank *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    flankFree(&el);
    }
*pList = NULL;
}


struct flank *readFlank(char *chrom)
/* Slurp in the flank rows for one chrom */
{
char fileName[256]="";
struct lineFile *lf=NULL;
struct flank *list=NULL, *el;
char *row[4];
int rowCount=4;
char *ch = &chrom[3];

/*
example flankPath:
    /cluster/bluearc/snp/hg16/build122/seq/ds_ch%s.xml.contig.seq.gz
*/

safef(fileName,sizeof(fileName),"%s/ds_ch%s.xml.contig.seq.gz",flankPath,ch);

lf=lineFileMayOpen(fileName, TRUE);
if (lf == NULL)
    {
    safef(fileName,sizeof(fileName),"%s/ds_ch%s.xml.contig.seq",flankPath,ch);
    lf=lineFileMayOpen(fileName, TRUE);
    if (lf == NULL)
	{
	return NULL;
	}
    }

while (lineFileNextRowTab(lf, row, rowCount))
    {
    el = flankLoad(row);
    slAddHead(&list,el);
    }
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
lineFileClose(&lf);
return list;
}


/* --- save memory by defining just the fields needed from snp  ---- */

struct snp
/* Information from snp */
    {
    struct snp *next;  	        /* Next in singly linked list. */
    char *chrom;		/* chrom */
    int chromStart;             /* start */
    int chromEnd;               /* end   */
    char *name;		        /* name  */
    char *strand;		/* strand */
    char *observed;		/* observed variants (usually slash-separated list) */
    char *locType;		/* location Type */
    };

struct snp *snpLoad(char **row)
/* Load a snp from row fetched with select * from snp
 * from database.  Dispose of this with snpFree(). */
{
struct snp *ret;

AllocVar(ret);
ret->chrom      = cloneString(row[0]);
ret->chromStart =        atoi(row[1]);
ret->chromEnd   =        atoi(row[2]);
ret->name       = cloneString(row[3]);
ret->strand     = cloneString(row[4]);
ret->observed   = cloneString(row[5]);
ret->locType    = cloneString(row[6]);
return ret;
}

void snpFree(struct snp **pEl)
/* Free a single dynamically allocated snp such as created
 * with snpLoad(). */
{
struct snp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freeMem(el->strand);
freeMem(el->observed);
freeMem(el->locType);
freez(pEl);
}

void snpFreeList(struct snp **pList)
/* Free a list of dynamically allocated snp's */
{
struct snp *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpFree(&el);
    }
*pList = NULL;
}


struct snp *readSnp(char *chrom)
/* Slurp in the snp rows for one chrom */
{
struct snp *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, observed, locType "
"from snp where chrom='%s' order by name", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}



/* --- -----------------------------------------------------------  ---- */

int mystrcmp(char *s1, char *s2)
/* case insensitive compare */
{
if ((*s1!=0) && (toupper(*s1)==toupper(*s2))) 
    while ((*s1!=0) && (toupper(*++s1)==toupper(*++s2))) ;
return (toupper(*s1) - toupper(*s2));
}

boolean allNs(char *s1)
/* check for all Ns */
{
if ((*s1!=0) && (toupper(*s1)=='N')) 
    while ((*s1!=0) && (toupper(*++s1)=='N')) ;
return (*s1 == 0);
}


void stripDashes(char *s1)
/* remove all Dashes from string inplace */
{
char *s2 = s1;
while (*s1!=0)
    {
    if (*s1 != '-')
	{
	*s2++ = *s1;
	}
    ++s1;
    }
*s2 = 0;
}



int misses(char *dna1, char *dna2)
/*  compare two short dna strands of equal length, 
return number of mismatches */
{
int len1 = strlen(dna1);
int len2 = strlen(dna2);
int misscnt = 0;
int i = 0;
int len = len1;
if (len2 < len)
  len = len2;
for(i=0;i<len;++i)
    {
    if (toupper(dna1[i]) != toupper(dna2[i]))
	{
	++misscnt;
	}
    }
misscnt += abs(len2-len1);
return misscnt;
}

char *checkAndFetchNib(struct dnaSeq *chromSeq, struct snp *snp, int lf, int ls)
/* fetch nib return string to be freed later. Reports if segment exceends ends of chromosome. */
{
if ((snp->chromStart - lf) < 0) 
    {
    printf("snp error (chromStart - offset) < 0 : %s %s %u %u %d \n",
	snp->name,
	snp->chrom,
	snp->chromStart,
	snp->chromEnd,
	lf
	);
    return NULL;
    }
if ((snp->chromStart - lf + ls)  > chromSeq->size) 
    {
    printf("bed error chromStart - leftFlank + seqlen > chromSeq->size : %s %s %u %u  chr-size: %u \n",
	snp->name,
	snp->chrom,
	snp->chromStart,
	snp->chromEnd,
	chromSeq->size
	);
    return NULL;
    }

return cloneStringZ(chromSeq->dna + snp->chromStart - lf, ls);
}

void writeExOut(enum snpExceptionType i, struct snp *snp)
{
fprintf(exf[i],"%s\t%d\t%d\t%s\t%d\t%s\t%s\n",
  snp->chrom,
  snp->chromStart,
  snp->chromEnd,
  snp->name, 
  i+BASE_SNP_EX_NUM, 
  snp->strand,
  snp->locType
  );
}


/* ---------------------------------------------------------------- */

void snpValid()
/* Test snp for one assembly. */
{


char *Org;

struct axtScoreScheme *simpleDnaScheme = NULL;

int match = 0;         /* good match of minimal acceptable quality */
int mismatch = 0;      /* unacceptable match quality */
int missing = 0;       /* unable to find rsId in dbSnpRs/affy */
int goodrc = 0;        /* matches after reverse-complement */
int assemblyDash = 0;  /* assembly context is just a single dash - (complex cases) */
int gapNib = 0;        /* nib returns n's, we are in the gap */
int strandMismatch = 0;   /* reported strand differs from what we found */

int totalMatch = 0;
int totalMismatch = 0;
int totalMissing = 0;
int totalGoodrc = 0;
int totalAssemblyDash = 0;
int totalGapNib = 0;
int totalStrandMismatch = 0; 


void *next = NULL;
char *id   = NULL;
char *seq  = NULL;

int matchScore = 100;
int misMatchScore = 100;
int gapOpenPenalty = 400;  
int gapExtendPenalty = 5; /* was 50, reducing for new snp version. */

int noDna = 0;
int snpRows = 0;

int goodExact = 0;
int badExact = 0;
struct slName *cns=NULL,*cn=NULL;

if (!hDbIsActive(db))
    {
    printf("Currently no support for db %s\n", db);
    return;
    }

Org = hOrganism(db);

simpleDnaScheme = axtScoreSchemeSimpleDna(matchScore, misMatchScore, gapOpenPenalty, gapExtendPenalty);

if (!cns)
    {
    printf("testDb: hAllChromNames returned empty list \n");
    return;
    }

openExOuts(db);  /* open separate files for each snp class of Exception of interest */

printf("maxFlank = %d \n",maxFlank);
printf("threshold = %d \n",threshold);

cns = hAllChromNames(db);
for (cn = cns; cn != NULL; cn = cn->next)
    {
    struct dnaSeq *chromSeq = NULL;
    struct snp *snps = NULL;
    struct snp *snp = NULL;

    struct flank *flanks = NULL;
    struct flank *flank = NULL;
    
    if (chr != NULL)
	if (!sameWord(chr,cn->name))
	    continue;

    uglyf("reading flanks %s \n",cn->name);
    
    flanks = readFlank(cn->name);
    if (flanks == NULL)
	{
	printf("readFlank returned NULL for chrom %s.\n",cn->name);
	continue;
	}
    printf("readFlank done for chrom %s \n",cn->name);
    
    printf("slCount(flanks)=%d for chrom %s \n",slCount(flanks),cn->name);

    slSort(&flanks, slFlankCmp);
    printf("slSort done for chrom %s \n",cn->name);

    uglyf("beginning chrom %s \n",cn->name);
   
    chromSeq = hLoadChrom(db, cn->name);
    printf("chrom %s :  size (%u) \n",cn->name,chromSeq->size);
    
    snps = readSnp(cn->name);
    printf("read %s.snp done for chrom=%s \n",db,cn->name);
        
    flank   = flanks; 
    
    printf("=========================================================\n");
    for (snp = snps; snp != NULL; snp = snp->next)
	{
	int cmp = -1;
	char *nibDna=NULL;

	++snpRows;

	
	if (Verbose)
	    {
	    printf("debug %s %u %u %s %s\n",
	      snp->chrom,
	      snp->chromStart,
	      snp->chromEnd,
	      snp->name,
	      snp->strand
	      );
	    }
	/* continue; */

	/* add check for Heather for specific case locType=exact and size=1 */
	if (sameString(snp->locType,"exact") && (snp->chromEnd == (snp->chromStart + 1)))
	    {
	    char *obs=cloneString(snp->observed);
            int  n = chopString(obs, "/", NULL, 0); 
            char **obsvd = needMem(n*sizeof(char*)); 
	    char *exactDna = checkAndFetchNib(chromSeq, snp, 0, 1);
	    int i=0;
	    boolean found = FALSE;
	    if (sameString(snp->strand,"-"))
		{
		reverseComplement(exactDna,strlen(exactDna));
		}
	    chopString(obs, "/", obsvd, n);
	    if (exactDna==NULL) 
		{
		printf("exactDna=NULL for %s %s %u %u (%s) %s \n",
		    snp->name,
		    snp->chrom,
		    snp->chromStart,
		    snp->chromEnd,
		    snp->observed,
		    snp->locType
		    );
		}
		
	    /*
	    uglyf("%s: exactDna=%s obs=%s \n",snp->name,exactDna,snp->observed);
	    */
	    
	    for(i=0; i<n; i++)
		{
		if (strlen(obsvd[i]) > 1)
		    {
		    printf("%s: incorrect length of observed %s <> 1 \n",
			snp->name, obsvd[i]
			);
		    }
		if (sameWord(obsvd[i],exactDna))
		    {
		    found = TRUE;
		    }
		}
	    if (found) { goodExact++; }
	    else 
	      { 
	      badExact++; 
	      uglyf("id: %s exact %s not found in observed %s \n",snp->name,exactDna,snp->observed); 
	      writeExOut(snpExNotExact, snp);
	      }
	    freez(&obsvd);
	    freez(&obs);
	    freez(&exactDna);
	    }
	
	
        while (cmp < 0)
	    {
	    while (cmp < 0)
		{
		next = flank;
		if (next == NULL) 
		    {
		    cmp = 1;
		    }
		else
		    {
		    break;
		    }
		}
		
	    if (cmp < 0)
		{
		id = flank->rsId;
		cmp=mystrcmp(id, snp->name);
		}
		
	    if (cmp < 0) 
		{
		flank = flank->next; 
		}
	    }	
	    

	if (cmp==0) 
	    {
	    int strand=1;
	    char *rc = NULL;
	    int m = 0;
	    int lf = 0;  /* size of left  flank context (lower case dna)  */
	    int rf = 0;  /* size of right flank context (lower case dna) */
	    int ls = 0;  /* total size of assembly dna context plus actual region in dbSnpRs/affy */
	    char *origSeq = NULL; /* use to display the original dnSnpRs.assembly seq */
	    int flankSize = 0;
	
	    lf=strlen(flank->leftFlank);
	    rf=strlen(flank->rightFlank);
	    
	    /* if flanks exceed maxFlank, truncate */
	    if (lf>maxFlank) 
		{
		char *temp=flank->leftFlank;
		flank->leftFlank=cloneString(temp+lf-maxFlank);
		lf = maxFlank;
		freez(&temp);
		}
	    if (rf>maxFlank) 
		{
		rf = maxFlank;
		flank->rightFlank[rf]=0;
		}
	    /* at Daryl's request, try to make them same case */
	    toLowerN(flank->leftFlank , lf);
	    toLowerN(flank->rightFlank, rf);
	    
	    flankSize = lf+1+rf;
	    seq = needMem(flankSize+1);
	    safef(seq,flankSize+1,"%s-%s",flank->leftFlank,flank->rightFlank);
	
		
            if (sameString(seq,"-"))
		{
		++assemblyDash;
		if (Verbose)
		printf("(no assembly context) rsId=%s chrom=%s %u %u \n assembly=%s \n\n",
		  id,
		  snp->chrom,

		  snp->chromStart,
		  snp->chromEnd,
		  seq
		  );
		continue;
		}
	
	    origSeq = seq;
	    
	    seq = cloneString(origSeq);
	    /* remove dashes indicating insert to simplify and correct processing of nib data */
	    stripDashes(seq);     
	    
	    ls = lf + rf + (snp->chromEnd - snp->chromStart);
	    
	    nibDna = checkAndFetchNib(chromSeq, snp, lf, ls);
	    if (nibDna==NULL) 
		{
		++noDna;
		printf("no dna for %s %s %u %u \n",
		    snp->name,
	  	    snp->chrom,
		    snp->chromStart,
	  	    snp->chromEnd
		    );
		continue;
		}
	    
            if (allNs(nibDna))
		{
		++gapNib;
		++mismatch; writeExOut(snpExMismatch, snp);
		if (Verbose)
		printf("(nib gap) rsId=%s chrom=%s %u %u \n assembly=%s \n  snpMap=%s \n\n",
		  id,
		  snp->chrom,
		  snp->chromStart,
		  snp->chromEnd,
		  seq,
		  nibDna
		  );
		continue;
		}
		
	    m = misses(seq,nibDna);
	    if (m > 1)
		{
		int n = -1;
	    
		rc = checkAndFetchNib(chromSeq, snp, rf, ls);
		if (rc==NULL) 
		    {
		    ++noDna;
		    printf("no dna for %s %s %u %u \n",
			snp->name,
			snp->chrom,
			snp->chromStart,
			snp->chromEnd
			);
		    continue;
		    }
	    
		reverseComplement(rc,strlen(rc));
		n = misses(seq, rc);
		if (n < m) 
		    {
		    strand=-1;
		    m = n;
		    }
		}
	    if (m <= 1)
		{
		++match;
		if (strand < 1)
		  ++goodrc;
		}
	    else
		{
		struct dnaSeq query, target;
		struct axt *axtAln = NULL;
		int bestScore = 0; 
		ZeroVar(&query);
		query.dna = seq;
		query.size = strlen(query.dna);
		
		ZeroVar(&target);
		target.dna = nibDna;
		target.size = strlen(target.dna);
		axtAln = axtAffine(&query, &target, simpleDnaScheme);
		strand = 1;
		if (axtAln) 
		    {
		    bestScore = axtAln->score / (lf+rf); 
		    }
		axtFree(&axtAln);
		
		if (bestScore < threshold)
		    {
		    ZeroVar(&target);
		    target.dna = rc;
		    target.size = strlen(target.dna);
		    axtAln = axtAffine(&query, &target, simpleDnaScheme);
		    if ((axtAln) && (bestScore < (axtAln->score / (lf+rf))))  
			{
			strand = -1;
			bestScore = axtAln->score / (lf+rf); 
			}
		    axtFree(&axtAln);
		    }
		
		if (bestScore >= threshold)
		    {
    		    ++match;
		    if (strand < 1)
      			++goodrc;
		    if (((strand == 1) && (!sameString(snp->strand,"+")))
       		    || ((strand == -1) && (!sameString(snp->strand,"-"))))
			{
    			strandMismatch++;   /* reported strand differs from what we found */
			writeExOut(snpExWrongStrand, snp);
			}
		    }
		else
		    {
    		    ++mismatch;
		    writeExOut(snpExMismatch, snp);
		    }
		
		if ((bestScore < threshold) || Verbose) 
		    {
		    printf(
			"score=%d misses=%u strand=%d repstr=%s rsId=%s chrom=%s %u %u lf=%d ls=%d \n"
			"   flanks=%s \n"
			"      chr=%s \n"
			"   rc chr=%s \n"
			"\n",
		      bestScore,
		      m,
		      strand,       /* best match found */
		      snp->strand,  /* reported from snp table */
		      id,
		      snp->chrom,
		      snp->chromStart,
		      snp->chromEnd,
		      lf,
		      ls,
		      seq,
		      nibDna,
		      rc
		      );
		     } 
		
		}
		
	    freez(&rc);
	    freez(&seq);
	    
	    freez(&origSeq);
	    
	
	    }	    
	else   /* if (cmp==0) */
	    {
	    char snpLkup[10] = "";
	    /* this id is missing from flanks */
	    ++missing;
	    safef(snpLkup,sizeof(snpLkup),"flnk%s",snp->chrom); 
	    if (Verbose)		    
    		printf("snp.name=%s is missing from %s (now at %s) \n\n",snp->name,snpLkup,id);
	    writeExOut(snpExNoFlanks, snp);
	    }
	
	
	freez(&nibDna);
    
	}
    snpFreeList(&snps);
    
    flankFreeList(&flanks);

    dnaSeqFree(&chromSeq);  

    printf("\n\n\n Total matches for chrom %s:\n ",cn->name);
    printf("             matches: %u \n ",match);
    printf("          mismatches: %u \n",mismatch);
    printf(" missing from flanks: %u \n",missing);
    printf("   rev compl matches: %u \n",goodrc);
    printf("    not rptd strand : %u \n",strandMismatch);
    printf("        assembly = -: %u \n",assemblyDash);
    printf("         nib in gap : %u \n",gapNib);
     
    printf("\n\n=========================================\n");
    
    totalMatch    += match;
    totalMismatch += mismatch;
    totalMissing  += missing;
    totalGoodrc   += goodrc;
    totalStrandMismatch  += strandMismatch;
    totalAssemblyDash    += assemblyDash;
    totalGapNib   += gapNib;
    
    match        = 0;
    mismatch     = 0;
    missing      = 0;
    goodrc       = 0;
    strandMismatch = 0;
    assemblyDash = 0;
    gapNib       = 0;
    
    printf("\n");
    printf("\n");
    
    }

slFreeList(&cns);

closeExOuts();  /* close Exception class output files */

axtScoreSchemeFree(&simpleDnaScheme);

printf("\n\n\n Grand Totals:  \n ");
printf("             matches: %u \n",totalMatch);
printf("          mismatches: %u \n",totalMismatch);
printf(" missing from flanks: %u \n",totalMissing);
printf("   rev compl matches: %u \n",totalGoodrc);
printf("    not rptd strand : %u \n",totalStrandMismatch);
printf("        assembly = -: %u \n",totalAssemblyDash);
printf("         nib in gap : %u \n",totalGapNib);


printf("\n          Total rows in snp: %u \n ",snpRows);
printf("\n        # no dna found for : %u \n ",noDna);

printf("\n          Total goodExact: %u \n ",goodExact);
printf("\n          Total  badExact: %u \n ",badExact);

printf("\n\n=========================================\n");

}


int main(int argc, char *argv[])
/* Process command line. */
{

/*
pushCarefulMemHandler(200000000);
*/

/* Set initial seed */
srand( (unsigned)time( NULL ) );
 
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
db = argv[1];
flankPath = argv[2];
chr       = optionVal("chr"      , chr      );
threshold = optionInt("threshold", threshold);
Verbose   = optionInt("verbose"  , Verbose  );
maxFlank  = optionInt("maxFlank" , maxFlank );

snpValid();

carefulCheckHeap();
return 0;
}



