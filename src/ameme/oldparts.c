void unpackTile(int tile, int tileSize, DNA *unpacked)
/* Unpack 2 base-per-nucleotide tile into unpacked. */
{
int i;
int t;

for (i = tileSize-1; i>= 0; i -= 1)
    {
    t = (tile & 0x3);
    tile >>= 2;
    unpacked[i] = valToNt[t];
    }
unpacked[tileSize] = 0;
}


int findRepeatAdjust(int tile, int tileSize)
/* Adjust for tile hitting multiple spots in a pattern. */
{
DNA unpacked[17];
int i;
DNA b;
boolean repeats = TRUE;

/* Unpack tile. */
assert(tileSize < sizeof(unpacked));
unpackTile(tile, tileSize, unpacked);


/* Check for whole tile being same base. */
repeats = TRUE;
b = unpacked[0];
for (i=1; i<tileSize; ++i)
    {
    if (unpacked[i] != b)
        {
        repeats = FALSE;
        break;
        }
    }
if (repeats)
    {
    return 1;
    }

/* Check for every other tile being same. */
repeats = TRUE;
for (i=2; i<tileSize; ++i)
    {
    if (unpacked[i] != unpacked[i&1])
        {
        repeats = FALSE;
        break;
        }
    }
if (repeats)
    {
    return 2;
    }

/* Check for every third tile being same. */
if (tileSize >= 6)
    {
    repeats = TRUE;
    for (i=3; i<tileSize; ++i)
        {
        if (unpacked[i] != unpacked[i%3])
            {
            repeats = FALSE;
            break;
            }
        }
    if (repeats)
        {
        return 3;
        }
    }

return 4;
}



void bestMatchInSeq(struct profile *prof, DNA *seq, int seqSize, double *retScore, int *retPos)

double power(double a, int b)
/* Returns a raised to an integer b power. */
{
double acc = 1.0;

if (b < 0)
    {
    b = -b;
    a = 1.0/a;
    }
while (--b >= 0)
    acc *= a;
return acc;
}


double runStartTile(struct startTile *tile, struct seqList *seqList, double logFreq[4],
    double improbThreshold)
/* Run start tile over sequence list and score it. */
{
int i;
DNA *tileDna = tile->seq, *seqDna, *endSeq;
int tileSize = tile->size, seqSize;
struct seqList *seqEl;
struct dnaSeq *seq;
double tileScore = 0;

for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    seq = seqEl->seq;
    seqDna = seq->dna;
    seqSize = seq->size;
    endSeq = seqDna + seqSize;
    for (; seqDna < endSeq; seqDna++)
        {
        double oneProb = 0;
        for (i=0; i<tileSize; ++i)
            {
            if (tileDna[i] == seqDna[i])
                {
                oneProb += logFreq[ntVal[tileDna[i]]];
                }
            }
        if (oneProb >= improbThreshold)
            {
            tileScore += oneProb;
            for (i=0; i<tileSize; ++i)
                {
                if (tileDna[i] == seqDna[i])
                    tile->counts[i] += 1;
                }
            ++tile->hitCount;
            }
        }
    }
return tileScore;
}

struct startTile *makeStartTiles(struct seqList *seqList, struct seqList *badSeq, double improbFactor)
/* Make a list of good starting tiles. */
{
double follow[5][5];
int i;
struct startTile *tileList = NULL, *tile;
struct hash *tileHash = newHash(12);
struct seqList *seqEl;
struct dnaSeq *seq;
DNA *dna;
int basesLeft;
DNA *oligo;
int oligoSize;
double oligoProb;
double runProbThreshold = -0.75*log(improbFactor);
double goodScore, badScore;

makeFreqTable(seqList, freq);
makeFollowTable(seqList, follow);
uglyf("A %f C %f G %f T %f\n", freq[A_BASE_VAL], freq[C_BASE_VAL],
    freq[G_BASE_VAL], freq[T_BASE_VAL]);

for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    uglyf("Searching %s for start tiles\r", seqEl->seq->srn->name);
    seq = seqEl->seq;
    dna = seq->dna;
    basesLeft = seq->size;
    while (ffFindGoodOligo(dna, basesLeft, improbFactor, freq, &oligo, &oligoSize, &oligoProb))
        {
        int nextOffset = oligo - dna + 1;
        char nbuf[maxStartTileSize+1];
        assert(oligoSize < sizeof(nbuf));
        memcpy(nbuf, oligo, oligoSize);
        nbuf[oligoSize] = 0;
        if (!hashLookup(tileHash, nbuf))
            {
            hashAdd(tileHash, nbuf, NULL);
            AllocVar(tile);
            tile->seq = oligo;
            tile->size = oligoSize;
            tile->improb = oligoProb;
            goodScore = runStartTile(tile, seqList, logFreq, runProbThreshold);
            if (badSeq)
                badScore = runStartTile(tile, badSeq, logFreq, runProbThreshold);
            else
                badScore = 0;
            tile->score = goodScore - badScore; 
            slAddHead(&tileList, tile);
            }
        dna += nextOffset;
        basesLeft -= nextOffset;        
        }
    }
