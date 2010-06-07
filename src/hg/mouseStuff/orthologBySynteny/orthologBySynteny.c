/* orthologBySynteny - Find syntenic location for a list of locations */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "chain.h"
#include "genePred.h"
#include "axt.h"
#include "axtInfo.h"
#include "gff.h"
#include "genbank.h"

static char const rcsid[] = "$Id: orthologBySynteny.c,v 1.10 2008/09/03 19:20:39 markd Exp $";

#define INTRON 10 
#define CODINGA 11 
#define CODINGB 12 
#define UTR5 13 
#define UTR3 14
#define STARTCODON 15
#define STOPCODON 16
#define SPLICESITE 17
#define NONCONSPLICE 18
#define INFRAMESTOP 19
#define INTERGENIC 20
#define REGULATORY 21
#define LABEL 22

#define INSERT_MERGE_SIZE 50


char *db ; /* from database */
char *db2; /* database that we are mapping synteny to */
int filter; /* max gene prediction allowed */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orthologBySynteny - Find syntenic location for a list of gene predictions on a single chromosome\n"
  "usage:\n"
  "   orthologBySynteny from-db to-db geneTable netTable chrom chainFile\n"
  "options:\n"
  "   -name=field name in gene table (from-db) to get name from [default: name]\n"
  "   -tName=field name in gene table (to-db) to get name from [default: name]\n"
  "   -track=name of track in to-db - find gene overlapping with syntenic location\n"
  "   -psl=geneTable is psl instead of gene prediction\n"
  "   -gff=output gene prediction\n"
  "   -filter=max gene prediction allowed [default 2mb]\n"
  );
}

void gffWrite(struct genePred *gp, FILE *f)
/* write out a standard gff record from a gene pred record - should be moved to gff.c but include file problems */
{
    char start[25] = "start_codon";
    char stop[25] = "stop_codon";
    int i;

    assert(gp->cdsStart >= 0);
    assert(gp->cdsEnd >= 0);
    assert(gp->txStart >= 0);
    assert(gp->txEnd >= 0);
    if (gp->strand[0] == '+')
        fprintf(f, "%s\tucsc\t%s\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number 1\n", 
        gp->chrom, start, (gp->cdsStart)+1, (gp->cdsStart)+3,gp->strand[0], gp->name, gp->name);
    else
        fprintf(f, "%s\tucsc\t%s\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number 1\n", 
        gp->chrom, stop, (gp->cdsStart)+1, (gp->cdsStart)+3,gp->strand[0], gp->name, gp->name);
    for (i=0 ; i< gp->exonCount ; i++)
        {
        if (
            (((gp->strand[0] == '+') && ((gp->exonStarts[i])+1 >= gp->cdsStart)) || (((gp->strand[0] == '-') && ((gp->exonEnds[i]) >= gp->cdsEnd)))) &&
            (((gp->strand[0] == '+') && ((gp->exonEnds[i])+1 <= gp->cdsEnd)) || (((gp->strand[0] == '-') && ((gp->exonEnds[i]) <= gp->cdsStart))))
           )
            /* if coding region write out CDS */
            fprintf(f, "%s\tucsc\tCDS\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number %d\n", 
                gp->chrom, (gp->exonStarts[i])+1, gp->exonEnds[i],gp->strand[0], gp->name, gp->name, i+1
                );
        fprintf(f, "%s\tucsc\texon\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number %d\n", 
            gp->chrom, (gp->exonStarts[i])+1, gp->exonEnds[i],gp->strand[0], gp->name, gp->name, i+1
            );
        }

    if (gp->strand[0] == '+')
        fprintf(f, "%s\tucsc\t%s\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number %d\n", 
            gp->chrom, stop, (gp->cdsEnd)+1, (gp->cdsEnd)+2,gp->strand[0], gp->name, gp->name, i
            );
    else
        fprintf(f, "%s\tucsc\t%s\t%d\t%d\t.\t%c\t.\tgene_id %s\ttranscript_id %s\texon_number %d\n", 
            gp->chrom, start, gp->cdsEnd, (gp->cdsEnd)+2,gp->strand[0], gp->name, gp->name, i
            );
    return;
}
struct hash *allChains(char *fileName)
/* read all chains in a chain file */
{
    struct hash *hash = newHash(0);
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chain *chain;
    char chainId[128];

    while ((chain = chainRead(lf)) != NULL)
        {
        safef(chainId, sizeof(chainId), "%d",chain->id);
        if (hashLookup(hash, chainId) == NULL)
                {
                hashAddUnique(hash, chainId, chain);
                }
        }
    lineFileClose(&lf);
    return hash;
}
struct axt *readAllAxt(char *chrom, char *alignment)
/* read axt records from the database table alignment for a chromosome */
{
char query[255];
struct lineFile *lf ;
struct axt *axt = NULL, *axtList = NULL;
struct axtInfo *ai = NULL;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn(db);
char **row;

if (alignment != NULL)
    snprintf(query, sizeof(query),
	     "select * from axtInfo where chrom = '%s' and species = '%s' and alignment = '%s'",
	     chrom, db2, alignment);
else
    snprintf(query, sizeof(query),
	     "select * from axtInfo where chrom = '%s' and species = '%s'",
	     chrom, db2);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ai = axtInfoLoad(row );
    }
