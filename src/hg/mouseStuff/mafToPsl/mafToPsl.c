/* mafToPsl - Convert maf to psl format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: mafToPsl.c,v 1.1 2003/08/17 20:43:24 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToPsl - Convert maf to psl format\n"
  "usage:\n"
  "   mafToPsl in.maf out.psl\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
	hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

void countInserts(char *s, int size, int *retNumInsert, int *retBaseInsert)
/* Count up number and total size of inserts in s. */
{
char c, lastC = s[0];
int i;
int baseInsert = 0, numInsert = 0;
if (lastC == '-')
    errAbort("%s starts with -", s);
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == '-')
        {
	if (lastC != '-')
	     numInsert += 1;
	baseInsert += 1;
	}
    lastC = c;
    }
*retNumInsert = numInsert;
*retBaseInsert = baseInsert;
}

int countInitialChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int count = 0;
char d;
while ((d = *s++) != 0)
    {
    if (c == d)
        ++count;
    else
        break;
    }
return count;
}

int countNonInsert(char *s, int size)
/* Count number of characters in initial part of s that
 * are not '-'. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (*s++ != '-')
        ++count;
return count;
}

int countTerminalChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int len = strlen(s), i;
int count = 0;
for (i=len-1; i>=0; --i)
    {
    if (c == s[i])
        ++count;
    else
        break;
    }
return count;
}
boolean isUpperCase(char c)
{
if ((int)c >= (int)'A' && (int)c <= (int)'Z')
   return TRUE;
return FALSE;
}

void aliStringToPsl(char *qName, char *tName, 
	char *qString, char *tString, 
	int qSize, int tSize, int aliSize, 
	int qStart, int qEnd, int tStart, int tEnd, char strand, FILE *f)
/* Output alignment in a pair of strings with insert chars
 * to a psl line in a file. */
{
unsigned match = 0;	/* Number of bases that match */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned repMatch = 0;	/* Number of bases that match and are repeats */
unsigned nCount = 0;	/* Number of bases that are gaps */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
boolean qInInsert = FALSE; /* True if in insert state on query. */
boolean tInInsert = FALSE; /* True if in insert state on target. */
boolean eitherInsert = FALSE;	/* True if either in insert state. */
int blockCount = 1, blockIx=0;
boolean qIsRc = FALSE;
int i;
char q,t;
int qs,qe,ts,te;
int *blocks = NULL, *qStarts = NULL, *tStarts = NULL;

/* Fix up things for ones that end or begin in '-' */
    {
    int qStartInsert = countInitialChars(qString, '-');
    int tStartInsert = countInitialChars(tString, '-');
    int qEndInsert = countTerminalChars(qString, '-');
    int tEndInsert = countTerminalChars(tString, '-');
    int startInsert = max(qStartInsert, tStartInsert);
    int endInsert = max(qEndInsert, tEndInsert);
    int qNonCount, tNonCount;

    if (startInsert > 0)
        {
	qNonCount = countNonInsert(qString, startInsert);
	tNonCount = countNonInsert(tString, startInsert);
	qString += startInsert;
	tString += startInsert;
	aliSize -= startInsert;
	qStart += qNonCount;
	tStart += tNonCount;
	}
    if (endInsert > 0)
        {
	aliSize -= endInsert;
	qNonCount = countNonInsert(qString+aliSize, endInsert);
	tNonCount = countNonInsert(tString+aliSize, endInsert);
	qString[aliSize] = 0;
	tString[aliSize] = 0;
        qEnd -= qNonCount;
        tEnd -= tNonCount;
	}
    }

/* Don't ouput if either query or target is zero length */
 if ((qStart == qEnd) || (tStart == tEnd))
     return;
/* First count up number of blocks and inserts. */
countInserts(qString, aliSize, &qNumInsert, &qBaseInsert);
countInserts(tString, aliSize, &tNumInsert, &tBaseInsert);
blockCount = 1 + qNumInsert + tNumInsert;

/* Count up match/mismatch/repMatch. */
for (i=0; i<aliSize; ++i)
    {
    q = qString[i];
    t = tString[i];
    if (q != '-' && t != '-')
	{
	if (q == 'N' || t == 'N' )
            ++nCount;
        else if (q == t && isUpperCase(q) && isUpperCase(t))
	    ++match;
	else if (toupper(q) == toupper(t))
            ++repMatch;
        else
	    ++misMatch;
	}
    }

/* Deal with minus strand. */
qs = qStart;
qe = qStart + match + misMatch + repMatch + tBaseInsert + nCount;
if (qe != qEnd)
   {
    printf("mismatch qe-qs %d tIns %d qe %d qEnd %d %s qStart %d match %d misMatch %d repmatch %d gaps %d \nmismatch te-ts %d qIns %d\n",
            qEnd-qStart, tBaseInsert, qe, qEnd,qName ,qStart , match , misMatch, repMatch , nCount,  tEnd-tStart, qBaseInsert);
    printf("Q %s\nT %s\n",qString, tString);
   }
assert(abs(qe-qEnd)<=1); 
if (strand == '-')
    {
    reverseIntRange(&qs, &qe, qSize);
    }
assert(qs < qe);
te = tStart + match + misMatch + repMatch + qBaseInsert + nCount;
if (te != tEnd)
   {
    printf("mismatch te-ts %d qIns %d te %d tEnd %d %s tStart %d match %d misMatch %d repmatch %d gaps %d \n         qe-qs %d tIns %d\n",
            tEnd-tStart, qBaseInsert, te, tEnd,tName ,tStart , match , misMatch, repMatch , nCount,  qEnd-qStart, tBaseInsert);
    printf("Q %s\nT %s\n",qString, tString);
   }
assert(abs(te-tEnd) <= 1);
assert(tStart < te);

/* Output header */
fprintf(f, "%d\t", match);
fprintf(f, "%d\t", misMatch);
fprintf(f, "%d\t", repMatch);
fprintf(f, "%d\t" ,nCount);
fprintf(f, "%d\t", qNumInsert);
fprintf(f, "%d\t", qBaseInsert);
fprintf(f, "%d\t", tNumInsert);
fprintf(f, "%d\t", tBaseInsert);
fprintf(f, "%c\t", strand);
fprintf(f, "%s\t", qName);
fprintf(f, "%d\t", qSize);
fprintf(f, "%d\t", qs);
fprintf(f, "%d\t", qe);
fprintf(f, "%s\t", tName);
fprintf(f, "%d\t", tSize);
fprintf(f, "%d\t", tStart);
fprintf(f, "%d\t", te);
fprintf(f, "%d\t", blockCount);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }

