/* snpMask - Print SNPs (single base substituions) using IUPAC codes and flanking sequences. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpMask.c,v 1.3 2005/06/30 09:08:35 heather Exp $";

char *database = NULL;
char *chromName = NULL;

struct snp
/* Information from snp */
    {
    struct snp *next;  	        /* Next in singly linked list. */
    char *name;			/* rsId  */
    int chromStart;               /* start   */
    char *observed;		/* observed variants (usually slash-separated list) */
    };

struct snp *snpLoad(char **row)
/* Load a snp from row fetched with select * from snp
 * from database.  Dispose of this with snpFree(). */
{
struct snp *ret;

AllocVar(ret);
ret->name       = cloneString(row[0]);
ret->chromStart   =        atoi(row[1]);
ret->observed   = cloneString(row[2]);
return ret;
}

void snpFree(struct snp **pEl)
/* Free a single dynamically allocated snp such as created * with snpLoad(). */
{
struct snp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->observed);
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


safef(query, sizeof(query), "select name, chromStart, observed from snp "
"where chrom='%s' and chromEnd = chromStart + 1 and class = 'snp' and locType = 'exact'", chrom);
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


char iupac(char *observed, char orig)
{
    if (sameString(observed, "A/C")) return 'M';
    if (sameString(observed, "A/G"))  return 'R';
    if (sameString(observed, "A/T"))  return 'W';
    if (sameString(observed, "C/G"))  return 'S';
    if (sameString(observed, "C/T"))  return 'Y';
    if (sameString(observed, "G/T"))  return 'K';
    if (sameString(observed, "A/C/G"))  return 'V';
    if (sameString(observed, "A/C/T"))  return 'H';
    if (sameString(observed, "A/G/T"))  return 'D';
    if (sameString(observed, "C/G/T"))  return 'B';
    if (sameString(observed, "A/C/G/T"))  return 'N';

    return orig;
}

void printSnpSeq(struct snp *snp, struct dnaSeq *seq)
{
int i = 0;
int startPos = (snp->chromStart) - 20;
int endPos = startPos + 40;
char *ptr = seq->dna;

printf("%s: ", snp->name);
for (i=startPos; i < endPos; i++)
  printf("%c", ptr[i]);
printf("\n");

}

void snpMask(char *nibFile, char *outFile)
/* snpMask - Print a nib file, using IUPAC codes for single base substitutions. */
{
struct dnaSeq *seq;
char *ptr;
struct snp *snps = NULL;
struct snp *snp = NULL;

if (!hDbIsActive(database))
    {
    printf("Currently no support for %s\n", database);
    return;
    }
hSetDb(database);

seq = nibLoadAll(nibFile);
ptr = seq->dna;
snps = readSnp(chromName);
// do all substitutions
for (snp = snps; snp != NULL; snp = snp->next)
    {
    ptr[snp->chromStart] = iupac(snp->observed, ptr[snp->chromStart]);
    }
// print
for (snp = snps; snp != NULL; snp = snp->next)
    {
    printSnpSeq(snp, seq);
    }

faWrite(outFile, chromName, seq->dna, seq->size);
snpFreeList(&snps);
dnaSeqFree(&seq);  
}

int main(int argc, char *argv[])
/* Process command line. */
{
database = argv[1];
chromName = argv[2];
snpMask(argv[3], argv[4]);
return 0;
}
