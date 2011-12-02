/* axtAffine - do alignment of two (shortish) sequences with
 * affine gap scoring, and return the result as an axt. 
 * This file is copyright 2000-2004 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "pairHmm.h"
#include "axt.h"



boolean axtAffineSmallEnough(double querySize, double targetSize)
/* Return TRUE if it is reasonable to align sequences of given sizes
 * with axtAffine. */
{
return targetSize * querySize <= 1.0E8;
}

static void affineAlign(char *query, int querySize, 
	char *target, int targetSize, struct axtScoreScheme *ss,
	struct phmmMatrix **retMatrix, struct phmmAliPair **retPairList,
	int *retScore)
/* Use dynamic programming to do alignment including affine gap
 * scores. */
{
struct phmmMatrix *a;
struct phmmState *hf, *iq, *it;
int qIx, tIx, sIx;  /* Query, target, and state indices */
int rowOffset, newCellOffset;
int bestScore = -0x4fffffff;
struct phmmMommy *bestCell = NULL;
int matchPair;
int gapStart, gapExt;

/* Check that it's not too big. */
if (!axtAffineSmallEnough(querySize, targetSize))
    errAbort("Can't align %d x %d, too big\n", querySize, targetSize);

gapStart = -ss->gapOpen;
gapExt = -ss->gapExtend;

/* Initialize 3 state matrix (match, query insert, target insert). */
a = phmmMatrixNew(3, query, querySize, target, targetSize);
hf = phmmNameState(a, 0, "match", 'M');
iq = phmmNameState(a, 1, "qSlip", 'Q');
it = phmmNameState(a, 2, "tSlip", 'T');

for (tIx = 1; tIx < a->tDim; tIx += 1)
    {
    UBYTE mommy = 0;
    int score, tempScore;

/* Macros to make me less mixed up when accessing scores from row arrays.*/
#define matchScore lastScores[qIx-1]
#define qSlipScore lastScores[qIx]
#define tSlipScore scores[qIx-1]
#define newScore scores[qIx]

/* Start up state block (with all ways to enter state) */
#define startState(state) \
   score = 0;

/* Define a transition from state while advancing over both
 * target and query. */
#define matchState(state, addScore) \
   { \
   if ((tempScore = state->matchScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, -1, -1); \
        score = tempScore; \
        } \
   } 

/* Define a transition from state while slipping query
 * and advancing target. */
#define qSlipState(state, addScore) \
   { \
   if ((tempScore = state->qSlipScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, 0, -1); \
        score = tempScore; \
        } \
   }

/* Define a transition from state while slipping target
 * and advancing query. */
#define tSlipState(state, addScore) \
   { \
   if ((tempScore = state->tSlipScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, -1, 0); \
        score = tempScore; \
        } \
   }

/* End a block of transitions into state. */
#define endState(state) \
    { \
    struct phmmMommy *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = phmmNullMommy; \
        score = 0; \
        } \
    newCell->mommy = mommy; \
    state->newScore = score; \
    if (score > bestScore) \
        { \
        bestScore = score; \
        bestCell = newCell; \
        } \
    } 

/* End a state that you know won't produce an optimal
 * final score. */
#define shortEndState(state) \
    { \
    struct phmmMommy *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = phmmNullMommy; \
        score = 0; \
        } \
    newCell->mommy = mommy; \
    state->newScore = score; \
    }


    rowOffset = tIx*a->qDim;
    for (qIx = 1; qIx < a->qDim; qIx += 1)
        {
        newCellOffset = rowOffset + qIx;
        
        /* Figure the cost or bonus for pairing target and query residue here. */
        matchPair = ss->matrix[(int)a->query[qIx-1]][(int)a->target[tIx-1]];

        /* Update hiFi space. */
            {
            startState(hf);
            matchState(hf, matchPair);
            matchState(iq, matchPair);
            matchState(it, matchPair);
            endState(hf);
            }

        /* Update query slip space. */
            {
            startState(iq);
            qSlipState(iq, gapExt);
            qSlipState(hf, gapStart);            
	    qSlipState(it, gapStart);	/* Allow double gaps, T first always. */
            shortEndState(iq);
            }
        
        /* Update target slip space. */
            {
            startState(it);
            tSlipState(it, gapExt);
            tSlipState(hf, gapStart);            
            shortEndState(it);
            }

        }
    /* Swap score columns so current becomes last, and last gets
     * reused. */
    for (sIx = 0; sIx < a->stateCount; ++sIx)
        {
        struct phmmState *as = &a->states[sIx];
        int *swapTemp = as->lastScores;
        as->lastScores = as->scores;
        as->scores = swapTemp;
        }
    }

