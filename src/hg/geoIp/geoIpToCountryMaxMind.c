/* program geoIpToCountryMaxMind 
 * by Galt Barber 2011-04-15
 * Read csv input geoip data (free from MaxMind) and 
 * output geoIpCountry.tab to load into hgFixed for testing,
 * and hgcentral for use with genome-browser cgis 
 * to map user IP addresses to country-code. */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "sqlNum.h"
#include "hash.h"
#include "obscure.h"
#include "csv.h"
#include "net.h"
#include "internet.h"

#define MAXWORDS 1024

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"-help"    , OPTION_BOOLEAN},
    {NULL, 0}
};

struct hash *locHash = NULL;

void usage(char *p) 
/* display correct usage/syntax */
{
errAbort("Usage:\n"
    "%s GeoLite2-Country-Blocks-IPv4.csv\n"
    "Processes the IP ranges from decimal form to IP-string form, ignores comments and country-code ZZ Reserved.\n"
    ,p);
}

void readLocations()
/* read locations data into hash */
{
locHash = hashNew(10);
char *locName = "GeoLite2-Country-Locations-en.csv";
// format: 7909807,en,AF,Africa,SS,"South Sudan",0 

struct lineFile *lf = lineFileOpen(locName, TRUE);
int lineSize;
char *line;
int wordCount;
int lineCount = 0;
while (lineFileNext(lf, &line, &lineSize))
    {
    struct slName *list = csvParse(line);
    // cannot use chopByString since the input has empty strings and they get skipped.
    wordCount = slCount(list);
    if (wordCount != 7)
	errAbort("Invalid row found, wordCount = %d != 7", wordCount);
    if (lineCount++ == 0) // 1st line is a comment
	continue;
    struct slName *f;
    int fnum = 0;
    char *geoname_id = NULL;
    char *countryCode = NULL;
    char *continentCode = NULL;  // lamely, 64 records have blank countryCode, but they do give continent codes.
    for (f=list, fnum=0; f; f=f->next, ++fnum)
	{
	if (fnum == 0)
	    geoname_id = f->name;

	if (fnum == 4)
	    countryCode = f->name;

	if (fnum == 2)  // just for lame blank ones.
	    continentCode = f->name;
	}

    //printf(" geoname_id %s ", geoname_id);
    //printf("countryCode %s \n", countryCode);

    if (sameString(countryCode, ""))
	{
	if (sameString(continentCode, "AS"))  // asia
	    {
	    countryCode = "JP";  // fake some other country in that continent, japan
	    }
	else if (sameString(continentCode, "EU"))  // asia
            {
            countryCode = "DE";  // fake some other country in that continent, germany
            }
	else
	    errAbort("unexpected country code is empty string in line #%d", lineCount);
	}

    hashAdd(locHash, geoname_id, countryCode);

    }

lineFileClose(&lf);
}

void parseCIDR(char *cidr, bits32 *pStartIp, bits32 *pEndIp)
/* parse input CIDR format IP range (or subnet) */
{
struct cidr *subnet = internetParseSubnetCidr(cidr);

internetCidrRange(subnet, pStartIp, pEndIp);

}



void geoIpToCountry(char *fileName) 
/* List each field in tab-separated file on a new line, dashed-lines separate records */
{

struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
char *words[MAXWORDS];
int wordCount;
int lineCount = 0;
while (lineFileNext(lf, &line, &lineSize))
    {

    // input format
    // network,geoname_id,registered_country_geoname_id,represented_country_geoname_id,is_anonymous_proxy,is_satellite_provider
    // 1.0.0.0/24,2077456,2077456,,0,0


    // cannot use chopByString since the input has empty strings and they get skipped.
    wordCount = chopByChar(line, ',', words, MAXWORDS);
    if (wordCount != 6)
	errAbort("Invalid row found, wordCount = %d != 6", wordCount);
    if (lineCount++ == 0) // 1st line is a comment
	continue;

    // get network info
    char *network = words[0];

    bits32 startIp, endIp;

    parseCIDR(network, &startIp, &endIp);

    // Handy for debugging
    //char startIpS[17];
    //char endIpS[17];
    //internetIpToDottedQuad(startIp, startIpS);
    //internetIpToDottedQuad(endIp, endIpS);
    //printf("dottedQuad start %s end %s\n", startIpS, endIpS);

    // get country info
    char *geoname_id = words[1];
    char *registered_country_geoname_id = words[2];

    //printf("network %s ", network);
    //printf(" geoname_id %s\n", geoname_id);
    struct hashEl *el = hashLookup(locHash, geoname_id);
    if (!el)
	{
 	el = hashLookup(locHash, registered_country_geoname_id);
	if (!el)
	    {
	    warn("%s missing %s and %s in location lookup", network, geoname_id, registered_country_geoname_id);
	    continue;
	    }
	}
    char *countryCode = el->val;


    printf("%u\t%u\t%s\n", startIp, endIp, countryCode);

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
readLocations();
geoIpToCountry(fileName);
return 0; 
} 


