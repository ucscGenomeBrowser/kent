/* fixalt - fix alt splice catalog - which has 3' and 5' splice sites
 * mixed up on minus strand. */
#include "common.h"
#include "cda.h"
#include "wormdna.h"
#include "htmshell.h"


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
return hrefTag;
}

char *skipJunkTag(char *pt)
{
static char *junks[] = {"</TT></A><TT>", "<TT>", "</TT>", "</A>", "</PRE>" };

pt = skipLeadingSpaces(pt);
if (pt[0] == '<')
    {
    int len;
    int i;
    for (i=0; i<ArraySize(junks); ++i)
        {
        len = strlen(junks[i]);
        if (strncmp(junks[i], pt, len) == 0)
            {
            pt += len;
            break;
            }
        }
    }
return pt;
}


char *unjunkLine(char *pt)
{
char *start, *end;
start = pt;
while ((start = strchr(start, '<')) != NULL)
    {
    end = skipJunkTag(start);
    if (end == start)
        start += 1;
    else
        strcpy(start, end);
    }
return pt;
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
boolean gotLine = FALSE;

while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    if (strncmp(lineBuf, "----------", 10) == 0)
        {
        gotLine = TRUE;
        break;
        }
    }
if (!gotLine)
    {
    lineCount = 0;
    rewind(f);
    }
while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    if (strncmp("</BODY>", lineBuf, 7) == 0)
        break;
    lineLen = strlen(lineBuf);
    if (lineLen < 3)
        continue;
    if (lineLen < 56)
        errAbort("Short line %d of %s\n", lineCount, fileName);
    if (lineLen == sizeof(lineBuf)-1)
        errAbort("Line longer than %d chars line %d of %s, oops.", lineLen, lineCount, fileName);
    linePt = unjunkLine(lineBuf);
    AllocVar(el);
    el->hrefTag = eatHrefTag(linePt);
    wordCount = chopLine(linePt, words);
    el->orfName = cloneString(words[0]);

    for (i=1; i<wordCount; ++i)
        {
        char *s = words[i];
        if (sameString(s, "EX"))
            el->skipExon = TRUE;
        else if (sameString(s, "IN"))
            el->skipIntron = TRUE;
        else if (sameString(s, "NG"))
            el->nonGenomic = TRUE;
        else if (s[1] == '\'')
            {
            if (s[0] == '3')
                el->alt3 = atoi(s+3);
            else if (s[0] == '5')
                el->alt5 = atoi(s+3);
            else
                errAbort("%s??? line %d of %s", s, lineCount, fileName);
            }
        else
            break;
        }
    if (i + 3 > wordCount)
        errAbort("Short line %d of %s", lineCount, fileName);

    el->ieOverlap = atoi(words[i]);
    el->iiOverlap = atoi(words[i+1]);
    for (i=i+2; i<wordCount; ++i)
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

/* Write the start of a stand alone .html file. */
htmStart(f, "Fixed Alt-Splicing Catalog");
fprintf(f, "<TT><PRE>");
fprintf(f, "-----------------------------\n");

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
/* Write the end of a stand-alone html file */
fprintf(f, "</TT></PRE>");
htmEnd(f);

fclose(f);
}



boolean isReverseStrand(char *htag, char *orfName)
{
char buf[1024];
char *words[100];
int wordCount;
char *parts[16];
int partCount;
char *chrom;
int start, end;
int plusCount = 0, minusCount = 0;
int totalCount;
struct cdaAli *aliList, *ali;
boolean isRev = FALSE;

strcpy(buf, htag);
wordCount = chopString(buf, "=", words, ArraySize(words));
assert(wordCount > 3);
partCount = chopString(words[2], "&", parts, ArraySize(parts));
if (!wormParseChromRange(parts[0], &chrom, &start, &end))
    assert(FALSE);
aliList = wormCdaAlisInRange(chrom, start, end);
for (ali = aliList; ali != NULL; ali = ali->next)
    {
    if (cdaDirChar(ali, '+') == '>')
        ++plusCount;
    else
        ++minusCount;
    }
totalCount = plusCount + minusCount;
if (plusCount > minusCount)
    {
    if (minusCount * 5 > totalCount)
        warn("Please double-check %s", orfName);
    }
else
    {
    if (plusCount * 5 > totalCount)
        warn("Please double-check %s", orfName);
    isRev = TRUE;
    }
cdaFreeAliList(&aliList);
return isRev;
}

void fixAlts(struct altSpec *altList)
{
struct altSpec *alt;
int reverseCount = 0;
int inspectCount = 0;
int totalCount = 0;

for (alt = altList; alt != NULL; alt = alt->next)
    {
    ++totalCount;
    if (alt->alt3 != 0 || alt->alt5 != 0)
        {
        ++inspectCount;
        if (isReverseStrand(alt->hrefTag, alt->orfName))
            {
            int temp = alt->alt3;
            alt->alt3 = alt->alt5;
            alt->alt5 = temp;
            ++reverseCount;
            }
        }
    }
printf("Reversed %d of %d 3'/5' (%d total)\n", reverseCount, inspectCount, totalCount);
}

       
int main(int argc, char *argv[])
{
char *oldName, *newName;
struct altSpec *altList;

if (argc != 3)
    {
    errAbort("fixalts - fixed minus strand 3'/5' mixup onalts file.\n"
             "Usage:\n"
             "       weedAlts alt.html newAlt.html\n");
    }
oldName = argv[1];
newName = argv[2];
printf("Reading %s\n", oldName);
altList = readAlts(oldName);
fixAlts(altList);
printf("Writing %s\n", newName);
writeAlts(newName, altList);
return 0;
}



