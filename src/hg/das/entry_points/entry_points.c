/* entry_points - DAS entry point server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "entry_points - DAS entry point server\n"
  "usage:\n"
  "   entry_points XXX\n"
  );
}

void dasHeader(int err)
/* Write out very start of DAS header */
{
puts("Content-Type:text/plain");
puts("X-DAS-Version: DAS/0.95");
printf("X-DAS-Status: %d\n", err);
puts("\n");
if (err != 200)
    exit(-1);
}


void entry_points()
/* entry_points - DAS entry point server. */
{
dasHeader(200);
printf(
"<?xml version=\"1.0\" standalone=\"no\"?>\n"
"     <!DOCTYPE DASGFF SYSTEM \"dasgff.dtd\">\n");
printf("<DASEP>\n");
printf("</DASEP>\n");

}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
entry_points();
return 0;
}
