/* Preonly.c - remove everything from a file except parts bracketed by
 * <pre></pre>.  This is useful for extracting GenBank files that are
 * embedded in .html. */
#include "common.h"

int main(int argc, char *argv[])
{
char *inName, *outName;
FILE *in, *out;
char line[1024];
char *pt;
char *endHtmlDir;
char c;
int lineCount;
boolean inPre = FALSE;

if (argc != 3)
    {
    errAbort("preonly - Remove everything from a file except parts bracketed by \n"
             "<pre></pre>. This is useful for extracting GenBank files that are\n"
             "embedded in .html.\n"
             "usage:\n"
             "     preonly infile outfile");
    }
inName = argv[1];
outName = argv[2];
in = mustOpen(inName, "r");
out = mustOpen(outName, "w");
while (fgets(line, sizeof(line), in) )
    {
    ++lineCount;
    if (lineCount%1000 == 0)
        printf("Processing line %d\n", lineCount);
    pt = line;
    while ((c = *pt++) != 0)
        {
        /* If it looks like an HTML command eat it up and if it's PRE or /PRE toggle state. */
        if (c == '<' && (isalpha(*pt) || *pt == '/') && (endHtmlDir = strchr(pt+1, '>')) != NULL)
            {
            *endHtmlDir = 0;
            if (sameWord(pt, "pre"))
                {
                if (inPre)
                    errAbort("Nested <PRE>'s, I can't handle it line %d of %s", lineCount, inName);
                inPre = TRUE;
                }
            else if (sameWord(pt, "/pre"))
                {
                if (!inPre)
                    errAbort("Nested </PRE>'s, I can't handle it line %d of %s", lineCount, inName);
                inPre = FALSE;
                }
            pt = endHtmlDir+1;
            }
        else
            {
            if (inPre)
                fputc(c, out);
            }
        } 
    }
fclose(in);
fclose(out);
return 0;
}