if (ai == NULL)
    {
    printf("\nNo axt alignments available for %s (database %s).\n\n",
	   hFreezeFromDb(db2), db2);
    /*return;*/
    }
lf = lineFileOpen(ai->fileName, TRUE);
while ((axt = axtRead(lf)) != NULL)
    {
    slAddHead(&axtList, axt);
    }
slReverse(&axtList);

axtInfoFree(&ai);
sqlFreeResult(&sr);
hFreeConn(&conn);
return(axtList);
}


void verifyAlist(struct axt *axtList)
/* very crude check for bad axt records */
{
    struct axt *axt;
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    if (!startsWith("chr",axt->qName))
        printf("BAD axt record %s %s tStart = %d score=%d\n",axt->qName, axt->tName, axt->tStart, axt->score);
    }
}

void generateGff(struct chain *chain, FILE *f, char *ortholog, char *name, char *type, double score, int level, struct genePred *gp, struct axt *axtList)
/* output a gff gene prediction*/
{
struct axt *axt= NULL ;
struct axt *axtFound = NULL ;
struct genePred *gpSyn = NULL;
int cStart, cEnd, txStart, txEnd, nextEndIndex, posStrand, tPtr;
int oneSize = 1;
int tCoding = FALSE;
int qCoding = FALSE;
int tClass = INTERGENIC;
int qClass = INTERGENIC;
int prevTClass = INTERGENIC;
int prevQClass = INTERGENIC;
int qStopCodon = FALSE;
int nextStart = gp->exonStarts[0];
int nextEnd = gp->exonEnds[0];
int tCodonPos = 1;
int qCodonPos = 1;
unsigned *eStarts, *eEnds;
int prevQptr = 0;
int lastChance = 0;

AllocVar(gpSyn);
for (axt = axtList; axt != NULL; axt = axt->next)
    { 
    if (sameString(gp->chrom , axt->tName) && ((axt->tStart <= gp->txEnd) && (axt->tEnd >= gp->txStart)) )
        {
    /*    if (gp->strand[0] == '-')
            {
            printf("swap axt t=%s q=%s name=%s\n",axt->tName, axt->qName, gp->name);
            reverseComplement(axt->qSym, axt->symCount);
            reverseComplement(axt->tSym, axt->symCount);
            tmp = hChromSize2(axt->qName) - axt->qStart;
            axt->qStart = hChromSize2(axt->qName) - axt->qEnd;
            axt->qEnd = tmp;
            }
            */
        axtFound = axt;
        break;
        }
    }
if (axtFound == NULL)
    /* no alignment skip this one */
    {
    fprintf(stderr,"No syntenic alignment %s %s:%d-%d %s:%d-%d\n",gp->name, gp->chrom, gp->txStart, gp->txEnd, axt->tName, axt->tStart,axt->tEnd);
    genePredFree(&gpSyn);
    return;
    }

                verifyAlist(axtList);
/* we found the first axt, map gene gp to syntenic gene gpSyn */
gpSyn->chrom = axtFound->qName;
gpSyn->name = gp->name;	/* Name of gene */
gpSyn->exonCount = 0;
gpSyn->exonStarts = AllocArray(eStarts, gp->exonCount);
gpSyn->exonEnds = AllocArray(eEnds, gp->exonCount);
if (axtFound->qStrand == '+' )
    gpSyn->strand[0] = gp->strand[0];	/* + or - for strand */
else
    {
    if (gp->strand[0] == '+')
        {
        gpSyn->strand[0] = '-';
        }
    else
        {
        gpSyn->strand[0] = '+';
        }
    }
nextEndIndex = 0;
assert(nextEndIndex < gp->exonCount);
assert(nextEndIndex >= 0);
nextStart = gp->exonStarts[nextEndIndex];
nextEnd = gp->exonEnds[nextEndIndex];
cStart = gp->cdsStart;
cEnd = gp->cdsEnd-3;
txStart = gp->txStart;
txEnd = gp->txEnd-3;
tPtr = axtFound->tStart;
if (gp->strand[0] == '+')
    {
    posStrand = TRUE;
    }
else
    {
    posStrand = FALSE;
    /*
    nextEndIndex = (gp->exonCount)-1;
    assert(nextEndIndex < gp->exonCount);
    assert(nextEndIndex >= 0);
    nextStart = gp->exonEnds[nextEndIndex];
    nextEnd = gp->exonStarts[nextEndIndex];
    cStart = gp->cdsEnd-1;
    cEnd = gp->cdsStart+1;
    txStart = gp->txEnd-1;
    txEnd = gp->txStart+1;
    tPtr = axtFound->tEnd;
    */
    }

    if (gp->strand[0] == '-')
        {
        verifyAlist(axtList);
        }
    if (sameString("NM_023781",gp->name))
        {
        verifyAlist(axtList);
        }
    if (sameString("NM_010001",gp->name))
        {
        verifyAlist(axtList);
        }
for (axt = axtFound; axt != NULL ; axt = axt->next)
    /* loop thru set of axt alignments for gene */
    {
    char *q = axt->qSym; /* dna sequence from query */
    char *t = axt->tSym; /* dna sequence from target */
    int i = 0; /* index into t array */
    int size = axt->symCount;
    int sizeLeft = size; /* remaining bases in the alignment */
    int qPtr = axt->qStart; /*index into axt file for syntenic gene */

    /* skip alignments if on different chromosome UNLESS we haven't found any exons yet */
    if (!sameString(axt->qName,axtFound->qName))
	{
        if (gpSyn->exonCount != 0)
            continue;
        else
            {
            /* start over with this new (presumably better alignment) */
            axtFound = axt;
            gpSyn->chrom = axtFound->qName;
            gpSyn->cdsStart = 0;
            gpSyn->txStart = 0;
            tCoding = FALSE;
            qCoding = FALSE;
            tClass = INTERGENIC;
            qClass = INTERGENIC;
            prevTClass = INTERGENIC;
            prevQClass = INTERGENIC;
            qStopCodon = FALSE;
            }
	}

    if (sameString(gp->name , "NM_008386"))
        {
        verifyAlist(axtList);
        }
    tPtr = axt->tStart; /*index into axt file for gene */
    while ((tPtr > nextEnd ) && (nextEndIndex < (gp->exonCount)-1))
        { /* missed exons - find next block if we had regions that didn't align*/
        if(nextEndIndex < (gp->exonCount)-1)
            {
            nextEndIndex++;
            nextStart = (gp->exonStarts[nextEndIndex]);
            nextEnd = (gp->exonEnds[nextEndIndex]);
            gpSyn->txStart = qPtr;	/* Transcription start position */
            tClass = INTRON;
            qClass = INTRON;
            }
        }
    while ((tPtr > nextStart ) && (nextEndIndex <= (gp->exonCount)-1) && tPtr < txEnd)
        { /* starting in the middle of an exon - yuck*/
        if((nextEndIndex+1) < (gp->exonCount))
            {
            nextStart = (gp->exonStarts[nextEndIndex+1]);
            gpSyn->txStart = qPtr;	/* Transcription start position */
            if (tPtr > cStart)
                {
                gpSyn->cdsStart = qPtr;
                tCoding = TRUE;
                }
            tClass = INTRON;
            qClass = INTRON;
            }
        else
            {
            gpSyn->txStart = qPtr;	/* Transcription start position */
            break;
            }
        } 
    prevQptr = qPtr;
    /*
    if (!posStrand)
        {
        qPtr = axt->qEnd;
        tPtr = axt->tEnd; *index into axt file for gene *
        if (tPtr < nextEnd)
            {
            nextEndIndex--;
            if(nextEndIndex >= gp->exonCount)
                printf("nextEndIndex = %d gp exons %d\n", nextEndIndex , gp->exonCount);
            assert(nextEndIndex < gp->exonCount);
            assert(nextEndIndex >= 0);
            nextStart = (gp->exonEnds[nextEndIndex]);
            nextEnd = (gp->exonStarts[nextEndIndex]);
            }
        }
*/
    while (sizeLeft > 0)
        {
        if (posStrand || !posStrand)
            { /* this next section sets the transcription start */
            if ((tClass == INTRON) && (tPtr >= nextStart)  && (tPtr >= cStart) && (tPtr < cEnd))        
                {
                tCoding = TRUE;
                if (qStopCodon == FALSE)
                    {
                    qCoding = TRUE;
                    qCodonPos = tCodonPos; /* put trans back in sync */
                    }
                }
            else if ((tPtr >= nextStart ) && (tPtr <= cStart ))
                /* start of UTR 5'*/
                {
                if (tClass == INTRON || tClass == INTERGENIC)
                    //assert(nextEndIndex+1 < gp->exonCount);
                    //nextStart = gp->exonStarts[nextEndIndex+1];
                    {
                    gpSyn->txStart = qPtr;	/* Transcription start position */
                    //assert(nextEndIndex+1 < gp->exonCount);
                    //nextStart = gp->exonStarts[nextEndIndex+1];
                    }
                tClass = UTR5; qClass = UTR5;
                }
            }
        else
            /* gene on neg strand */
            {
            if (((tClass == UTR5) || (tClass == INTRON)) && (tPtr >= nextStart) && (tPtr >= cStart) && (tPtr < cEnd))        
                {
                tCoding = TRUE;
                if (qStopCodon == FALSE)
                    {
                    qCoding = TRUE;
                    qCodonPos = tCodonPos; /* put trans back in sync */
                    }
                }
            else if ((tPtr >= nextStart-1 ) && (tPtr > cStart ))
                /* start of UTR 5'*/
                {
                if (tClass == INTRON)
                    {
                    //gpSyn->txEnd = qPtr;	/* Transcription start position */
                    }
                tClass = UTR5; qClass = UTR5;
                }
            }
            if (posStrand || !posStrand)
                {/* this is crucial, maps exons (regardless of coding or not) to syntenic region */
                if (tPtr == nextStart)
                    {
                    prevQptr = qPtr; /* remember this for when you hit the end of exon, prevents 1/2 exon */
                    }
                if (tPtr == nextEnd)
                    {
                    tCoding=FALSE;
                    qCoding=FALSE;
                    if (tClass != UTR3)
                        {
                        tClass=INTRON;
                        qClass=INTRON;
                        }
                    if(gpSyn->exonCount < gp->exonCount)
                        {
                        assert(gpSyn->exonCount < gp->exonCount);
                        eStarts[gpSyn->exonCount] = prevQptr;
                        if (qPtr-prevQptr > filter)
                            {
                            fprintf(stderr,"exon %d larger than %d %d-%d %s from %s:%d-%d axt %c to %s:%d-%d %d-%d %s.\n",gp->exonCount,filter, prevQptr, qPtr, gp->name,gp->chrom, gp->txStart,gp->txEnd, axtFound->qStrand, gpSyn->chrom, gpSyn->txStart,gp->txEnd, gpSyn->cdsStart, gpSyn->cdsEnd, gpSyn->strand);
                            return;
                            }
                        eEnds[gpSyn->exonCount] = qPtr;
                        nextEndIndex++;
                        gpSyn->exonCount++;	/* Number of exons */
                        assert(nextEndIndex >= 0);
                        if(nextEndIndex < gp->exonCount)
                            {
                            nextStart = gp->exonStarts[nextEndIndex];
                            nextEnd = gp->exonEnds[nextEndIndex];
                            }
                        }
                    }
                }
            else
                {
                if (tPtr == nextStart-1)
                    {
                    prevQptr = qPtr;
                    }
                if (tPtr == nextEnd-1)
                    {
                    tCoding=FALSE;
                    qCoding=FALSE;
                    tClass=INTRON;
                    qClass=INTRON;
                    nextEndIndex--;
                    assert(nextEndIndex < gp->exonCount);
                    assert(nextEndIndex >= 0);
                    nextStart = (gp->exonEnds[nextEndIndex]);
                    nextEnd = (gp->exonStarts[nextEndIndex]);
                    assert(gpSyn->exonCount < gp->exonCount);
	                eStarts[gpSyn->exonCount] = qPtr;
	                eEnds[gpSyn->exonCount] = prevQptr;
                    gpSyn->exonCount++;	/* Number of exons */
                    }
                }
            if (posStrand || !posStrand)
                { /* handle start/stop coding */
                if ((tPtr >= (cStart)) && (tPtr <=(cStart+2)))
                    {
                    tClass=STARTCODON;
                    qClass=STARTCODON;
                    tCoding=TRUE;
                    qCoding=TRUE;
                    gpSyn->cdsStart = qPtr-2;	/* coding start position */
                    assert(gpSyn->cdsStart > 0);
                    if (tPtr == cStart) 
                        {
                        tCodonPos=1;
                        qCodonPos=1;
                        }
                    }
                if ((tPtr >= cEnd) && (tPtr <= (cEnd+2)))
                    {
                    tClass=STOPCODON;
                    qClass=STOPCODON;
                    gpSyn->cdsEnd = qPtr-1;	/* coding start position */
                    tCoding=FALSE;
                    qCoding=FALSE;
                    }
                }
            else
                {
                if ((tPtr <= (cStart)) && (tPtr >=(cStart-2)))
                    {
                    tClass=STARTCODON;
                    qClass=STARTCODON;
                    gpSyn->cdsStart = qPtr;	/* coding start position */
                    tCoding=TRUE;
                    qCoding=TRUE;
                    if (tPtr == cStart-3) 
                        {
                        tCodonPos=1;
                        qCodonPos=1;
                        }
                    }
                if ((tPtr <= cEnd+1) && (tPtr >= (cEnd-1)))
                    {
                    tClass=STOPCODON;
                    tCoding=FALSE;
                    }
                }
            if (posStrand || !posStrand)
                { /* handle end of transcription */
                if (tPtr == (cEnd +3) )
                    {
                    tClass = UTR3;
                    qClass = UTR3;
                    }
                if ((tPtr >= txEnd) && (tClass == UTR3))
                    {
                    gpSyn->txEnd = qPtr-3;	/* Transcription end position */
                    tClass = INTERGENIC;
                    qClass = INTERGENIC;
                    }
                }
            else 
                {
                if (tPtr == (cEnd -2) )
                    {
                    tClass = UTR3;
                    qClass = UTR3;
                    }
                if ((tPtr > txEnd) && (tClass == UTR3))
                    {
                    gpSyn->txStart = qPtr;	/* Transcription end position */
                    tClass = INTERGENIC;
                    qClass = INTERGENIC;
                    }
                }

            if (t[i] != '-')
                {/* don't count gaps */
                if (posStrand || !posStrand)
                    {
                    tPtr++;
                    }
                else
                    {
                    tPtr--;
                    }
                if (tCoding) 
                    {
                    tCodonPos++;
                    }
                if (tCodonPos>3) tCodonPos=1;
                }
            if (q[i] != '-')
                {
                if (posStrand || !posStrand)
                    qPtr++;
                else
                    qPtr--;
                if (qCoding) 
                    {
                    qCodonPos++;
                    }
                if (qCodonPos>3) qCodonPos=1;
                }

            sizeLeft -= oneSize; /* this is now 1, left over from parsing lines */
            i++;

            }
        lastChance = qPtr ; /*save this value in case there are no more alignments */
        } /* end of axt file */
if (gpSyn->exonCount == 0)
    {
    fprintf(stderr,"No alignment found for %s %s:%d-%d.\n",gp->name,gp->chrom, gp->txStart,gp->txEnd);
    return;
    }

if (posStrand || !posStrand)
    {
    if (gpSyn->exonStarts[0] <= 0 ||  (gpSyn->exonEnds[0] <= 0) )
        fprintf(stderr,"%s is wrong\n",gpSyn->name);
    assert(gpSyn->exonStarts[0] > 0);
    assert(gpSyn->exonEnds[0] > 0);
    if (gpSyn->cdsStart == 0) gpSyn->cdsStart = gpSyn->exonStarts[0];
    if (gpSyn->cdsEnd == 0) gpSyn->cdsEnd = gpSyn->exonEnds[(gpSyn->exonCount)-1];
    if (gpSyn->txEnd != gpSyn->exonEnds[(gpSyn->exonCount)-1]) 
        gpSyn->txEnd = gpSyn->exonEnds[(gpSyn->exonCount)-1] ;
    }
    if (gpSyn->txEnd == 0) gpSyn->txEnd = lastChance; //gpSyn->exonEnds[(gpSyn->exonCount)-1];
    if (gpSyn->txStart == 0)
        gpSyn->txStart = gpSyn->exonStarts[0] ;
if (gpSyn->txStart <= 0 ||  (gpSyn->txEnd <= 0) || (gpSyn->cdsStart <= 0) || (gpSyn->cdsEnd <= 0) )
    fprintf(stderr,"%s is wrong\n",gpSyn->name);
assert(gpSyn->txStart > 0);
assert(gpSyn->txEnd > 0);
assert(gpSyn->cdsStart > 0);
assert(gpSyn->cdsEnd > 0);
if (gpSyn->txEnd < gpSyn->txStart || gpSyn->cdsEnd < gpSyn->cdsStart)
    {
    fprintf(stderr,"Gene Prediction backwards %d %s from %s:%d-%d axt %c to %s:%d-%d %d-%d %s.\n",filter, gp->name,gp->chrom, gp->txStart,gp->txEnd, axtFound->qStrand, gpSyn->chrom, gpSyn->txStart,gp->txEnd, gpSyn->cdsStart, gpSyn->cdsEnd, gpSyn->strand);
    }
else if (gpSyn->txEnd - gpSyn->txStart < filter && gpSyn->cdsEnd - gpSyn->cdsStart < filter) 
    {
    fprintf(stderr,"OK %s from %s:%d-%d %s axt %c to %s:%d-%d %d-%d %s.\n", gp->name,gp->chrom, gp->txStart,gp->txEnd, gp->strand, axtFound->qStrand, gpSyn->chrom, gpSyn->txStart,gpSyn->txEnd,gpSyn->cdsStart, gpSyn->cdsEnd, gpSyn->strand); 
    gffWrite(gpSyn, stdout);
    }
else
    {
    fprintf(stderr,"Gene Prediction larger than %d %s from %s:%d-%d axt %c to %s:%d-%d %d-%d %s.\n",filter, gp->name,gp->chrom, gp->txStart,gp->txEnd, axtFound->qStrand, gpSyn->chrom, gpSyn->txStart,gp->txEnd, gpSyn->cdsStart, gpSyn->cdsEnd, gpSyn->strand);
    }
/*genePredFree(&gpSyn);*/
}