/* Trace back from best scoring cell. */
*retPairList = phmmTraceBack(a, bestCell);
*retMatrix = a;
*retScore = bestScore;

#undef matchScore
#undef qSlipScore
#undef tSlipScore
#undef newScore
#undef startState
#undef matchState
#undef qSlipState
#undef tSlipState
#undef shortEndState
#undef endState
}

struct axt *axtAffine(bioSeq *query, bioSeq *target, struct axtScoreScheme *ss)
/* Return alignment if any of query and target using scoring scheme. */
{
struct axt *axt;
int score;
struct phmmMatrix *matrix;
struct phmmAliPair *pairList;

affineAlign(query->dna, query->size, target->dna, target->size, ss,
	&matrix, &pairList, &score);
axt = phhmTraceToAxt(matrix, pairList, score, query->name, target->name);
phmmMatrixFree(&matrix);
slFreeList(&pairList);
return axt;
}


/* ----- axtAffine2Level begins ----- 

 Written by Galt Barber, December 2004
 I wrote this on my own time and am donating this
 to the public domain.  The original concept 
 was Don Speck's, as described by Kevin Karplus.

 @article{Grice97,
     author = "J. A. Grice and R. Hughey and D. Speck",
     title = "Reduced space sequence alignment",
     journal = cabios,
     volume=13,
     number=1,
     year=1997,
     month=feb,
     pages="45-53"
     }
								 

*/

#define WORST 0xC0000000 /* WORST Score approx neg. inf. 0x80000000 overflowed, reduced by half */


/* m d i notation: match, delete, insert in query */

struct cell2L 
{
int  bestm;   /* best score array */
int  bestd;                         
int  besti;                          
char backm;   /* back trace array */
char backd;                         
char backi;                          
};

#ifdef DEBUG
void dump2L(struct cell2L* c)
/* print matrix cell for debugging 
   I redirect output to a file 
   and look at it with a web browser
   to see the long lines
*/
{
    printf("%04d%c %04d%c %04d%c   ",
     c->bestd, c->backd,
     c->bestm, c->backm,
     c->besti, c->backi
     );
}     
#endif

