/* cosmoCtr - count how many cosmids have significant alignments. */
#include "common.h"
#include "snof.h"



int main(int argc, char *argv[])
{
char *listName = "C:/biodata/cbriggsae/finish/allcosmi.txt";
char *snofName = "C:/biodata/celegans/xeno/cbriggsae/all";
FILE *list;
struct snof *snof;
char line[512];
int lineCount;
char *s, *e;
char cosmoName[128];
int aliCount = 0, noAliCount = 0;

list = mustOpen(listName, "r");
snof = snofOpen(snofName);
while (fgets(line, sizeof(line), list) != NULL)
    {
    ++lineCount;
    if ((s = strrchr(line, '\\')) != NULL && (e = strrchr(s, '.')) != NULL)
        {
        int size;
        long offset;

        s += 1;
        size = e - s;
        memcpy(cosmoName, s, size);
        strcpy(cosmoName+size, ".c1");
        if (!snofFindOffset(snof, cosmoName, &offset))
            {
            printf("No alignment for %s\n", cosmoName);
            ++noAliCount;
            }
        else
            ++aliCount;
        }
    }
printf("%d aligned, %d didn't.\n", aliCount, noAliCount);
return 0;
}