void chainPrintHead(struct chain *chain, FILE *f, char *ortholog, char *name, char *type, double score, int level)
/* output a map file similar to psl */
{
if (chain->qStrand == '+')
    fprintf(f, "%s\t%s\t%d\t%s\t%d\t+\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\t%1.0f\t%s\n", 
        name, type, level, chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
        chain->id,score, ortholog);
else
    fprintf(f, "%s\t%s\t%d\t%s\t%d\t+\t%d\t%d\t%s\t%d\t%c\t%d\t%d\t%d\t%1.0f\t%s\n", 
        name, type, level, chain->tName, chain->tSize, chain->tStart, chain->tEnd,
        chain->qName, chain->qSize, chain->qStrand, chain->qSize - chain->qEnd, chain->qSize - chain->qStart,
        chain->id,score, ortholog);
}

char *mapOrtholog(struct sqlConnection *conn,  char *geneTable, char *chrom, int gStart, int gEnd)
/* finds gene that overlaps syntenic projection */
{
struct sqlResult *sr;
char **row;
char query[512];
static char retName[255];
char *tFieldName = optionVal("tName", "name");

    sprintf(query, "select %s, txStart, txEnd from %s where chrom = '%s' and txStart <= %d and txEnd >= %d",tFieldName, geneTable,chrom,gEnd, gStart);
    /*printf("%s\n",query);*/
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *name = (row[0]);
        sprintf(retName, "%s",name);
        sqlFreeResult(&sr);
        return(retName);
        }
    sqlFreeResult(&sr);
    return(NULL);
}

