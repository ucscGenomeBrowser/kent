/* WeedAlts - remove genes from alts file that are also in weed file.
 * Also do some light reformating of alts file, then write it back out.
 */

#include "common.h"
#include "hash.h"

struct hash *makeWeedHash(char *fileName)
{
FILE *f = fopen(fileName, "r");
struct hash *hash = newHash(12);
char lineBuf[512];
char *words[1];

while (fgets(lineBuf, sizeof(lineBuf), f))
    {
    if (chopLine(lineBuf, words))
        {
        hashAdd(hash, words[0], NULL);
        }
    }
fclose(f);
return hash;
}

struct altSpec
    {
    struct altSpec *next;
    char *hrefTag;
    char *orfName;
    boolean skipExon, skipIntron, nonGenomic;
    int alt3, alt5;
    int ieOverlap, iiOverlap;
    struct slName *cdna;
    };



char *eatHrefTag(char *line)
{
char *startTag = strchr(line, '<');
char *endTag = strchr(startTag, '>');
char *hrefTag = cloneStringZ(startTag, endTag-startTag+1);
strcpy(startTag, endTag+1);
startTag = strchr(startTag, '<');
endTag = strchr(startTag, '>');
strcpy(startTag, endTag+1);
return hrefTag;
}

struct altSpec *readAlts(char *fileName)
{
FILE *f = mustOpen(fileName, "r");
char lineBuf[2048];
int lineLen;
int lineCount = 0;
struct altSpec *list = NULL, *el;
char *words[512];
int wordCount;
char *linePt;
int i;

while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    lineLen = strlen(lineBuf);
    if (lineLen < 3)
        continue;
    if (lineLen < 56)
        errAbort("Short line %d of %s\n", lineCount, fileName);
    if (lineLen == sizeof(lineBuf)-1)
        errAbort("Line longer than %d chars line %d of %s, oops.", lineLen, lineCount, fileName);
    AllocVar(el);
    el->hrefTag = eatHrefTag(lineBuf);
    el->skipExon =  (lineBuf[17] != ' ');
    el->skipIntron = (lineBuf[22] != ' ');
    el->nonGenomic = (lineBuf[27] != ' ');
    chopByWhite(lineBuf, words, 1);
    el->orfName = cloneString(words[0]);
    linePt = lineBuf + 29;
    wordCount = chopLine(linePt, words);
    if (wordCount == ArraySize(words))
        errAbort("At least %d words on line %d of %s, oops.", wordCount, lineCount, fileName);
    assert(isdigit(words[0][0]));
    el->alt5 = atoi(words[0]);
    el->alt3 = atoi(words[1]);
    el->ieOverlap = atoi(words[2]);
    el->iiOverlap = atoi(words[3]);
    assert(wordCount >= 5);
    for (i=4; i<wordCount; ++i)
        slAddTail(&el->cdna, newSlName(words[i]));
    slAddHead(&list, el);
    }
slReverse(&list);
fclose(f);
return list;
}

char *boos(boolean boo, char *trues, char *falses)
/* Return string reflecting boolean value. */
{
return boo ? trues : falses;
}

char *alt3(struct altSpec *alt)
{
static char buf[12];

if (alt->skipExon || alt->skipIntron || alt->alt5 || alt->alt3 == 0)
    return "   ";
else
    {
    sprintf(buf, "3'(%d)", alt->alt3);
    return buf;
    }
}

char *alt5(struct altSpec *alt)
{
static char buf[12];

if (alt->skipExon || alt->skipIntron || alt->alt3 || alt->alt5 == 0)
    return "   ";
else
    {
    sprintf(buf, "5'(%d)", alt->alt5);
    return buf;
    }
}


void writeAlts(char *fileName, struct altSpec *altList)
{
FILE *f = mustOpen(fileName, "w");
struct altSpec *alt;
struct slName *cdna;

for (alt = altList; alt != NULL; alt = alt->next)
    {
    fprintf(f, "%s%-13s</A>   %s   %s   %-6s %-6s  %s %5d %5d  ",
        alt->hrefTag, alt->orfName, 
        boos(alt->skipExon, "EX", "  "), boos(alt->skipIntron, "IN", "  "),
        alt5(alt), alt3(alt), boos(alt->nonGenomic, "NG", "  "),  
        alt->ieOverlap, alt->iiOverlap);
    for (cdna = alt->cdna; cdna != NULL; cdna = cdna->next)
        fprintf(f, " %s", cdna->name);
    fputc('\n', f);
    }
fclose(f);
}

struct altSpec *weedAlts(struct hash *weedHash, struct altSpec *altList)
{
struct altSpec *newList = NULL;
struct altSpec *alt, *next;

alt = altList;
while (alt != NULL)
    {
    next = alt->next;
    if (!hashLookup(weedHash, alt->orfName))
        slAddHead(&newList, alt);
    alt = next;
    }
slReverse(&newList);
return newList;
}

       
int main(int argc, char *argv[])
{
char *weedName, *oldName, *newName;
struct hash *weedHash;
struct altSpec *altList;

if (argc != 4)
    {
    errAbort("weedAlts - removes weeds from alts file.\n"
             "Usage:\n"
             "       weedAlts weed.txt alt.txt newAlt.txt\n"
             "The first word of each line in the weed.txt file specifies gene to weed out.\n");
    }
weedName = argv[1];
oldName = argv[2];
newName = argv[3];
weedHash = makeWeedHash(weedName);
altList = readAlts(oldName);
printf("Before weeding %d alts\n", slCount(altList));
altList = weedAlts(weedHash, altList);
printf("After weeding %d alts\n", slCount(altList));
writeAlts(newName, altList);
return 0;
}
