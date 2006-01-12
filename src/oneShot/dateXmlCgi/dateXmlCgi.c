/* dateXmlCgi - A cgi that just returns date in XML. */
#include "common.h"

static char const rcsid[] = "$Id: dateXmlCgi.c,v 1.1 2005/12/27 18:04:29 kent Exp $";


int main(int argc, char *argv[])
/* Process command line. */
{
time_t now = time(NULL);
puts("Content-Type:application/xml");
puts("");
puts("<?xml version='1.0'?>");
puts("<dateXmlCgi>");
printf(" <date>%s</date>\n", ctime(&now));
puts("</dateXmlCgi>");
return 0;
}
