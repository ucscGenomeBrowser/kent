/* dsn - DSN Server for DAS. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dsn - DSN Server for DAS\n"
  "usage:\n"
  "   dsn XXX\n"
  );
}

void dsn()
/* dsn - DSN Server for DAS. */
{
puts("Content-Type:text/plain");
puts("X-DAS-Version: DAS/0.95");
puts("X-DAS-Status: 200");
puts("\n");
puts("<?xml version=\"1.0\" standalone=\"no\"?>\n"
" <!DOCTYPE DASDSN SYSTEM \"dasdsn.dtd\">\n"
" <DASDSN>\n"
"   <DSN>\n"
"     <SOURCE id=\"ucscSplicedEsts\" version=\"hg7\">UCSC Spliced ESTs</SOURCE>\n"
"     <MAPMASTER>http://servlet.sanger.ac.uk:8080/das/ensembl110</MAPMASTER>\n"
"     <DESCRIPTION>UCSC Spliced ESTs on April 2001</DESCRIPTION>\n"
"   </DSN>\n"
" </DASDSN>\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
dsn();
return 0;
}
