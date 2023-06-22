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
    {"-help"    , OPTION_BOOLEAN},
    {NULL, 0}
};

struct hash *locHash = NULL;

void usage(char *p) 
/* display correct usage/syntax */
{
errAbort("Usage:\n"
    "%s IPaddress\n"
    "IP address may be IPv4 or IPv6.\n"
    ,p);
}


int main (int argc, char *argv[]) 
{
char *ipStr=NULL;
optionInit(&argc, argv, optionSpecs);
if ((argc != 2) || optionExists("-help"))
    usage(argv[0]);

ipStr=argv[1];

struct sqlConnection *centralConn = hConnectCentral();

char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");
char fullTableName[1024];
safef(fullTableName, sizeof fullTableName, "%s%s", "geoIpCountry6", geoSuffix);
if (!sqlTableExists(centralConn, fullTableName))
    {
    errAbort("geoIpCountry6 table not found in hgcentral.\n");
    }

char *country = geoMirrorCountry6(centralConn, ipStr);
if (strlen(country)==2)  // ok
    printf("%s\n", country);
else
    errAbort("unable to read IP address for %s, error message was: %s", ipStr, country);

// Leave it to others to extend it to process a list of things.

// Example code for hostName lookup is in kent/src/utils/dnsInfo/dnsInfo.c

return 0; 
} 


