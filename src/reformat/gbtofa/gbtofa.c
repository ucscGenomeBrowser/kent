/* gbtofa - convert from genebank to fa format. Does
 * filtering to make sure it's really DNA/RNA. */
#include "common.h"
#include "dnautil.h"
#include "hash.h"

void filter(FILE *in, FILE *out)
{
char locusLine[512];
char *locusWords[8];
int locusWordCount;
char *dnaIdWord;
int locusCount = 0;

char accessionLine[256];
char *accessionWords[3];
int accessionWordCount;

char definitionLine[256];
char *definitionWords[16];
int definitionWordCount;
char koharaName[64];

char *ourName;

char lineBuf[512];
char *linePt;

int basePairs;
int twiceBasePairs;
DNA *dnaBuf = NULL;
int dnaAllocated = 0;
int dnaCount;
DNA base;
char c;

boolean gotKohara;
boolean isEmbryo;
int embryoCount = 0;
int uniqCount = 0;

int i;
int lineSize;

struct hash *hash = newHash(12);
struct hashEl *el;

/* Main loop - once per cDNA. */
for (;;)
    {
LOOP_START:
    /* Loop to get locus. */
    isEmbryo = FALSE;
    for (;;)
        {
        if (fgets(locusLine, sizeof(locusLine), in) == NULL)
            {
            goto LOOP_END;
            }
        if (strncmp(locusLine, "LOCUS", 5) == 0)
            {
            if ((locusWordCount = chopString(locusLine, whiteSpaceChopper, locusWords, ArraySize(locusWords))) < 4)
                errAbort("Bad LOCUS line, only %d words", locusWordCount);
            dnaIdWord = locusWords[3];
            if (differentWord(dnaIdWord, "bp"))
                {
                warn("Locus %s doesn't appear to be nucleic acid (expecting %s got %s), skipping",
                    locusWords[1], "bp", dnaIdWord);
                goto LOOP_START;
                }
            if (!differentWord(locusWords[4], "DNA"))
                {
                warn("Locus %s is DNA not RNA, skipping", locusWords[1]);
                goto LOOP_START;
                }
            basePairs = atol(locusWords[2]);
            if (basePairs <= 0)
                {
                errAbort("Expecting positive number of base pairs, got %s",
                    locusWords[2]);
                }

            /* Make sure we have room for DNA. */
            twiceBasePairs = 2*basePairs;
            if (twiceBasePairs > dnaAllocated)
                {
                dnaAllocated = twiceBasePairs;
                freeMem(dnaBuf);
                dnaBuf = needMem(dnaAllocated);
                }
            dnaCount = 0;
            ++locusCount;
            break;
            }
        }
    /* Loop to get definition. */
    gotKohara = FALSE;
    for (;;)
        {
        if (fgets(definitionLine, sizeof(definitionLine), in) == NULL)
            {
            warn("Got LOCUS without DEFINITION");
            goto LOOP_END;
            }
        if (strncmp(definitionLine, "DEFINITION", 10) == 0)
            {
            definitionWordCount = chopLine(definitionLine, definitionWords);
            if (definitionWordCount > 4 && 
                sameString(definitionWords[2], "Yuji") && 
                sameString(definitionWords[3], "Kohara"))
                {
                gotKohara = TRUE;
                }
            break;
            }
        }
    if (gotKohara)
        {
        fgets(definitionLine, sizeof(definitionLine), in);
        definitionWordCount = chopLine(definitionLine, definitionWords);
        if (definitionWordCount >= 3 && sameString(definitionWords[0], "clone")  && 
            definitionWords[1][0] == 'y' && definitionWords[1][1] == 'k' && 
            (definitionWords[2][0] == '3' || definitionWords[2][0] == '5') )
            {
            sprintf(koharaName, "%s.%c", definitionWords[1], definitionWords[2][0]);
            ourName = koharaName;
            }
        else if (definitionWordCount >= 7 && sameString(definitionWords[0], "embryo") && 
            sameString(definitionWords[4], "clone") &&
            definitionWords[5][0] == 'y' && definitionWords[5][1] == 'k' && 
            (definitionWords[6][0] == '3' || definitionWords[6][0] == '5') )
            {
            sprintf(koharaName, "%s.%c", definitionWords[5], definitionWords[6][0]);
            ourName = koharaName;
            isEmbryo = TRUE;
            }
        else
            {
            errAbort("Unusual Kohara!");
            gotKohara = FALSE;
            }
        }
    /* Loop to get accession. */
    for (;;)
        {
        if (fgets(accessionLine, sizeof(accessionLine), in) == NULL)
            {
            warn("Got LOCUS without ACCESSION");
            goto LOOP_END;
            }
        if (strncmp(accessionLine, "ACCESSION", 9) == 0)
            {
            if ((accessionWordCount = chopString(accessionLine, whiteSpaceChopper, accessionWords, ArraySize(accessionWords))) < 2)
                errAbort("Bad ACCESSION line, only %d words", accessionWordCount);
            if (!gotKohara)
                ourName = accessionWords[1];
            break;
            }
        }

    if ((el = hashLookup(hash, ourName)) != NULL)
        {
        warn("Got duplicate ourName %s, skipping all but first",
            ourName);
        goto LOOP_START;
        }
    hashAdd(hash, ourName, NULL);
    ++uniqCount;
    if (isEmbryo)
        ++embryoCount;

    /* Loop to get origin */
    for (;;)
        {
        if (fgets(lineBuf, sizeof(lineBuf), in) == NULL)
            {
            warn("Got LOCUS without ORIGIN");
            goto LOOP_END;
            }
        if (strncmp(lineBuf, "ORIGIN", 6) == 0)
            {
            break;
            }
        }
    /* Loop to read DNA */
    for (;;)
        {
        if (fgets(lineBuf, sizeof(lineBuf), in) == NULL)
            {
            warn("No // at end of sequence");
            break;
            }
        if (strncmp(lineBuf, "//", 2) == 0)
            {
            break;
            }
        /* Add in this line's worth of DNA. */
        linePt = lineBuf;
        while ((c = *linePt++) != 0)
            {
            if ((base = ntChars[c]) != 0)
                {
                if (dnaCount >= dnaAllocated)
                    {
                    errAbort("Got more than %d bases, header said there "
                             "should only be %d in %s!",
                             dnaCount, basePairs, ourName);
                    }
                dnaBuf[dnaCount++] = base;                
                }
            }
        }
    /* Now write out in .fa format. */
    printf("%d %s\n", uniqCount, ourName);
    fprintf(out, ">%s", ourName);
    if (isEmbryo)
        fprintf(out, " embryo");
    fputc('\n', out);
    for (i=0; i<dnaCount; i += lineSize)
        {
        lineSize = dnaCount - i;
        if (lineSize > 50) lineSize = 50;
        mustWrite(out, dnaBuf+i, lineSize);
        putc('\n', out);
        }
    }
LOOP_END:
    printf("Saved %d sequences (%d embryonic) to fa file\n", uniqCount, embryoCount);
    freeHash(&hash);
}

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in, *out;

if (argc != 3)
    {
    printf("gbtofa converts from GeneBank to fa format.\n");
    printf("usage:      gbtofa in.gb out.fa\n");
    exit(-1);
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");
dnaUtilOpen();
filter(in,out);
carefulClose(&in);
carefulClose(&out);
return 0;
}