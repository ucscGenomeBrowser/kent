/* dateXmlCgi - A cgi that just returns date in XML. */
#include "common.h"
#include "cheapcgi.h"



int main(int argc, char *argv[])
/* Process command line. */
{
time_t now = time(NULL);
cgiPrintContentType("application/xml");
puts("<?xml version='1.0'?>");
puts("<dateXmlCgi>");
printf(" <date>%s</date>\n", ctime(&now));
puts("</dateXmlCgi>");
return 0;
}
