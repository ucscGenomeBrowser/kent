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

/* we made modified derivitives to save mem selecting minimum fields.
#include "dbSnpRs.h"
#include "affy10KDetails.h"
#include "affy120KDetails.h"
*/

#include "hdb.h"
#include "snpMap.h"

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


// ==========================================================================


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

// safef(fileName,sizeof(fileName),"/cluster/bluearc/snp/hg16/build122/seq/ds_ch%s.xml.contig.seq.gz",ch);

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
    char *strand;		/* strand*/
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand "
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


/* ------------------------------------------------ */

struct snpMap *readSnpMap(char *chrom)
/* Slurp in the snpMap rows for one chrom */
{
struct snpMap *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select * from snpMap where chrom='%s' order by name", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpMapLoad(&row[1]);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}


/* --- save memory by defining just the fields needed from dbSnpRs  ---- */

struct dbSnpRs
/* Information from dbSNP at the reference SNP level */
    {
    struct dbSnpRs *next;  	/* Next in singly linked list. */
    char *rsId;			/* dbSnp reference snp (rs) identifier */
    char *assembly;		/* the sequence in the assembly */
    };

struct dbSnpRs *dbSnpRsLoad(char **row)
/* Load a dbSnpRs from row fetched with select * from dbSnpRs
 * from database.  Dispose of this with dbSnpRsFree(). */
{
struct dbSnpRs *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->rsId = cloneString(row[0]);
ret->assembly = cloneString(row[1]);
return ret;
}

void dbSnpRsFree(struct dbSnpRs **pEl)
/* Free a single dynamically allocated dbSnpRs such as created
 * with dbSnpRsLoad(). */
{
struct dbSnpRs *el;

if ((el = *pEl) == NULL) return;
freeMem(el->rsId);
freeMem(el->assembly);
freez(pEl);
}

void dbSnpRsFreeList(struct dbSnpRs **pList)
/* Free a list of dynamically allocated dbSnpRs's */
{
struct dbSnpRs *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dbSnpRsFree(&el);
    }
*pList = NULL;
}

struct dbSnpRs *readDbSnps(char *tbl)
/* Slurp in the entire dbSnpRs table */
{
struct dbSnpRs *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select rsId, assembly from hgFixed.%s order by rsId", tbl);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = dbSnpRsLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}


/* --- save memory by defining just the fields needed from affy10KDetails  ---- */


struct affy10KDetails
/* Information from affy10KDetails representing the Affymetrix 10K Mapping Array */
    {
    struct affy10KDetails *next;  /* Next in singly linked list. */
    char *affyId;	/* Affymetrix SNP id */
    char *rsId;	/* RS identifier (some are null) */
    char sequenceA[35];	/* The A allele with flanking sequence */
    };


struct affy10KDetails *affy10KDetailsLoad(char **row)
/* Load a affy10KDetails from row fetched with select * from affy10KDetails
 * from database.  Dispose of this with affy10KDetailsFree(). */
{
struct affy10KDetails *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->affyId = cloneString(row[0]);
ret->rsId = cloneString(row[1]);
strcpy(ret->sequenceA, row[2]);
return ret;
}


void affy10KDetailsFree(struct affy10KDetails **pEl)
/* Free a single dynamically allocated affy10KDetails such as created
 * with affy10KDetailsLoad(). */
{
struct affy10KDetails *el;

if ((el = *pEl) == NULL) return;
freeMem(el->affyId);
freeMem(el->rsId);
freez(pEl);
}

void affy10KDetailsFreeList(struct affy10KDetails **pList)
/* Free a list of dynamically allocated affy10KDetails's */
{
struct affy10KDetails *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    affy10KDetailsFree(&el);
    }
*pList = NULL;
}