void mustGetMrnaStartStop( char *acc, unsigned *cdsStart, unsigned *cdsEnd )
{
	char query[256];
	struct sqlConnection *conn;
	struct sqlResult *sr;
	char **row;

	/* Get cds start and stop, if available */
	conn = hAllocConn(db);
	sprintf(query, "select cds from mrna where acc = '%s'", acc);
	sr = sqlGetResult(conn, query);
	assert((row = sqlNextRow(sr)) != NULL);
       	sprintf(query, "select name from cds where id = '%d'", atoi(row[0]));
    	sqlFreeResult(&sr);
       	sr = sqlGetResult(conn, query);
    	assert((row = sqlNextRow(sr)) != NULL);
        genbankParseCds(row[0], cdsStart, cdsEnd);
	sqlFreeResult(&sr);
    hFreeConn(&conn);
}

void subsetChain(char *geneTable, char *netTable, char *chrom, struct hash *chainHash, struct axt *axtList)
/* Return subset ofchain at syntenic location. */
{
struct genePred *gp = NULL;
char *fieldName = optionVal("name", "name");
char *syntenicGene = optionVal("track", "knownGene");
boolean psl = optionExists("psl");
boolean gff = optionExists("gff");
struct chain *aChain;
struct sqlConnection *conn = hAllocConn(db);
struct sqlConnection *conn2 = hAllocConn(db2);
struct sqlResult *sr;
char **row;
char query[512];
char prevName[255] = "x";
char *ortholog = NULL;
struct chain *retSubChain;
struct chain *retChainToFree;
/*int chainArr[MAXCHAINARRAY];*/
filter = optionInt("filter",2000000);

                verifyAlist(axtList);
if (psl)
    sprintf(query, "select g.qName, g.tName, g.strand, g.tStart, g.tEnd, g.qStart, g.qEnd, g.blockCount, g.tStarts, g.blockSizes, g.%s,n.chainId, n.type, n.level, n.qName, n.qStart, n.qEnd, g.qSize, g.qStarts, g.tSize from %s g, %s n where n.tName = '%s' and n.tStart <= g.tEnd and n.tEnd >=  g.tStart and g.tName = '%s' and type <> 'gap' order by g.%s, g.tStart, score desc",fieldName, geneTable,netTable, chrom,chrom, fieldName );
else
    sprintf(query, "select g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds,  g.%s, n.chainId, n.type, n.level, n.qName, n.qStart, n.qEnd from %s g, %s n where n.tName = '%s' and n.tStart <= g.txEnd and n.tEnd >=  g.txStart and chrom = '%s' and n.type <> 'gap' order by g.%s, g.txStart, score desc",fieldName, geneTable,netTable, chrom,chrom, fieldName );
//fprintf(stderr,"query\n%s\n",query);
sr = sqlGetResult(conn, query);

                verifyAlist(axtList);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int geneStart = sqlUnsigned(row[3]);
    int geneEnd = sqlUnsigned(row[4]);
    char *name = row[10];
    int chainId = sqlUnsigned(row[11]);
    int level = sqlUnsigned(row[13]);
    int sizeOne;
    char chainIdStr[128];
    unsigned int cdsStart, cdsEnd;

    struct psl *p = NULL;

    verifyAlist(axtList);
    safef(chainIdStr,sizeof(chainIdStr),"%d",chainId);
    /*gp = genePredLoad(row + hasBin);*/
    AllocVar(gp);
    AllocVar(p);

    if(psl)
    {
        p->qName = cloneString(row[0]);
	    p->tName = cloneString(row[1]);
	    strcpy(p->strand, cloneString(row[2]));
	    p->tStart = sqlUnsigned(row[3]);
	    p->tEnd = sqlUnsigned(row[4]);
	    p->qStart = sqlUnsigned(row[5]);
	    p->qEnd = sqlUnsigned(row[6]);
    	p->blockCount = sqlUnsigned(row[7]);
	    sqlUnsignedDynamicArray(row[8], &p->tStarts, &sizeOne );
	    assert( sizeOne == p->blockCount );
	    sqlUnsignedDynamicArray(row[9], &p->blockSizes, &sizeOne );
	    assert( sizeOne == p->blockCount );
	    sqlUnsignedDynamicArray(row[18], &p->qStarts, &sizeOne );
	    assert( sizeOne == p->blockCount );
        p->qSize = sqlUnsigned(row[17]);
        p->tSize = sqlUnsigned(row[19]);

	    //get mRNA start and stop positions and fail otherwise.
	    fprintf(stderr,"\n### %s\t", p->qName);
	    mustGetMrnaStartStop(p->qName, &cdsStart, &cdsEnd);
	    fprintf(stderr, "%d\t%d ###\n", cdsStart, cdsEnd);

	    //convert to genePred
	    gp = genePredFromPsl( p, cdsStart, cdsEnd, INSERT_MERGE_SIZE );
    }
    else //input gene table is a gff not a psl
    {
    	gp->exonCount = sqlUnsigned(row[7]);
    	gp->name = cloneString(row[0]);
    	gp->chrom = cloneString(row[1]);
    	strcpy(gp->strand, row[2]);
    	gp->txStart = sqlUnsigned(row[3]);
    	gp->txEnd = sqlUnsigned(row[4]);
    	gp->cdsStart = sqlUnsigned(row[5]);
    	gp->cdsEnd = sqlUnsigned(row[6]);
    	sqlUnsignedDynamicArray(row[8], &gp->exonStarts, &sizeOne);
    	assert(sizeOne == gp->exonCount);
    	sqlUnsignedDynamicArray(row[9], &gp->exonEnds, &sizeOne);
    	assert(sizeOne == gp->exonCount);
    }

    verifyAlist(axtList);

    if ((atoi(row[11]) != 0) && strcmp(prevName,name))
        {
        /*printf("name %s prev %s\n",name, prevName);*/
        aChain = hashMustFindVal(chainHash, chainIdStr);
        fprintf(stderr," chain %d ",aChain->id);
        chainSubsetOnT(aChain, geneStart, geneEnd, &retSubChain,  &retChainToFree);
        if(retSubChain != NULL)
            {
            assert(atoi(row[11]) > 0);
            assert(atoi(row[3]) > 0);
            assert(atoi(row[4]) > 0);
            /* if -track option on then look for an ortholog */
            if (syntenicGene)
                {
                if (retSubChain->qStrand == '+')
                    ortholog = mapOrtholog(conn2, syntenicGene, retSubChain->qName, retSubChain->qStart, retSubChain->qEnd );
                else
                    ortholog = mapOrtholog(conn2, syntenicGene, retSubChain->qName, retSubChain->qSize - retSubChain->qEnd, retSubChain->qSize - retSubChain->qStart );
                }
            if (gff)
                {
                assert(gp->cdsStart > 0);
                assert(gp->cdsEnd > 0);
                assert(gp->txStart > 0);
                assert(gp->txEnd > 0);
                verifyAlist(axtList);
                if (gp->strand[0] == '+')
                    generateGff(retSubChain, stdout, ortholog, name, row[12], aChain->score, level, gp, axtList);
                else
                    generateGff(retSubChain, stdout, ortholog, name, row[12], aChain->score, level, gp, axtList);
                verifyAlist(axtList);
                }
            else
                chainPrintHead(retSubChain, stdout, ortholog, name, row[12], aChain->score, level);
            }
        else
            {
            /* gap in net/chain skip to next level */
            chainFree(&retChainToFree);
            continue;
            }
        chainFree(&retChainToFree);
        }
    sprintf(prevName , "%s",name);
    /*genePredFree(&gp);
    gp = NULL;*/
                verifyAlist(axtList);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
}

void orthologBySyntenyChrom(char *geneTable, char *netTable, char *chrom, struct hash *chainHash)
{
    struct axt *axtList = readAllAxt(chrom,"Blastz Chained");
    subsetChain(geneTable, netTable, chrom, chainHash, axtList);

}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;

optionHash(&argc, argv);
if (argc != 7)
    usage();
db = argv[1];
db2 = argv[2];
chainHash = allChains(argv[6]);
orthologBySyntenyChrom(argv[3], argv[4], argv[5], chainHash);
return 0;
}