/* Allocate dynamic memory for block lists. */
AllocArray(blocks, blockCount);
AllocArray(qStarts, blockCount);
AllocArray(tStarts, blockCount);

/* Figure block sizes and starts. */
eitherInsert = FALSE;
qs = qe = qStart;
ts = te = tStart;
for (i=0; i<aliSize; ++i)
    {
    q = qString[i];
    t = tString[i];
    if (q == '-' || t == '-')
        {
	if (!eitherInsert)
	    {
	    blocks[blockIx] = qe - qs;
	    qStarts[blockIx] = qs;
	    tStarts[blockIx] = ts;
	    ++blockIx;
	    eitherInsert = TRUE;
	    }
	else if (i > 0)
	    {
	    /* Handle cases like
	     *     aca---gtagtg
	     *     acacag---gtg
	     */
	    if ((q == '-' && tString[i-1] == '-') || 
	    	(t == '-' && qString[i-1] == '-'))
	        {
		blocks[blockIx] = 0;
		qStarts[blockIx] = qe;
		tStarts[blockIx] = te;
		++blockIx;
		}
	    }
	if (q != '-')
	   qe += 1;
	if (t != '-')
	   te += 1;
	}
    else
        {
	if (eitherInsert)
	    {
	    qs = qe;
	    ts = te;
	    eitherInsert = FALSE;
	    }
	qe += 1;
	te += 1;
	}
    }
if(blockIx > blockCount-1)
    printf("blocks %d qNumInsert + tNumInsert %d aliSize %d\n",blockIx, qNumInsert + tNumInsert, aliSize);
assert((blockIx - (blockCount-1)) <= 0);
blocks[blockIx] = qe - qs;
qStarts[blockIx] = qs;
tStarts[blockIx] = ts;

/* Output block sizes */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", blocks[i]);
fprintf(f, "\t");

/* Output qStarts */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", qStarts[i]);
fprintf(f, "\t");

/* Output tStarts */
for (i=0; i<blockCount; ++i)
    fprintf(f, "%d,", tStarts[i]);
fprintf(f, "\n");

/* Clean Up. */
freez(&blocks);
freez(&qStarts);
freez(&tStarts);
}


void mafToPsl(char *inName, char *outName)
/* mafToPsl - Convert maf to psl format. */
{
struct mafFile *mp = mafOpen(inName);
FILE *f = mustOpen(outName, "w");
struct mafAli *maf;
struct mafComp *comp, *ref;

while ((maf = mafNext(mp)) != NULL)
    {
    int size = maf->textSize;
    ref = maf->components;
    assert(ref->next != NULL);
    for (comp = ref->next; comp != NULL; comp = comp->next)
    aliStringToPsl(comp->src, ref->src, comp->text, ref->text,
    	comp->srcSize,  ref->srcSize, maf->textSize, 
        comp->start, (comp->start)+comp->size, ref->start, (ref->start)+ref->size,
        comp->strand, f);
    mafAliFree(&maf);
    }
carefulClose(&f);
}

/*
{
struct mafFile *mp = mafOpen(in);
FILE *f = mustOpen(out, "w");
struct mafAli *ali;
struct mafComp *comp;
int i;

//axtWriteStart(f, "blastz");
while ((ali = mafNext(mp)) != NULL)
  {
    struct axt *axt;
    char srcName[128];
    AllocVar(axt);
    axt->score = ali->score;
    for (comp = ali->components; comp != NULL; comp = comp->next)
    {
	    if( startsWith( tName, comp->src ) )
	    {
		    axt->tName = strdup(strstr(comp->src, ".")+1);	
            axt->tStrand = comp->strand;
		    axt->tStart = comp->start;
		    axt->tEnd = comp->start + comp->size;
		    axt->tSym = comp->text;
		    comp->text = NULL;


            if( axt->tStrand != '+' )
            {
                fprintf( stderr, "Target sequence not on positive
                        strand, as required\n");
                exit(1);
            }
	    }
	    else if( startsWith( qName, comp->src ))
	    {
                axt->qName = strdup(strstr( comp->src, "." )+1);	
                axt->qStrand = comp->strand;
                axt->qStart = comp->start;
                axt->qEnd = comp->start + comp->size;
                axt->qSym = comp->text;
                comp->text = NULL;

	    }
    }

    if( axt->tSym == NULL || axt->qSym == NULL )
        {
        axtFree(&axt);
        continue;
        }
    if (strlen( axt->tSym  ) == strlen( axt->qSym )) 
        axt->symCount = strlen( axt->tSym );
    else
    {
        fprintf( stderr, "Target and query sequences are different
                 lengths\n%s\n%s\n\n", axt->tSym, axt->qSym );
        axtFree(&axt);
        continue;
        //exit(1);
    }
    axtWrite(axt,f);
    axtFree(&axt);
   }

fclose(f);
}
*/
int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
mafToPsl(argv[1], argv[2]);
return 0;
}
