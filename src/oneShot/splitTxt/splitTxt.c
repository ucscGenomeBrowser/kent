/* splitTxt - split a text file into a number of files of the given # of lines */
#include "common.h"


int main(int argc, char *argv[])
{
char *inName;
char outName[256];
int maxLines;
char *baseName;
int i;
int lineCount;
char line[4096];
FILE *in, *out;

if (argc != 4 || !isdigit(argv[2][0]))
    {
    errAbort("splitTxt - splits a text file into a number of files of the given # of lines\n"
             "usage:\n"
             "    splitTxt file lineCount baseName"
             "example:\n"
             "    splitTxt huge.txt 1000 small\n"
             "This will break up huge.txt into 1000 line files small001, small002, etc.\n");
    }

inName = argv[1];
maxLines = atoi(argv[2]);
baseName = argv[3];
in = mustOpen(inName, "r");
for (i=1; ; i++)
    {
    sprintf(outName, "%s%03d", baseName, i);
    out = mustOpen(outName, "w");
    printf("Writing %s\n", outName);
    for (lineCount = 0; lineCount < maxLines; ++lineCount)
        {
        if (fgets(line, sizeof(line), in) == NULL)
            exit(0);
        fputs(line, out);
        }
    fclose(out);
    }
return 0;
}