/* ck3and5 - Checks xxxx.3 and xxxx.5 EST pairs to make sure they align near the same
 * piece of DNA. */
#include "common.h"
#include "hash.h"
#include "snof.h"
#include "cda.h"
#include "wormdna.h"

struct ck3and5
    {
    struct ck3and5 *next;
    char *three;
    char *five;
    };

FILE *aliFile;
struct snof *aliSnof;

void openAli()
/* Open up alignment file and index for it. */
{
char aliName[512];

sprintf(aliName, "%sgood", wormCdnaDir());
aliSnof = snofOpen(aliName);
aliFile = wormOpenGoodAli();
}

struct cdaAli *getAli(char *name)
/* Get alignment of named cDNA. */
{
long offset;
struct cdaAli *ali;

if (!snofFindOffset(aliSnof, name, &offset))
    return NULL;
fseek(aliFile, offset, SEEK_SET);
ali = cdaLoadOne(aliFile);
if (ali == NULL)
    errAbort("Couldn't load %s in ali file", name);
return ali;
}

int oneAlignCount = 0;
int farAlignCount = 0;
int sameStrandCount = 0;
int unrel3and5Count = 0;

boolean pairMatch(char *three, char *five, FILE *out)
/* Return FALSE if pair doesn't align in plausible place with respect to each other. */
{
struct cdaAli *ali3, *ali5;

ali3 = getAli(three);
ali5 = getAli(five);
if (ali3 == NULL && ali5 == NULL)
    return TRUE;        /* Neither one aligned, that's ok. */
if (ali3 == NULL)
    {
    fprintf(out, "%s aligned, but not %s\n", five, three);
    ++oneAlignCount;
    return FALSE;
    }
if (ali5 == NULL)
    {
    fprintf(out, "%s aligned but not %s\n", three, five);
    ++oneAlignCount;
    return FALSE;
    }
if (ali3->chromIx != ali5->chromIx)
    {
    ++farAlignCount;
    fprintf(out, "%s is on chromosome %s but %s is on chromosome %s\n",
        three, wormChromForIx(ali3->chromIx),
        five, wormChromForIx(ali5->chromIx));
    return FALSE;
    }
if (ali3->chromEnd < ali5->chromStart - 100000 || ali3->chromStart > ali5->chromEnd + 100000)
    {
    ++farAlignCount;
    fprintf(out, "%s aligns to %s:%d-%d  %s aligns to %s:%d-%d\n",
        three, wormChromForIx(ali3->chromIx), ali3->chromStart, ali3->chromEnd,
        five, wormChromForIx(ali5->chromIx), ali5->chromStart, ali5->chromEnd);
    return FALSE;
    }
if (ali3->strand == ali5->strand)
    {
    ++sameStrandCount;    
    fprintf(out, "%s and %s align on same strand\n", three, five);
    return FALSE;
    }
if (ali5->strand == '+')
    {
    if (ali3->chromEnd < ali5->chromStart)
        {
        unrel3and5Count++;
        return FALSE;
        }
    }
else if (ali5->strand == '-')
    {
    if (ali5->chromEnd < ali3->chromStart)
        {
        unrel3and5Count++;
        return FALSE;
        }
    }
else
    errAbort("Strand should be + or -, not %c", ali5->strand);
return TRUE;
}

struct hash *buildMultiAlignHash()
/* Return a hash table filled with names of cDNAs that align more
 * than once. */
{
struct cdaAli *cda;
struct hash *dupeHash;
char lastName[128];
char *name;
int dupeCount = 0;

lastName[0] = 0;
dupeHash = newHash(12);

while (cda = cdaLoadOne(aliFile))
    {
    name = cda->name;
    if (sameString(name, lastName))
        {
        if (!hashLookup(dupeHash, name))
            {
            hashAdd(dupeHash, name, NULL);
            ++dupeCount;
            }
        }
    strcpy(lastName, name);
    cdaFreeAli(cda);
    }
printf("Found %d multiple alignments\n", dupeCount);
return dupeHash;
}