void kForwardAffine(
struct cell2L *cells,  /* dyn prg arr cells */
int row,      /* starting row base */
int rowmax,   /* ending row */
int rdelta,   /* convert between real targ seq row and logical row */                             
int cmost,    /* track right edge, shrink as traces back */
int lv,       /* width of array including sentinel col 0 */ 
char *q,      /* query and target seqs */
char *t,
struct axtScoreScheme *ss,  /* score scheme passed in */
int *bestbestOut,  /* return best overall found, and it's row and col */
int *bestrOut,
int *bestcOut,
char *bestdirOut
)
/*
Calculates filling dynprg mtx forward.
 Called 3 times from affine2Level.

row is offset into the actual best and back arrays,
 so rdelta serves as a conversion between
 the real target seq row and the logical row
 used in best and back arrays.

cmost is a column limiter that lets us avoid
 unused areas of the array when doing the
 backtrace 2nd pass. This can be an average
 of half of the total array saved.

*/
{
int r=0, rr=0;
int gapOpen  =ss->gapOpen;    
int gapExtend=ss->gapExtend;  
int doubleGap=ss->gapExtend;  // this can be gapOpen or gapExtend, or custom ?
struct cell2L *cellp,*cellc;  /* current and previous row base */
struct cell2L *u,*d,*l,*s;    /* up,diag,left,self pointers to hopefully speed things up */
int c=0;
int bestbest = *bestbestOut; /* make local copy of best best */
cellc = cells+(row-1)*lv;     /* start it off one row back coming into loop */

#ifdef DEBUG
for(c=0;c<=cmost;c++) /* show prev row */
    { dump2L(cellc+c); }
printf("\n");
#endif

for(r=row; r<=rowmax; r++)
    {
    cellp = cellc;
    cellc += lv;    /* initialize pointers to curr and prev rows */
    
    rr = r+rdelta;

    d = cellp;   /* diag is prev row, prev col */
    l = cellc;   /* left is curr row, prev col */
    u = d+1;     /*   up is prev row, curr col */
    s = l+1;     /* self is curr row, curr col */
    
    /* handle col 0 sentinel as a delete */
    l->bestm=WORST; 
    l->bestd=d->bestd-gapExtend;
    l->besti=WORST;                 
    l->backm='x';
    l->backd='d';
    l->backi='x';
    if (rr==1)    /* special case row 1 col 0 */
	{
	l->bestd=-gapOpen;
	l->backd='m';
	}
#ifdef DEBUG
    dump2L(cellc); 
#endif
    
    for(c=1; c<=cmost; c++)
	{

	int best=WORST;
	int try  =WORST;
	char dir=' ';
	/* note: is matrix symmetrical? if not we could have dim 1 and 2 backwards */
	int subst = ss->matrix[(int)q[c-1]][(int)t[rr-1]];  /* score for pairing target and query. */

	/* find best M match query and target */
	best=WORST;
	try=d->bestd;    
	if (try > best)
	    {
	    best=try;
	    dir='d';
	    }
	try=d->bestm;   
	if (try > best)
	    {
	    best=try;
	    dir='m';
	    }
	try=d->besti;   
	if (try > best)
	    {
	    best=try;
	    dir='i';
	    }
	try=0;                   /* local ali can start anywhere */
	if (try > best)
	    {
	    best=try;
	    dir='s';         
	    }
	best += subst;
	s->bestm = best;
	s->backm = dir;
	if (best > bestbest)
	    {
	    bestbest=best;
	    *bestbestOut=best;
	    *bestrOut=rr;
	    *bestcOut=c;
	    *bestdirOut=dir;
	    }

	/* find best D delete in query */
	best=WORST;
	try=u->bestd - gapExtend;
	if (try > best)
	    {
	    best=try;
	    dir='d';
	    }
	try=u->bestm - gapOpen;    
	if (try > best)
	    {
	    best=try;
	    dir='m';
	    }
	try=u->besti - doubleGap;    
	if (try > best)
	    {
	    best=try;
	    dir='i';
	    }
	s->bestd = best;
	s->backd = dir;
	if (best > bestbest)
	    {
	    bestbest=best;
	    *bestbestOut=best;
	    *bestrOut=rr;
	    *bestcOut=c;
	    *bestdirOut=dir;
	    }

	/* find best I insert in query */
	best=WORST;
	try=l->bestd - doubleGap;
	if (try > best)
	    {
	    best=try;
	    dir='d';
	    }
	try=l->bestm - gapOpen;    
	if (try > best)
	    {
	    best=try;
	    dir='m';
	    }
	try=l->besti - gapExtend;    
	if (try > best)
	    {
	    best=try;
	    dir='i';
	    }
	s->besti = best;
	s->backi = dir;
	if (best > bestbest)
	    {
	    bestbest=best;
	    *bestbestOut=best;
	    *bestrOut=rr;
	    *bestcOut=c;
	    *bestdirOut=dir;
	    }

#ifdef DEBUG
    dump2L(cellc+c); 
#endif

	d++;l++;u++;s++;

	}
#ifdef DEBUG
printf("\n");
#endif
  
    }
} 



