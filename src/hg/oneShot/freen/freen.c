/* freen - My Pet Freen. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "portable.h"
#include "hdb.h"
#include "trackTable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen hgN\n");
}

void freen(char *text)
{
int x, y;
printf("%d\n", 139221 - 82402 + 1);
x = 118260 - 82402 + 1;
y = 19629 - 1 + 1;
printf("%d\n", x + y + 1332);

printf("%d\n", 38640124 + x - 1);
printf("%d\n", 38696944 - y + 1);
printf(">>>\n");
printf("%d\n", 38675982 - 38640124 + 1);
printf("%d\n", 118260 - 82402 + 1);
printf("%d\n", 38677314 - 38675983 + 1);
printf("%d\n", 1332);
printf("%d\n", 38696943 - 38677315+ 1);
printf("%d\n", 19629 - 1 + 1); 
printf("<<<\n");
printf("%d vs %d\n", 139221 - 82402 + 1, 38696943 - 38640124 + 1);

printf("%d vs %d\n",
	118260 - 82402 + 1  + 1332 + 19629 - 1 + 1,
	38675982 - 38640124 + 1 + 38677314 - 38675983 + 1 + 38696943 - 38677315 + 1);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 )
    usage();
freen(argv[1]);
return 0;
}
