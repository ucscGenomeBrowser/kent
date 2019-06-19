/* program geoIpToCountryMaxMind 
 * by Galt Barber 2011-04-15
 * Read csv input geoip data and output format for use with genome-browser cgis 
 * to map user IP addresses to country-code. */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "sqlNum.h"
#include "hash.h"
#include "obscure.h"
#include "csv.h"
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

bits32 internetPackIp(unsigned char unpacked[4])
/* Convert from 4-byte format with most significant
 * byte first to native 32-bit format. */
{
int i;
bits32 packed = 0;
for (i=0; i<=3; ++i)
    {
    packed <<= 8;
    packed |= unpacked[i];
    }
return packed;
}

void parseCIDR(char *cidr, bits32 *pStartIp, bits32 *pEndIp)
/* parse input CIDR format IP range (or subnet) */
{
char *s = cloneString(cidr);
char *c = strchr(s, '/');
if (!c)
    errAbort("expected slash char '/' in input cidr %s\n", cidr);
*c++ = 0;
char *ip = s;
unsigned int bits = sqlUnsigned(c);
//printf("ip=%s, bits=%d \n", ip, bits);   // DEBUG REMOVE
unsigned char quadIp[4];
internetParseDottedQuad(ip, quadIp);
//int i;
//for(i=0;i<4;++i)
//    printf("ip[%d]=%d\n", i, quadIp[i]);   // DEBUG REMOVE
bits32 packedIp = 0;
packedIp = internetPackIp(quadIp);  // TODO should this go in the library internet.c? 
//printf("packed32 bits=%u %08x\n", packedIp, packedIp);   // DEBUG REMOVE
int r = 32 - bits;
bits32 start = packedIp & (((unsigned int) 0xFFFFFFFF) << r);
bits32 end;
// on this platform shr or shl 32 of a 32-bit value actually does nothing at all rather than turning it to 0s.
if (bits == 32)
    end = packedIp;
else
    end = packedIp | (((unsigned int) 0xFFFFFFFF) >> bits);
//printf("start=%u %08x\n", start, start);   // DEBUG REMOVE
//printf("end  =%u %08x\n", end,   end  );   // DEBUG REMOVE

char startIpS[17];
char endIpS[17];
internetIpToDottedQuad(start, startIpS);
internetIpToDottedQuad(end, endIpS);

//printf("dottedQuad start %s end %s\n", startIpS, endIpS);

*pStartIp = start;
*pEndIp = end;

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


    //if (!sameString(countryCode, "ZZ")) // Filter out Reserved Ip ranges
    printf("%u\t%u\t%s\n", startIp, endIp, countryCode);

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
readLocations();
geoIpToCountry(fileName);
return 0; 
} 