freeHash(&tileHash);
slSort(&tileList, cmpStartScores);

    {
    int count = 0;
    for (tile = tileList; tile != NULL; tile = tile->next)
        {
        if (++count < 30 || tile->next == NULL)
            {
            char buf[maxStartTileSize+1];
            memcpy(buf, tile->seq, tile->size);
            buf[tile->size] = 0;
            uglyf("Tile #%d seq %s size %d hits %d score %f\n", count, buf, 
                tile->size, tile->hitCount, tile->score); 
            }
        }
    }

return tileList;
}

void displayTileMatch(struct startTile *tile, struct seqList *list, double improbFactor)
{
struct seqList *el;
struct dnaSeq *seq;
DNA *dna, *endDna, *tileDna = tile->seq;
int tileSize = tile->size;
int i;
double runProbThreshold = -0.75*log(improbFactor);
double logBase[5];
double *logFreq = logBase+1;
double oneProb;

makeFreqTable(list, freq);
logFreq[-1] = 0;
for (i=0; i<4; ++i)
    logFreq[i] = -log(freq[i]);

for (el = list; el != NULL; el = el->next)
    {
    char buf[256];
    seq = el->seq;
    assert(seq->size < sizeof(buf));
    memcpy(buf, seq->dna, seq->size+1);
    dna = buf;
    endDna = dna + seq->size - tileSize;
    for (;dna < endDna; ++dna)
        {
        oneProb = 0.0;
        for (i=0; i<tileSize; ++i)
            {
            if (tileDna[i] == dna[i])
                {
                oneProb += logFreq[ntVal[tileDna[i]]];
                }
            }
        if (oneProb >= runProbThreshold)
            {
            for (i=0; i<tileSize; ++i)
                {
                if (tileDna[i] == dna[i])
                    dna[i] = toupper(dna[i]);
                }
            }
        }
    printf("%s\n", buf);
    }
}


#ifdef OLD
if (cgiVarExists("startTile"))
    {
    struct profile *prof;
    startTile = cgiString("startTile");
    startPos = cgiInt("startPos");

    printf("Here is the <A HREF=#converge>convergence</A> of the initial pattern %s at %d. ",
           startTile, startPos);
    printf("The <A HREF=#hits>pattern hits</A> in the DNA sequences is at the end.");       
    htmlHorizontalLine();
    printf("<TT><PRE><A NAME=converge></A>\n");
    dnaFilter(startTile, startTile);
    prof = profileFromTile(startTile, strlen(startTile), startPos,  (int)averageSeqSize(goodSeq));
    printf("Converging profile:\n");
    prof->score = scoreProfile(prof, goodSeq, badSeq, NULL, FALSE);
    printProfile(prof);
    prof = convergeAndPrint(prof, 25);
    htmlHorizontalLine();
    printf("<A NAME=hits></A>\n");
    showProfHits(prof, goodSeq);
    }
else
    {
    puts("<H2 ALIGN=CENTER>Preliminary Results</H2>");
    puts("The sequence you input was broken into 'tiles' of six base pairs each. "
         "The parts of the sequence that are underlined below show some promise of "
         "being part of a pattern.  The tiles in this picture are constrained to be "
         "separated by at least one base. Clicking on a "
         "tile will run the Memish Locator algorithm iteratively with that tile as a "
         "starting point.  The algorithm will display it's step-by-step convergence, "
         "and the positions in your input that match the pattern it finds.\n\n"
         "This is <I>very</I> experimental at this stage, but it's already interesting "
         "to play with.");
           
            
    printf("<TT><PRE>\n");
    showTileOpts();
    }
#endif /* OLD */

void hyperlinkProfile(struct profile *prof)
/* Start a hyperlink for profile. */
{
char *consSeq = consensusSeq(prof);

printf("<A HREF=../cgi-bin/ameme.exe?startTile=%s&startPos=%f&good=%s",
    consSeq, prof->locale.mean, goodName);
if (badName)
    printf("&bad=%s", badName);
if (nullModelCgiName)
    {
    printf("&Background=%s", nullModelCgiName);
    }
printf(">");
}