struct axt *axtAffine2Level(bioSeq *query, bioSeq *target, struct axtScoreScheme *ss)
/* 

   (Moving boundary version, allows target T size twice as large in same ram)

   Return alignment if any of query and target using scoring scheme. 
   
   2Level uses an economical amount of ram and should work for large target sequences.
   
   If Q is query size and T is target size and M is memory size, then
   Total memory used M = 30*Q*sqrt(T).  When the target is much larger than the query
   this method saves ram, and average runtime is only 50% greater, or 1.5 QT.  
   If Q=5000 and T=245,522,847 for hg17 chr1, then M = 2.2 GB ram.  
   axtAffine would need M=3QT = 3.4 TB.
   Of course massive alignments will be painfully slow anyway.

   Works for protein as well as DNA given the correct scoreScheme.
  
   NOTES:
   Double-gap cost is equal to gap-extend cost, but gap-open would also work.
   On very large target, score integer may overflow.
   Input sequences not checked for invalid chars.
   Input not checked but query should be shorter than target.
   
*/
{
struct axt *axt=needMem(sizeof(struct axt));

char *q = query->dna;
char *t = target->dna;

int Q= query->size;
int T=target->size;
int lv=Q+1;                    /* Q+1 is used so often let's call it lv for q-width */
int lw=T+1;                    /* T+1 is used so often let's call it lw for t-height */


int r = 0;                                 /* row matrix index */
int c = 0;                                 /* col matrix index */
char dir=' ';                              /* dir for bt */
int bestbest = WORST;                      /* best score in entire mtx */

int k=0;                                   /* save every kth row (k decreasing) */
int ksize = 0;                             /* T+1 saved rows as ksize, ksize-1,...,1*/
int arrsize = 0;                           /* dynprg array size, +1 for 0 sentinel col. */
struct cell2L *cells = NULL;               /* best score dyn prog array */
int ki = 0;                                /* base offset into array */
int cmost = Q;                             /* track right edge shrinkage during backtrace */
int kmax = 0;                              /* rows range from ki to kmax */
int rr = 0;                                /* maps ki base to actual target seq */
int nrows = 0;                             /* num rows to do, usually k or less */
int bestr = 0;                             /* remember best r,c,dir for local ali */
int bestc = 0;           
char bestdir = 0;
int temp = 0;


char *btq=NULL;      /* temp pointers to track ends of string while accumulating */
char *btt=NULL;

ksize = (int) (-1 + sqrt(8*lw+1))/2;    
if (((ksize*(ksize+1))/2) < lw) 
    {ksize++;}
arrsize = (ksize+1) * lv;                 /* dynprg array size, +1 for lastrow that moves back up. */
cells = needLargeMem(arrsize * sizeof(struct cell2L));   /* best score dyn prog array */

#ifdef DEBUG
printf("\n k=%d \n ksize=%d \n arrsize=%d \n Q,lv=%d,%d T=%d \n \n",k,ksize,arrsize,Q,lv,T);
#endif

axt->next = NULL;
axt->qName = cloneString(query->name);
axt->tName = cloneString(target->name);
axt->qStrand ='+';
axt->tStrand ='+';
axt->frame = 0;
axt->score=0;
axt->qStart=0;
axt->tStart=0;
axt->qEnd=0;
axt->tEnd=0;
axt->symCount=0;
axt->qSym=NULL;
axt->tSym=NULL;

if ((Q==0) || (T==0))
    {
    axt->qSym=cloneString("");
    axt->tSym=cloneString("");
    freez(&cells);
    return axt; 
    }



/* initialize origin corner */
    cells[0].bestm=0;
    cells[0].bestd=WORST;
    cells[0].besti=WORST;                 
    cells[0].backm='x';
    cells[0].backd='x';
    cells[0].backi='x';
#ifdef DEBUG
    dump2L(cells); 
#endif

/* initialize row 0 col 1 */
    cells[1].bestm=WORST;
    cells[1].bestd=WORST;
    cells[1].besti=-ss->gapOpen;
    cells[1].backm='x';
    cells[1].backd='x';
    cells[1].backi='m';
#ifdef DEBUG
    dump2L(cells+1); 
#endif

/* initialize first row of sentinels */
for (c=2;c<lv;c++)
    {
    cells[c].bestm=WORST;
    cells[c].bestd=WORST;
    cells[c].besti=cells[c-1].besti-ss->gapExtend;
    cells[c].backm='x';
    cells[c].backd='x';
    cells[c].backi='i';
#ifdef DEBUG
    dump2L(cells+c); 
#endif
    }
#ifdef DEBUG
printf("\n");
printf("\n");
#endif

k=ksize;

ki++;  /* advance to next row */

r=1;   /* r is really the rows all done */
while(1)
    {
    nrows = k;  /* do k rows at a time, save every kth row on 1st pass */
    if (nrows > (lw-r)) {nrows=lw-r;}  /* may get less than k on last set */
    kmax = ki+nrows-1;

    kForwardAffine(cells, ki, kmax, r-ki, cmost, lv, q, t, ss, &bestbest, &bestr, &bestc, &bestdir);
#ifdef DEBUG
printf("\n");
#endif

    r += nrows;

    if (nrows == k)   /* got full set of k rows */
	{
	/* compress, save every kth row */     
	/* optimize as a mem-copy */
	memcpy(cells+ki*lv,cells+kmax*lv,sizeof(struct cell2L) *lv);    
	}

    if (r >= lw){break;} /* we are done */
    
    ki++;
    k--;        /* decreasing k is "moving boundary" */
}

#ifdef DEBUG
printf("\nFWD PASS DONE. bestbest=%d bestr=%d bestc=%d bestdir=%c \n\n",bestbest,bestr,bestc,bestdir);
#endif

/* start doing backtrace */
    
/* adjust for reverse pass */

/* for local we automatically skip to bestr, bestc to begin tb */

if (bestbest <= 0)  /* null alignment */
    {
    bestr=0;
    bestc=0;
    /* bestdir won't matter */
    }

r = bestr;
c = bestc;
dir = bestdir;
cmost = c;

axt->qEnd=bestc;
axt->tEnd=bestr;

temp = (2*ksize)+1;
ki = (int)(temp-sqrt((temp*temp)-(8*r)))/2;
rr = ((2*ksize*ki)+ki-(ki*ki))/2;
kmax = ki+(r-rr);
k = ksize - ki;


/* now that we jumped back into saved start-points,
   let's fill the array forward and start backtrace from there.
*/

#ifdef DEBUG
printf("bestr=%d, bestc=%d, bestdir=%c k=%d, ki=%d, kmax=%d\n",bestr,bestc,bestdir,k,ki,kmax);
#endif

kForwardAffine(cells, ki+1, kmax, rr-ki, cmost, lv, q, t, ss, &bestbest, &bestr, &bestc, &bestdir);
   
#ifdef DEBUG
printf("\n(initial)BKWD PASS DONE. cmost=%d r=%d c=%d dir=%c \n\n",cmost,r,c,dir);
#endif


/* backtrace */   

/* handling for resulting ali'd strings when very long */

axt->symCount=0;
axt->qSym = needLargeMem((Q+T+1)*sizeof(char));
axt->tSym = needLargeMem((Q+T+1)*sizeof(char));
btq=axt->qSym;
btt=axt->tSym;
while(1)
    {
    while(1)
	{
#ifdef DEBUG
	printf("bt: r=%d, c=%d, dir=%c \n",r,c,dir);
#endif

	
    	if ((r==0) && (c==0)){break;} /* hit origin, done */
	if (r<rr){break;} /* ran out of targ seq, backup and reload */
	if (dir=='x'){errAbort("unexpected error backtracing");} /* x only at origin */
	if (dir=='s'){break;}   /* hit start, local ali */
	if (dir=='m') /* match */
	    {
	    *btq++=q[c-1];  /* accumulate alignment output strings */
	    *btt++=t[r-1];  /* accumulate alignment output strings */
	    axt->symCount++; 
	    dir = cells[lv*(ki+r-rr)+c].backm;  /* follow backtrace */
	    r--;            /* adjust coords to move in dir spec'd by back ptr */
	    c--;
	    cmost--;        /* decreases as query seq is aligned, so saves on unused areas */
	    }
	else
	    {
	    if (dir=='d')  /* delete in query (gap) */
		{
		*btq++='-';     /* accumulate alignment output strings */
    		*btt++=t[r-1];  /* accumulate alignment output strings */
    		axt->symCount++; 
		dir = cells[lv*(ki+r-rr)+c].backd;  /* follow backtrace */
    		r--;            /* adjust coords to move in dir spec'd by back ptr */
		}
	    else    /* insert in query (gap) */
		{
		*btq++=q[c-1];  /* accumulate alignment output strings */
    		*btt++='-';     /* accumulate alignment output strings */
    		axt->symCount++; 
		dir = cells[lv*(ki+r-rr)+c].backi;  /* follow backtrace */
    		c--;
    		cmost--;        /* decreases as query seq is aligned, so saves on unused areas */
		}
	    }
	
	}

    /* back up and do it again */
    ki--;
    k++;   /* k grows as we move back up */ 
    rr-=k;
    kmax = ki+k-1;

    /* check for various termination conditions to stop main loop */
    if (ki < 0) {break;}
    if ((r==0)&&(c==0)) {break;}
    if (dir=='s') {break;}

    /* re-calculate array from previous saved kth row going back
       this is how we save memory, but have to regenerate half on average
       we are re-using the same call 
     */

#ifdef DEBUG
printf("bestr=%d, bestc=%d, bestdir=%c k=%d, ki=%d, kmax=%d\n",bestr,bestc,bestdir,k,ki,kmax);
#endif


    kForwardAffine(cells, ki+1, kmax, rr-ki, cmost, lv, q, t, ss, &bestbest, &bestr, &bestc, &bestdir);

#ifdef DEBUG
    printf("\nBKWD PASS DONE. cmost=%d r=%d c=%d\n\n",cmost,r,c);
#endif

    }

axt->qStart=c;
axt->tStart=r;

/* reverse backwards trace and zero-terminate strings */

reverseBytes(axt->qSym,axt->symCount);
reverseBytes(axt->tSym,axt->symCount);
axt->qSym[axt->symCount]=0;
axt->tSym[axt->symCount]=0;

axt->score=bestbest;


/* 
should I test stringsize and if massively smaller, realloc string to save ram? 
*/

freez(&cells);

return axt;
}



