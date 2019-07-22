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
#include "jksql.h"
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
    "%s GeoLite2-Country-Blocks-IPv6n4.csv > geoIpCountry6.tab\n"
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


void upDateAndTestForSizeExceeded(int *j, int encSize)
{
(*j)++;
if (*j > encSize)
    errAbort("output buffer size exceeded %d in minimalDumpEncoder", encSize);
}

void minimalDumpEncoder(char *s, int size, char *enc, int encSize)
/* Perform minimal encoding for tab dumped mysql files. Free the reesult. */
{
char c;
int i;
int j = 0;
for (i = 0; i < size; ++i)
    {
    c = s[i];
    if (
	(c == '\\') ||
	(c == '\t') ||
	(c == '\n') ||
	(c == 0)
       )
	{
	*enc++ = '\\';
	upDateAndTestForSizeExceeded(&j, encSize);
	}
    if (c == 0)
	c = '0';
    *enc++ = c;
    upDateAndTestForSizeExceeded(&j, encSize);
    }
*enc++ = 0; // terminate string.
upDateAndTestForSizeExceeded(&j, encSize);
}

void geoIpToCountry(char *fileName) 
/* List each field in tab-separated file on a new line, dashed-lines separate records */
{

// No need to sort the input since the ipv6 addresses all come after ipv4,
// and the 2 .csv files are already sorted.
// I could have used my ipv6 address sorting routine for cmp function to sort however.

struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
char *words[MAXWORDS];
int wordCount;
int lineCount = 0;
struct in6_addr lastEndIp;
ZeroVar(&lastEndIp);
boolean firstTime = TRUE;
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
    verbose(2, "network=%s\n", network);


    struct cidr *subnet = internetParseSubnetCidr(network);

    verbose(2, "subnet->subnetLength=%d\n", subnet->subnetLength);


    struct in6_addr startIp, endIp;

    internetCidrRange(subnet, &startIp, &endIp);


    char startIpS[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &startIp.s6_addr, startIpS, sizeof(startIpS)) < 0) 
	errAbort("inet_ntop failed for ipv6 address.");
    verbose(2, "startIpS (ntop) %s\n", startIpS);

    char endIpS[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &endIp.s6_addr, endIpS, sizeof(endIpS)) < 0) 
	errAbort("inet_ntop failed for ipv6 address.");
    verbose(2, "  endIpS (ntop) %s\n", endIpS);


    char startIpHex[33];
    ip6AddrToHexStr(&startIp, startIpHex, sizeof startIpHex);
    verbose(2, "             startIp as hex: %s\n", startIpHex);

    char   endIpHex[33];
    ip6AddrToHexStr(&endIp, endIpHex, sizeof endIpHex);
    verbose(2, "               endIp as hex: %s\n", endIpHex);

    // get country info
    char *geoname_id = words[1];
    char *registered_country_geoname_id = words[2];

    verbose(2, " geoname_id %s\n", geoname_id);
    char *countryCode = NULL;
    struct hashEl *el = hashLookup(locHash, geoname_id);
    if (el)
	{
	countryCode = el->val;
	}
    else
	{
 	el = hashLookup(locHash, registered_country_geoname_id);
	if (el)
	    {
	    countryCode = el->val;
	    }
	else
	    {
	    warn("%s missing %s and %s in location lookup, substituting US", network, geoname_id, registered_country_geoname_id);
	    countryCode = "US";
	    }
	}

    verbose(2, "%s\t%s\t%s\n", startIpHex, endIpHex, countryCode); 

    // final output should be geoIpCountry6.tab
    // but with ipv6 addresses in it. 
    // And in MySQL dump format! just use mysql escape string function

    // largest possible expansion = 2*16 + 1 = 33
    char startIpEnc[33];
    char   endIpEnc[33];  
    minimalDumpEncoder((char *)&startIp, 16, startIpEnc, sizeof startIpEnc);
    minimalDumpEncoder((char *)  &endIp, 16,   endIpEnc, sizeof   endIpEnc);

    printf("%s\t%s\t%s\n", startIpEnc, endIpEnc, countryCode); 
    if (verboseLevel() > 1)
	fflush(stdout); // so we can mix stdout with verbose output correctly.

    // Make sure the input ranges are ascending and non-overlapping.
    if (firstTime)
	{
	firstTime = FALSE;
	}
    else
	{
	if (ip6AddrCmpBits(&startIp, &lastEndIp) <= 0)
	    errAbort("startIp is not greater than last endIp. Should be ascending and non-overlapping.");
	
	}
     ip6AddrCopy(&endIp, &lastEndIp);

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


