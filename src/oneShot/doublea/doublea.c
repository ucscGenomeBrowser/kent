/* DoubleA - count # of cDNAs that align well to more than one spot. */
#include "common.h"
#include "../cdnaAli/inc/cdnaAli.h"

int countMulti(struct cdna *cdna)
{
int count = 0;
for (;cdna != NULL; cdna = cdna->next)
    {
    if (slCount(cdna->aliList) > 1)
        ++count;
    }
return count;
}

int main(int argc, char *argv[])
{
char *goodName;
int multiCount;
struct cdna *cdna;

if (argc != 2)
    {
    errAbort("doublea - counts the number of multiply aligned cDNAs in good.txt\n"
             "usage:\n"
             "    doublea good.txt");
    }
cdnaAliInit();
goodName = argv[1];
cdna = loadCdna(goodName);
multiCount = countMulti(cdna);
printf("%d out of %d cDNAs are multiply aligned\n", multiCount, slCount(cdna));
return 0;
}
