#include "common.h"
#include "hash.h"

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

int cmpAlts(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct altSpec *a = *((struct altSpec **)va);
const struct altSpec *b = *((struct altSpec **)vb);
return strcmp(a->orfName, b->orfName);
}

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
    el->alt3 = atoi(words[0]);
    el->alt5 = atoi(words[1]);
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

void writeAlts(char *fileName, struct altSpec *altList)
{
FILE *f = mustOpen(fileName, "w");
struct altSpec *alt;
struct slName *cdna;

for (alt = altList; alt != NULL; alt = alt->next)
    {
    fprintf(f, "%s%-13s</A>   %s   %s   %s   %5d %5d %5d %5d  ",
        alt->hrefTag, alt->orfName, 
        boos(alt->skipExon, "EX", "  "), boos(alt->skipIntron, "IN", "  "),
        boos(alt->nonGenomic, "NG", "  "), alt->alt3, alt->alt5,
        alt->ieOverlap, alt->iiOverlap);
    for (cdna = alt->cdna; cdna != NULL; cdna = cdna->next)
        fprintf(f, " %s", cdna->name);
    fputc('\n', f);
    }
fclose(f);
}



int main(int argc, char *argv[])
{
struct altSpec *a, *b, *ab;
a = readAlts("julWeeded.txt");
b = readAlts("augNewWeeded.txt");
uglyf("Got %d in a, %d in b\n", slCount(a), slCount(b));
ab = slCat(a,b);
uglyf("Merged to get %d\n", slCount(ab));
slSort(&ab, cmpAlts);
writeAlts("augWeeded.txt", ab);
return 0;
}
