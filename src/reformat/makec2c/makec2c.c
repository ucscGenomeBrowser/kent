#include "common.h"

char *inNames[] = {
    "CHROMOSOME_I.gff",
    "CHROMOSOME_II.gff",
    "CHROMOSOME_III.gff",
    "CHROMOSOME_IV.gff",
    "CHROMOSOME_V.gff",
    "CHROMOSOME_X.gff",
};

char *bigChromNames[] = {
    "CHROMOSOME_I",
    "CHROMOSOME_II",
    "CHROMOSOME_III",
    "CHROMOSOME_IV",
    "CHROMOSOME_V",
    "CHROMOSOME_X",
};

char *smallChromNames[] = {
    "I",
    "II",
    "III",
    "IV",
    "V",
    "X",
};

char *smallName(char *bigName)
{
int i;
for (i=0; i<ArraySize(bigChromNames); ++i)
    {
    if (sameString(bigChromNames[i], bigName))
        return smallChromNames[i];
    }
errAbort("%s isn't a chromosome", bigName);
}

char *unquote(char *s)
/* Remove opening and closing quotes from string s. */
{
int len =strlen(s);
if (s[0] != '"')
    errAbort("Expecting begin quote on %s\n", s);
if (s[len-1] != '"')
    errAbort("Expecting end quote on %s\n", s);
s[len-1] = 0;
return s+1;
}


int main(int argc, char *argv[])
{
char *outName;
char *inName;
int i;
FILE *in, *c2c;
char line[4*1024];
char origLine[4*1024];
int lineCount = 0;
char *words[4*256];
int wordCount;

if (argc != 2)
    {
    errAbort("makec2c - creates a cosmid chromosome offset index file from Sanger .gffs\n"
           "usage:\n"
           "      makec2c c2c\n"
           "This will create the file c2c with the index.  This program should be run\n"
           "from directory with CHROMOSOME_n.gff files in it.\n");
    }
outName = argv[1];
c2c = mustOpen(outName, "w");
for (i=0; i<ArraySize(inNames); ++i)
    {
    inName = inNames[i];
    printf("Processing %s\n", inName);
    in = mustOpen(inName, "r");
    while (fgets(line, sizeof(line), in))
        {
        ++lineCount;
        if (strncmp(line, "CHROMOSOME", 10) == 0)
            {
            strcpy(origLine, line);
            wordCount = chopLine(line, words);
            if (wordCount < 8)
                errAbort("Short line %d in %s\n", lineCount, inName);
            if (sameString(words[1], "SEQUENCE"))
                {
                char *seqName;
                if (wordCount < 10)
                    errAbort("Short SEQUENCE line %d in %s\n", lineCount, inName);
                seqName = unquote(words[9]);
                if (strncmp(seqName, "LINK", 4) != 0 && strncmp(seqName, "SUPERLINK", 9) != 0
                    && strncmp(seqName, "CHROMOSOME", 10) != 0 &&  strchr(seqName, '.') == NULL)
                    {
                    fprintf(c2c, "%s:%s-%s + %s\n", smallName(words[0]), words[3], words[4], seqName);
                    } 
                }
            }
        }
    fclose(in);
    }
return 0;
}