/* countUpc - count upper case characters and give upper case to total
 * letter ratio. */
#include "common.h"

void usage()
{
errAbort("countUpc - count upper case characters and give upper case\n"
         "to total letter ratio\n"
         "usage\n"
         "    countUpc file");
}

int main(int argc, char *argv[])
{
char *fileName;
FILE *file;
int letterCount = 0;
int upperCount = 0;
int c;

if (argc != 2)
    usage();
fileName = argv[1];
file = mustOpen(fileName, "r");
while ((c = fgetc(file)) != EOF)
    {
    if (isalpha(c))
        {
        ++letterCount;
        if (isupper(c))
            ++upperCount;
        }
    }
fclose(file);
printf("%d letters %d upper case (%2.1f%%)\n",
    letterCount, upperCount, 100.0 * upperCount/letterCount);
return 0;
}