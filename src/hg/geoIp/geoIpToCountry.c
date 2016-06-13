/* program geoIpToCountry 
 * by Galt Barber 2011-04-15
 * Read csv input geoip data and output format for use with genome-browser cgis 
 * to map user IP addresses to country-code. */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "sqlNum.h"
#include "internet.h"

#define MAXWORDS 1024

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"-help"    , OPTION_BOOLEAN},
    {NULL, 0}
};


void usage(char *p) 
/* display correct usage/syntax */
{
errAbort("Usage:\n"
    "%s geoipCvsFile\n"
    "Processes the IP ranges from decimal form to IP-string form, ignores comments and country-code ZZ Reserved.\n"
    ,p);
}


void padDottedQuad(char dottedQuad[17])
/* pad quads with leading 0 to length of 3 */
{
unsigned char quad[4];
internetParseDottedQuad(dottedQuad, quad);
safef(dottedQuad, 17, "%03u.%03u.%03u.%03u",
 quad[0] , quad[1] , quad[2] , quad[3]);
}


void geoIpToCountry(char *fileName) 
/* List each field in tab-separated file on a new line, dashed-lines separate records */
{

struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
char *words[MAXWORDS];
int wordCount;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopString(line, ",", words, MAXWORDS);
    if (wordCount != 7)
	errAbort("Invalid row found, wordCount != 7");
    bits32 startIp = sqlUnsigned(stripEnclosingChar(words[0],'"'));
    bits32 endIp = sqlUnsigned(stripEnclosingChar(words[1],'"'));
    char *countryCode = stripEnclosingChar(words[4],'"');

    char startIpQuad[17];
    internetIpToDottedQuad(startIp, startIpQuad);
    padDottedQuad(startIpQuad);
    char endIpQuad[17];
    internetIpToDottedQuad(endIp, endIpQuad);
    padDottedQuad(endIpQuad);

    if (!sameString(countryCode, "ZZ")) // Filter out Reserved Ip ranges
	printf("%s\t%s\t%s\n", startIpQuad, endIpQuad, countryCode);
    //printf("start IP %s %u %s\n", words[0], startIp, startIpQuad);
    //printf("  end IP %s %u %s\n", words[1], endIp, endIpQuad);
    //printf("country code %s\n", countryCode);
    //printf("----------------------------------------\n");
    }

lineFileClose(&lf);
}



int main (int argc, char *argv[]) 
{
char *fileName="stdin";
optionInit(&argc, argv, optionSpecs);
if ((argc != 2) || optionExists("-help"))
    usage(argv[0]);
fileName=argv[1];
geoIpToCountry(fileName);
return 0; 
} 