struct affy10KDetails *readAffy10()
/* Slurp in the entire affy10KDetails table */
{
struct affy10KDetails *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select affyId, rsId, sequenceA from hgFixed.affy10KDetails order by affyId");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = affy10KDetailsLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}


/* --- save memory by defining just the fields needed from affy120KDetails  ---- */

struct affy120KDetails
/* Information from affyGenoDetails representing the Affymetrix 120K SNP Genotyping array */
    {
    struct affy120KDetails *next;  /* Next in singly linked list. */
    int affyId;	/* Affymetrix SNP id */
    char *rsId;	/* RS identifier (some are null) */
    char sequenceA[35];	/* The A allele with flanking sequence */
    };


struct affy120KDetails *affy120KDetailsLoad(char **row)
/* Load a affy120KDetails from row fetched with select * from affy120KDetails
 * from database.  Dispose of this with affy120KDetailsFree(). */
{
struct affy120KDetails *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->affyId = sqlSigned(row[0]);
ret->rsId = cloneString(row[1]);
strcpy(ret->sequenceA, row[2]);
return ret;
}

void affy120KDetailsFree(struct affy120KDetails **pEl)
/* Free a single dynamically allocated affy120KDetails such as created
 * with affy120KDetailsLoad(). */
{
struct affy120KDetails *el;

if ((el = *pEl) == NULL) return;
freeMem(el->rsId);
freez(pEl);
}

void affy120KDetailsFreeList(struct affy120KDetails **pList)
/* Free a list of dynamically allocated affy120KDetails's */
{
struct affy120KDetails *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    affy120KDetailsFree(&el);
    }
*pList = NULL;
}