void showTileOpts()
/* Display seqList with promising tiles hyperlinked to
 * iterative aligner. */
{
struct profile *profileList, *prof;
struct seqList *seqEl;
struct dnaSeq *seq;
double bestScore;
double cutoffScore;
int tileSize;
int seqSize;
int profCount;
struct profile nomansLand;

profileList = findStartProfiles(goodSeq, startScanLimit, badSeq);
if (profileList == NULL)
    errAbort("Couldn't find any pattern, sorry\n");
slSort(&profileList, cmpProfiles);
profileList = collapseStartProfiles(profileList);
bestScore = profileList->score;
cutoffScore = bestScore-2;
if (cutoffScore < 0)
    cutoffScore = 0;

/* Allocate the matchProf members. */
for (seqEl = goodSeq; seqEl != NULL; seqEl = seqEl->next)
    {
    seq = seqEl->seq;
    seqEl->matchProf = needMem(seq->size * sizeof(seqEl->matchProf[0]));
    }

seqSize = goodSeqElSize;

/* Fill in matchProf with profile associated with corresponding position in
 * sequence. */
for (prof = profileList; prof != NULL; prof = prof->next)
    {
    makeLocProb(prof->locale.mean, prof->locale.standardDeviation, seqSize);
    if (prof->score < cutoffScore)
        break;
    tileSize = prof->columnCount;
    for (seqEl = goodSeq; seqEl != NULL; seqEl = seqEl->next)
        {
        double score;
        int pos;
        DNA *dna;
        struct profile **matchers;
               
        /* Claim best match in sequence for this profile (if it's
         * a decent match. */ 
        seq = seqEl->seq;
        dna = seq->dna;
        bestMatchInSeq(prof, dna, seqEl->softMask, seqSize, locProb, &score, &pos);
        if (score > 0 && score > prof->score-1)
            {
            int i;
            int startIx, endIx;
            boolean clippedStart = FALSE, clippedEnd = FALSE;
            boolean allClear = TRUE;
            
            startIx = pos-1;
            if (startIx < 0) 
                {
                startIx = 0;
                clippedStart = TRUE;
                }
            endIx = pos + tileSize;
            if (endIx >= seqSize)
                {
                endIx = seqSize-1;
                clippedEnd = TRUE;
                }
            matchers = seqEl->matchProf;
            for (i=startIx; i<=endIx; ++i)
                {
                if (matchers[i] != NULL)
                    allClear = FALSE;
                }
            if (allClear)
                {
                for (i=pos; i<pos+tileSize; ++i)
                    matchers[i] = prof;
                if (!clippedStart)
                    matchers[clippedStart] = &nomansLand;
                if (!clippedEnd)
                    matchers[clippedEnd] = &nomansLand;
                }
            }        
        }
    }
htmlHorizontalLine();

/* Do the nasty output function. */
for (seqEl = goodSeq; seqEl != NULL; seqEl = seqEl->next)
    {
    struct profile **matchers = seqEl->matchProf;
    struct dnaSeq *seq = seqEl->seq;
    DNA *dna = seq->dna;
    int dnaSize = seq->size;
    struct profile *prof, *lastProf = NULL;
    int i;
    
    for (i=0; i<dnaSize; ++i)
        {
        prof = matchers[i];
        if (prof != lastProf)
            {
            if (lastProf != NULL && lastProf != &nomansLand)
                printf("</A>");
            if (prof != NULL && prof != &nomansLand)
                hyperlinkProfile(prof);        
            }
        printf("%c", dna[i]);
        lastProf = prof;
        }
    if (prof != NULL && prof != &nomansLand)
        printf("</A>");
    printf("\n");
    }

htmlHorizontalLine();

puts("</PRE></TT>");
puts("Here are the promising tiles ranked by score. As you can see I need to still "
     "figure out how to merge nearly identical and overlapping tiles.<BR>");
puts("<PRE><TT>");
profCount = 0;
for (prof = profileList; prof != NULL; prof = prof->next)
    {
    char *consSeq;
    if (++profCount >= 100)
        break;
    hyperlinkProfile(prof);
    consSeq = consensusSeq(prof);
    printf("%s at %2.0f</A> score %f ", consSeq, prof->locale.mean, prof->score);
    if (profCount % 3 == 0)
        printf("\n");
    }
htmlHorizontalLine();
}


