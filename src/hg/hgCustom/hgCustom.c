/* hgCustom - CGI Script that lets you put in your own .bed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - CGI Script that lets you put in your own .bed file.\n"
  "usage:\n"
  "   hgCustom XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgCustom(char *XXX)
/* hgCustom - CGI Script that lets you put in your own .bed file.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgCustom(argv[1]);
return 0;
}
