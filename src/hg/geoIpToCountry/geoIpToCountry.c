/* program geoIpToCountry
 * by Galt Barber 2023-06-21
 * use geoIp innfrastructure and hgcentral.geoIpCountry6 table 
 * to quicklyy map IP address to country-code. */

#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hgConfig.h"

#include "hdb.h"
#include "geoMirror.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"help"    , OPTION_BOOLEAN},
    {"twoLetterCodes"    , OPTION_STRING},
    {NULL, 0}
};

// could use this hash to avoid duplicate processing
static struct hash *locHash = NULL;

// may have country code translation here
static struct hash *countryCode = NULL;

void usage(char *p) 
/* display correct usage/syntax */
{
errAbort("Usage:\n"
    "%s <options> IPaddress\n"
    "    IP address may be IPv4 or IPv6.\n"
    "    Or, it can be a file name with a list of addresses to check.\n"
    "options:\n"
    " -twoLetterCodes <file.tsv> - to provide translation of two letter codes\n"
    "                              to full country names."
    ,p);
}


int main (int argc, char *argv[]) 
{
char *ipStr=NULL;
optionInit(&argc, argv, optionSpecs);
if ((argc != 2) || optionExists("help"))
    usage(argv[0]);

struct sqlConnection *centralConn = hConnectCentral();

char *codes = optionVal("twoLetterCodes", NULL);
ipStr=argv[1];
boolean fileList = isRegularFile(ipStr);
if (codes)
    {
    char *words[2];
    struct lineFile *lf = lineFileOpen(codes, TRUE);
    verbose(2, "# reading country code translations: %s\n", codes);
    countryCode = newHash(0);
    while (lineFileNextRowTab(lf, words, ArraySize(words)))
	{
	hashAdd(countryCode, words[1], cloneString(words[0]));
        verbose(3, "# %s == %s\n", words[1], words[0]);
	}
    lineFileClose(&lf);
    }


char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");
char fullTableName[1024];
safef(fullTableName, sizeof fullTableName, "%s%s", "geoIpCountry6", geoSuffix);
if (!sqlTableExists(centralConn, fullTableName))
    {
    errAbort("geoIpCountry6 table not found in hgcentral.\n");
    }


if (fileList)
    {
    locHash = newHash(0);
    char *line;
    long long lineCount = 0;
    long long checked = 0;
    long long failed = 0;
    struct lineFile *lf = lineFileOpen(ipStr, TRUE);
    verbose(2, "# processing file list: %s\n", ipStr);
    verboseTimeInit();
    while (lineFileNextReal(lf, &line))
	{
        ++lineCount;
        if (hashIncInt(locHash, line) > 1)	// avoid duplicates
	    continue;
        ++checked;
	char *country = geoMirrorCountry6(centralConn, line);
	if (strlen(country)==2)  // ok
	    {
	    if (countryCode)
		{
		char *name = hashFindVal(countryCode, country);
		printf("%s\t%s\t'%s'\n", country, line, name);
		}
	    else
		printf("%s\t%s\n", country, line);
	    }
        else
	    {
	    printf("n/a\t%s\tfailed\n", line);
	    ++failed;
            }
        freeMem(country);
	}
    lineFileClose(&lf);
   verboseTime(0, "# processed %lld lines, checked %lld addresses, %lld failed", lineCount, checked, failed);
    }
else
    {
    char *country = geoMirrorCountry6(centralConn, ipStr);
    if (strlen(country)==2)  // ok
	{
	if (countryCode)
	    {
	    char *name = hashFindVal(countryCode, country);
	    printf("%s\t%s\t%s\n", country, ipStr, name);
	    }
	else
	    printf("%s\t%s\n", country, ipStr);
	}
    else
	errAbort("unable to read IP address for %s, error message was: %s", ipStr, country);
    freeMem(country);
    }

// Leave it to others to extend it to process a list of things.

// Example code for hostName lookup is in kent/src/utils/dnsInfo/dnsInfo.c

return 0; 
} 


