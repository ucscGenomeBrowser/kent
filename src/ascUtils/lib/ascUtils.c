#include "common.h"
#include "ascUtils.h"

char *eatHrefTag(char *line)
/* Remove HrefTag from line and return it. */
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

struct altSpec *readAlts(char *fileName, boolean oldStyle)
/* Read alt-splicing file and turn it into an altSpec list. */
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
int newStyleExtraSpace = (oldStyle ? 0 : 15);
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
    if (!oldStyle)
        {
        if (lineBuf[27] != ' ')
            {
            el->alt5 = atoi(lineBuf+29);
            }
        if (lineBuf[34] != ' ')
            {
            el->alt3 = atoi(lineBuf+36);
            }
        }
    el->nonGenomic = (lineBuf[27+newStyleExtraSpace] != ' ');
    chopByWhite(lineBuf, words, 1);
    el->orfName = cloneString(words[0]);
    linePt = lineBuf + 29+newStyleExtraSpace;
    wordCount = chopLine(linePt, words);
    if (wordCount == ArraySize(words))
        errAbort("At least %d words on line %d of %s, oops.", wordCount, lineCount, fileName);
    assert(isdigit(words[0][0]));
    if (oldStyle)
        {
        el->alt3 = atoi(words[0]);
        el->alt5 = atoi(words[1]);
        el->ieOverlap = atoi(words[2]);
        el->iiOverlap = atoi(words[3]);
        assert(wordCount >= 5);
        for (i=4; i<wordCount; ++i)
            slAddTail(&el->cdna, newSlName(words[i]));
        }
    else
        {
        el->ieOverlap = atoi(words[0]);
        el->iiOverlap = atoi(words[1]);
        assert(wordCount >= 3);
        for (i=2; i<wordCount; ++i)
            slAddTail(&el->cdna, newSlName(words[i]));
        }
    slAddHead(&list, el);
    }
slReverse(&list);
fclose(f);
return list;
}

static char *boos(boolean boo, char *trues, char *falses)
/* Return string reflecting boolean value. */
{
return boo ? trues : falses;
}

static char *alt3(struct altSpec *alt)
/* Returns value to print at alt3 slot. */
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

static char *alt5(struct altSpec *alt)
/* Returns value to print at alt5 slot. */
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


void writeAlts(char *fileName, struct altSpec *altList, 
    boolean newOnly, boolean notOldOnly)
/* Write alt-splicing file. */
{
FILE *f = mustOpen(fileName, "w");
struct altSpec *alt;
struct slName *cdna;

for (alt = altList; alt != NULL; alt = alt->next)
    {
    if (newOnly && !alt->hasNewCdna)
        continue;
    if (notOldOnly && alt->isInOld)
        continue;
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