struct affy120KDetails *readAffy120()
/* Slurp in the entire affy10KDetails table */
{
struct affy120KDetails *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
/* added cast in order by clause because we must match the sort order of snpMap.name which is a string */
safef(query, sizeof(query), "select affyId, rsId, sequenceA from hgFixed.affy120KDetails order by cast(affyId as char)");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = affy120KDetailsLoad(row);
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

int leftFlank(char *s1)
/* look for length of lowercase left flank */
{
char *s2 = s1;
while ((*s1!=0) && (toupper(*s1)!=*s1))
    {
    ++s1;
    }
return (s1 - s2);
}

int rightFlank(char *s1)
/* look for length of lowercase right flank */
{
char *st = s1;
char *se = st+strlen(st)-1;  /* position at last actual char before zero term */
s1 = se;
while ((s1>=st) && (toupper(*s1)!=*s1))
    {
    --s1;
    }
return (se - s1);
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


// NOT USED. this function is now probably not needed can remove
int lengthOneDash(char *s1)
/* get length, count at most one dash */
{
int dash = 0;
int cnt = 0;
while (*s1!=0) 
    {
    if (*s1 == '-')
	{
	if (!dash)
	    ++dash;
	}
    else
	{
	++cnt;
	}
    ++s1;
    }
return (cnt + dash);
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
//toUpperN(dna1, len1);
//toUpperN(dna2, len2);

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

//debug changed:
//char *checkAndFetchNib(struct dnaSeq *chromSeq, struct snpMap *snp, int lf, int ls)
char *checkAndFetchNib(struct dnaSeq *chromSeq, struct snp *snp, int lf, int ls)
/* fetch nib return string to be freed later. Reports if segment exceends ends of chromosome. */
{
if ((snp->chromStart - lf) < 0) 
    {
    printf("snpMap error (chromStart - offset) < 0 : %s %s %u %u %d \n",
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

void snpValid()
/* Test snpMap --> dbSnpRs/affy for one assembly. */
{


char *Org;
char *dbSnpTbl = NULL;

struct dbSnpRs *dbSnps = NULL;
struct dbSnpRs *dbSnp = NULL;

struct affy10KDetails *affy10s = NULL;
struct affy10KDetails *affy10  = NULL;

struct affy120KDetails *affy120s = NULL;
struct affy120KDetails *affy120  = NULL;

struct axtScoreScheme *simpleDnaScheme = NULL;

int match = 0;         /* good match of minimal acceptable quality */
int mismatch = 0;      /* unacceptable match quality */
int missing = 0;       /* unable to find rsId in dbSnpRs/affy */
int goodrc = 0;        /* matches after reverse-complement */
int assemblyDash = 0;  /* assembly context is just a single dash - (complex cases) */
int gapNib = 0;        /* nib returns n's, we are in the gap */

int totalMatch = 0;
int totalMismatch = 0;
int totalMissing = 0;
int totalGoodrc = 0;
int totalAssemblyDash = 0;
int totalGapNib = 0;

boolean affy = FALSE;

int mode = 3;  
void *next = NULL;
char *id   = NULL;
char *seq  = NULL;
char affy120id[12];

int matchScore = 100;
int misMatchScore = 100;
int gapOpenPenalty = 400;  // was 400, tried 250 ok
int gapExtendPenalty = 5; // was 50, reducing for new snp version.

int noDna = 0;
//int snpMapRows = 0;
int snpRows = 0;


/* controls whether affy120k, affy10k, or dbSnpRs is used 
   currently affys are human only
*/
if (!hDbIsActive(db))
    {
    printf("Currently no support for db %s\n", db);
    return;
    }

hSetDb(db);

Org = hOrganism(db);

if (sameWord(Org,"Human"))
    affy = TRUE;

//debug:
affy=FALSE;


if (sameWord(Org,"Human"))
    dbSnpTbl = "dbSnpRsHg";
else if (sameWord(Org,"Mouse"))
    dbSnpTbl = "dbSnpRsMm";
else if (sameWord(Org,"Rat"))
    dbSnpTbl = "dbSnpRsRn";
else 
    {
    printf("Currently no support for Org %s\n", Org);
    return;
    }

simpleDnaScheme = axtScoreSchemeSimpleDna(matchScore, misMatchScore, gapOpenPenalty, gapExtendPenalty);

//uglyf("dbSnp Table=%s \n",dbSnpTbl);

uglyf("Affy=%s \n", affy ? "TRUE" : "FALSE" );

//debug: disabled
//dbSnps = readDbSnps(dbSnpTbl);
//printf("read hgFixed.%s \n",dbSnpTbl);

if (affy)
    {
    affy10s = readAffy10();
    printf("read hgFixed.affy10KDetails \n");

    affy120s = readAffy120();
    printf("read hgFixed.affy120KDetails \n");
    }



int bogus = 0;

// debug
if (0) 
    {
    printf("rsId     assembly-sequence                     \n");
    printf("---------------------------------------------- \n");
    for (dbSnp = dbSnps; dbSnp != NULL; dbSnp = dbSnp->next)
	{
    	printf("%s %s \n",
	  dbSnp->rsId,
	  dbSnp->assembly
	  );
    
	// debug: cut it short for testing only
	if (++bogus > 1)
    	    break;
    
	}
    printf("\n");
    printf("\n");
    }
	

bogus=0;

struct slName *cns = hAllChromNames();
struct slName *cn=NULL;
if (!cns)
    {
    printf("testDb: hAllChromNames returned empty list \n");
    return;
    }


if (affy)
    {
    mode=1; /* start on affy120 with numbers in snpMap.rsId */
    }
else
    {
    mode=2; /* start on dbSnps with "rs*" in snpMap.rsId */
    }

printf("maxFlank = %d \n",maxFlank);
printf("threshold = %d \n",threshold);

for (cn = cns; cn != NULL; cn = cn->next)
    {
    struct dnaSeq *chromSeq = NULL;
    //debug: removed
    //struct snpMap *snps = NULL;
    //struct snpMap *snp = NULL;
    struct snp *snps = NULL;
    struct snp *snp = NULL;

    struct flank *flanks = NULL;
    struct flank *flank = NULL;
    
    if (chr != NULL)
	if (!sameWord(chr,cn->name))
	    continue;

    // currently there are no flanks for chrN_random
    if (endsWith(cn->name,"_random"))
	continue;
    // currently there are no flanks for 
    if (endsWith(cn->name,"_hla_hap1"))
	continue;
    if (endsWith(cn->name,"_hla_hap2"))
	continue;
    // currently no flanks for chrM 
    if (sameWord(cn->name,"chrM"))
	continue;

    uglyf("beginning chrom %s \n",cn->name);
   
    chromSeq = hLoadChrom(cn->name);
    printf("chrom %s :  size (%u) \n",cn->name,chromSeq->size);
    
    //debug: removed
    //snps = readSnpMap(cn->name);
    //printf("read %s.snpMap where chrom=%s \n",db,cn->name);
    
    snps = readSnp(cn->name);
    printf("read %s.snp done for chrom=%s \n",db,cn->name);
        
    flanks = readFlank(cn->name);
    printf("readFlank done for chrom %s \n",cn->name);
    if (flanks == NULL)
	{
	errAbort("readFlank returned NULL for chrom %s.\n",cn->name);
	}
    
    printf("slCount(flanks)=%d for chrom %s \n",slCount(flanks),cn->name);

    slSort(&flanks, slFlankCmp);
    printf("slSort done for chrom %s \n",cn->name);

    
    flank   = flanks; 
    //dbSnp   = dbSnps; 
    affy10  = affy10s;
    affy120 = affy120s;
    
    printf("=========================================================\n");
    for (snp = snps; snp != NULL; snp = snp->next)
	{
	int cmp = -1;
	char *nibDna=NULL;
	char *nibDnaRc=NULL;

	//++snpMapRows;
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

	
        while (cmp < 0)
	    {
	    while (cmp < 0)
		{
    		switch (mode)
		    {
		    case 1:
			next = affy120; break;
		    case 2:
			//next = dbSnp; break;
			next = flank; break;
		    case 3:
			next = affy10; break;
		    }
		if (next == NULL) 
		    {
		    switch (mode)
			{
			case 1:
			    ++mode; break;
			case 2:
			    ++mode; break;
			case 3:
			    cmp = 1; break;
			}
		    }
		else
		    {
		    break;
		    }
		}
		
	    if (cmp < 0)
		{
		switch (mode)
		    {
		    case 1:
			/* have int type but want string */
			safef(affy120id, sizeof(affy120id), "%d", affy120->affyId); 
			id = affy120id;
			break;
		    case 2:
			//id = dbSnp->rsId; break;
			id = flank->rsId; break;
		    case 3:
			id = affy10->affyId; break;
		    }
		cmp=mystrcmp(id, snp->name);
		}
		
	    if (cmp < 0) 
		{
		switch (mode)
		    {
		    case 1:
			affy120 = affy120->next; break;
		    case 2:
			//dbSnp = dbSnp->next; break;
			flank = flank->next; break;
		    case 3:
			affy10 = affy10->next; break;
		    }
		}
	    }	
	    

	if (cmp==0) 
	    {
	    int strand=1;
	    char *rc = NULL;
	    int m = 0;
	    int lf = 0;  /* size of left flank context (lower case dna) (this was when dbSnpRs? may still be true for affy) */
	    int rf = 0;  /* size of right flank context (lower case dna) */
	    int ls = 0;  /* total size of assembly dna context plus actual region in dbSnpRs/affy */
	    char *origSeq = NULL; /* use to display the original dnSnpRs.assembly seq */
	    int flankSize = 0;
	    
	    switch (mode)
		{
		case 1:
		    seq = affy120->sequenceA; break;
		case 2:
		    //seq = dbSnp->assembly; break;
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
		    /* at Daryl's request, upper case */
		    toUpperN(flank->leftFlank , lf);
		    toUpperN(flank->rightFlank, rf);
		    
		    flankSize = lf+1+rf;
		    seq = needMem(flankSize+1);
		    safef(seq,flankSize+1,"%s-%s",flank->leftFlank,flank->rightFlank);
		    //debug
		    //uglyf("flanks together=%s \n",seq);
		    break;
		case 3:
		    seq = affy10->sequenceA; break; 
		}
		
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
	    if (mode != 2)
		{
    		lf = leftFlank(origSeq);
    		rf = rightFlank(origSeq);
		}
	    
	    //debug
	    //uglyf("flanks lf=%d, rf=%d \n",lf,rf);
	    
	    seq = cloneString(origSeq);
	    /* remove dashes indicating insert to simplify and correct processing of nib data */
	    stripDashes(seq);     
	    
            //oldway: ls = strlen(seq);      /* used to be: lengthOneDash(seq); */
	    ls = lf + rf + (snp->chromEnd - snp->chromStart);
	    
	    //debug
	    //uglyf("about to call checkandFetchNib origSeq=%s lf=%d, rf=%d ls=%d \n", origSeq, lf, rf, ls);
	
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
	    
	    //debug
	    //uglyf("got past checkandFetchNib call: \n nibDna=%s  \n",nibDna);
	
            if (allNs(nibDna))
		{
		++gapNib;
		++mismatch;
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
	    
		//debug
    		//uglyf("rc: about to call checkandFetchNib \n");
	
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
	    
		//debug
		//uglyf("rc: got past checkandFetchNib call: \n rc Dna=%s  \n",rc);
	
		reverseComplement(rc,strlen(rc));
		int n = misses(seq, rc);
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
		    bestScore = axtAln->score / (lf+rf);   // was  ls;
		    }
		axtFree(&axtAln);
		
		if (bestScore < threshold)
		    {
		    ZeroVar(&target);
		    target.dna = rc;
		    target.size = strlen(target.dna);
		    axtAln = axtAffine(&query, &target, simpleDnaScheme);
		    if ((axtAln) && (bestScore < (axtAln->score / (lf+rf))))  // was ls
			{
			strand = -1;
			bestScore = axtAln->score / (lf+rf);  // was ls;
			}
		    axtFree(&axtAln);
		    }
		
		if (bestScore >= threshold)
		    {
    		    ++match;
		    if (strand < 1)
      			++goodrc;
		    }
		else
		    {
    		    ++mismatch;
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
	    
	    if (mode==2)
		freez(&origSeq);
	    
	
	    }
	else
	    {
	    char snpLkup[10] = "";
	    /* this id is missing from dbSnpRs/affy! */
	    ++missing;
	    switch (mode)
		{
		case 1:
		    safef(snpLkup,sizeof(snpLkup),"%s","affy120"); break;
		case 2:
		    //safef(snpLkup,sizeof(snpLkup),"%s",dbSnpTbl); break;
		    safef(snpLkup,sizeof(snpLkup),"flnk%s",snp->chrom); break;
		case 3:
		    safef(snpLkup,sizeof(snpLkup),"%s","affy10"); break;
		}
	    if (Verbose)		    
    		//printf("snpMap.name=%s is missing from %s (now at %s) \n\n",snp->name,snpLkup,id);
    		printf("snp.name=%s is missing from %s (now at %s) \n\n",snp->name,snpLkup,id);
	    }
	
	
	freez(&nibDna);
    
	// debug: cut it short for testing only
	//break;
    
	}
    //debug: removed	    
    //snpMapFreeList(&snps);
    snpFreeList(&snps);
    
    flankFreeList(&flanks);

    dnaSeqFree(&chromSeq);  

    printf("\n\n\n Total matches for chrom %s:\n ",cn->name);
    printf("             matches: %u \n ",match);
    printf("          mismatches: %u \n",mismatch);
    printf(" missing from flanks: %u \n",missing);
    printf("   rev compl matches: %u \n",goodrc);
    printf("        assembly = -: %u \n",assemblyDash);
    printf("         nib in gap : %u \n",gapNib);
     
    printf("\n\n=========================================\n");
    
    totalMatch    += match;
    totalMismatch += mismatch;
    totalMissing  += missing;
    totalGoodrc   += goodrc;
    totalAssemblyDash += assemblyDash;
    totalGapNib   += gapNib;
    
    match        = 0;
    mismatch     = 0;
    missing      = 0;
    goodrc       = 0;
    assemblyDash = 0;
    gapNib       = 0;
    // debug: cut it to just one or two chrom for testing
    //if (++bogus > 1)
    //    break;
    
    printf("\n");
    printf("\n");
    
    }

slFreeList(&cns);

//debug: removed
//dbSnpRsFreeList(&dbSnps);
if (affy) 
    {
    affy10KDetailsFreeList(&affy10s);
    affy120KDetailsFreeList(&affy120s);
    }

axtScoreSchemeFree(&simpleDnaScheme);

printf("\n\n\n Grand Totals:  \n ");
printf("             matches: %u \n ",totalMatch);
printf("          mismatches: %u \n",totalMismatch);
printf(" missing from flanks: %u \n",totalMissing);
printf("   rev compl matches: %u \n",totalGoodrc);
printf("        assembly = -: %u \n",totalAssemblyDash);
printf("         nib in gap : %u \n",totalGapNib);


//printf("\n       Total rows in snpMap: %u \n ",snpMapRows);
printf("\n          Total rows in snp: %u \n ",snpRows);
printf("\n        # no dna found for : %u \n ",noDna);

printf("\n\n=========================================\n");

}


int main(int argc, char *argv[])
/* Process command line. */
{

//pushCarefulMemHandler(200000000);

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