int main(int argc, char *argv[])
{
char *outName;
FILE *out;
char faName[512];
FILE *fa;
char line[1024];
int lineCount=0;
char *strand;
char *cdnaName;
char *lastDot;
struct hash *dupeHash;
struct hash *hash = newHash(17);
struct hashEl *hel;
char nameBuf[128];
struct ck3and5 *ckList = NULL, *ck;
char nameEnd;
int estCount = 0;
int matchingPairCount = 0;
int badPairCount = 0;

if (argc != 2)
    {
    errAbort("ck3and5 - checks that 3' and 5' ESTs from the same clone align near each other.\n"
             "usage:\n"
             "    ck3and5 output.txt");
    }
outName = argv[1];
out = mustOpen(outName, "w");

/* Scan through cdna file building up list of matching 3'/5'EST's */
sprintf(faName, "%s%s", wormCdnaDir(), "allcdna.fa");
fa = mustOpen(faName, "r");
while (fgets(line, sizeof(line), fa))
    {
    ++lineCount;
    if (lineCount % 100000 == 0)
        printf("Line %d of %s\n", lineCount, faName);
    if (line[0] == '>')
        {
        strand = strchr(line, ' ');
        if (strand == NULL)
            errAbort("Expecting lots of info, didn't get much line %d of %s:\n%s",
                lineCount, faName, line);
        cdnaName = line+1;
        *strand++ = 0;
        strncpy(nameBuf, cdnaName, sizeof(nameBuf));
        lastDot = strrchr(nameBuf, '.');
        if (lastDot != NULL)
            {
            ++estCount;
            *lastDot = 0;
            nameEnd = lastDot[1];
            if ((hel = hashLookup(hash, nameBuf)) != NULL)
                ck = hel->val;
            else
                {
                AllocVar(ck);
                slAddHead(&ckList, ck);
                hashAdd(hash, nameBuf, ck);
                }
            if (nameEnd == '3')
                ck->three = cloneString(cdnaName);
            else if (nameEnd == '5')
                ck->five = cloneString(cdnaName);
            else
                errAbort("Expecting 3 or 5 line %d of %s, got %c", 
                    lineCount, faName, nameEnd);
            }               
        }
    }
fclose(fa);

/* Get duplicate alignment hash. */
openAli();
dupeHash = buildMultiAlignHash();

/* Scan through list looking for ones that have both 3' and 5' versions.
 * Look these up and make sure they go to more or less the same place. */
for (ck = ckList; ck != NULL; ck = ck->next)
    {
    if (ck->three && ck->five && !hashLookup(dupeHash, ck->three) && !hashLookup(dupeHash, ck->five))
        {
        ++matchingPairCount;
        if (matchingPairCount % 1000 == 0)
            printf("Matching pair %d\n", matchingPairCount);
        badPairCount += !pairMatch(ck->three, ck->five, out);
        }
    }
fprintf(out, "%d bad pairs out of %d matching pairs in %d ESTs\n", badPairCount, matchingPairCount, estCount);
printf("%d bad pairs out of %d matching pairs in %d ESTs\n", badPairCount, matchingPairCount, estCount);
printf("See %s for details on bad pairs\n", outName);
printf("Summary of bad pairs:\n"
       "  %d where only one aligned\n"
       "  %d where aligned far from each other\n"
       "  %d where aligned close but on same strand\n"
       "  %d where 3' and 5' reversed\n",
       oneAlignCount, farAlignCount, sameStrandCount, unrel3and5Count
       );
fprintf(out, "Summary of bad pairs:\n"
       "  %d where only one aligned\n"
       "  %d where aligned far from each other\n"
       "  %d where aligned close but on same strand\n"
       "  %d where 3' and 5' reversed\n",
       oneAlignCount, farAlignCount, sameStrandCount, unrel3and5Count
       );

return 0;
}

