/* geoMirror - support geographic based mirroring (e.g. euro and asia nodes) */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "geoMirror.h"
#include "hgConfig.h"
#include "internet.h"

/* geographic server (mirror) support

   Customize 'Mirrors' drop-down menu based on current server.  If the server
   is a UCSC-sponsored site (USA/Ca or European), show this in drop-down menu
   with a checkmark, and allow user to change server.  Based on design notes here:
        http://genomewiki.ucsc.edu/genecats/index.php/Euronode

   NOTE: Uses hgcentral.gbNode table
   to populate menu items and locate redirects.  This implementation uses the
   spec from above wiki page:
        browser.node=1 -> US server (genome.ucsc.edu)
        browser.node=2 -> European server (genome-euro.ucsc.edu)
        browser.node=3 -> Asian server (genome-asia.ucsc.edu)


   For Testing, use browser.geoSuffix: e.g.
        browser.geoSuffix=Test
   will cause it to use in hgcentral the tables  gbNodeTest and geoIpNodeTest instead.
   See the genomewiki link above for more documentation on testing.

*/


boolean geoMirrorEnabled()
{
// return TRUE if this site has geographic mirroring turned on
return geoMirrorNode() != NULL;
}

char *geoMirrorNode()
{
// return which geo mirror node this is (or NULL if geo mirroring is turned off)
return cfgOption("browser.node");
}


int geoMirrorDefaultNode4(struct sqlConnection *centralConn, char *ipStr)
// return default node for given IP
{
char query[1024];
bits32 ip = 0;
int defaultNode = 1;

if (isIpv6Address(ipStr))  // ipv6 which we do not handle yet. TODO
    return defaultNode;  // at least tolerate IPV6 remote addr

internetDottedQuadToIp(ipStr, &ip);

char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");

// We (sort-of) assume no overlaps in geoIpNode table, so we can use limit 1 to make query very efficient;
// we do accomodate a range that is completely contained in another (to accomodate the hgroaming entry for testing);
// this is accomplished by "<= ipEnd" in the sql query. 
// TODO The hgroaming thing is probably obsolete and testing is done with browser.geoSuffix= instead.
// If so, we may wish to remove the loop below since that was added by Larry and reformulate
// it as it was originally done by Galt.  However it does not seem to affect performance so we can leave it for now.

sqlSafef(query, sizeof query, 
    "select ipStart, ipEnd, node from geoIpNode%s where %u >= ipStart and %u <= ipEnd order by ipStart desc limit 1"
    , geoSuffix, ip, ip);
char **row;
struct sqlResult *sr = sqlGetResult(centralConn, query);

if ((row = sqlNextRow(sr)) != NULL)
    {
    uint ipStart = sqlUnsigned(row[0]);
    uint ipEnd = sqlUnsigned(row[1]);
    if (ipStart <= ip && ipEnd >= ip)
        {
        defaultNode = sqlSigned(row[2]);
        }
    }
sqlFreeResult(&sr);

return defaultNode;
}
 
int geoMirrorDefaultNode6(struct sqlConnection *centralConn, char *ipStr)
// return default node for given IP
{
char query[1024];
char newIpStr[NI_MAXHOST];
struct in6_addr ip;
ZeroVar(&ip);
char ipHex[33];

int defaultNode = 1;

if (isIpv6Address(ipStr))
    safef(newIpStr, sizeof newIpStr, "%s", ipStr);
else if (isIpv4Address(ipStr))
    safef(newIpStr, sizeof newIpStr, "%s%s", IPV4MAPPED_PREFIX, ipStr); // "::ffff:"
else 
    {
    warn("Unexpected strange ip address string: %s", ipStr);
    return defaultNode;
    }

if (!internetIpStringToIp6(newIpStr, &ip))
    errAbort("internetIpStringToIp6 failed for %s", ipStr);

ip6AddrToHexStr(&ip, ipHex, sizeof ipHex);

char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");

// We (sort-of) assume no overlaps in geoIpNode table, so we can use limit 1 to make query very efficient;
// we do accomodate a range that is completely contained in another (to accomodate the hgroaming entry for testing);
// this is accomplished by "<= ipEnd" in the sql query. 
// TODO The hgroaming thing is probably obsolete and testing is done with browser.geoSuffix= instead.
// If so, we may wish to remove the loop below since that was added by Larry and reformulate
// it as it was originally done by Galt.  However it does not seem to affect performance so we can leave it for now.

sqlSafef(query, sizeof query, 
    "select ipStart, ipEnd, node from geoIpNode6%s where unhex('%s') >= ipStart and unhex('%s') <= ipEnd order by ipStart desc limit 1"
    , geoSuffix, ipHex, ipHex);
char **row;
struct sqlResult *sr = sqlGetResult(centralConn, query);

if ((row = sqlNextRow(sr)) != NULL)
    {
    struct in6_addr ipStart;
    ip6AddrCopy((struct in6_addr *)row[0], &ipStart);

    struct in6_addr ipEnd;
    ip6AddrCopy((struct in6_addr *)row[1], &ipEnd  );

    if (
	(ip6AddrCmpBits(&ipStart, &ip) <= 0)
	&&
	(ip6AddrCmpBits(&ipEnd  , &ip) >= 0)
       )
        {
        defaultNode = sqlSigned(row[2]);
        }
    }
sqlFreeResult(&sr);

return defaultNode;
}

int geoMirrorDefaultNode(struct sqlConnection *centralConn, char *ipStr)
// return default node for given IP
{
char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");
char fullTableName[1024];
safef(fullTableName, sizeof fullTableName, "%s%s", "geoIpNode6", geoSuffix);
int defaultNode = 1;
if (sqlTableExists(centralConn, fullTableName))
    {
    defaultNode = geoMirrorDefaultNode6(centralConn, ipStr);
    }
else
    {
    defaultNode = geoMirrorDefaultNode4(centralConn, ipStr);
    }
return defaultNode;
}

char *geoMirrorMenu()
/* Create customized geoMirror menu string for substitution of  into 
 * <!-- OPTIONAL_MIRROR_MENU --> in htdocs/inc/globalNavBar.inc 
 * Reads hgcentral geo tables and hg.conf settings. 
 * Free the returned string when done. */
{
struct dyString *dy = dyStringNew(0);  // by default replacment is just an empty string.
if (geoMirrorEnabled())
    {
    dyStringAppend(dy, "<li id=\"geoMirrorMenu\" class=\"noHighlight\"><hr></li>\n");
    // Geo mirror functionality (e.g. in nav bar)
    char *myNode = geoMirrorNode();
    /* hgcentral.gbNode table so UI can share w/ browser GEO mirror redirect code */
    char **row = NULL;
    struct sqlConnection *conn = hConnectCentral();  // after hClade since it access hgcentral too
    // get the gbNode table
    char *geoSuffix = cfgOptionDefault("browser.geoSuffix","");
    char query[256];
    sqlSafef(query, sizeof query, "SELECT node, domain, shortLabel from gbNode%s order by node", geoSuffix);
    struct sqlResult *sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *node = row[0];
	char *domain = row[1];
	char *shortLabel = row[2];
        dyStringPrintf(dy, "<li id=\"server%s\"", node);
        if (sameString(node, myNode))
            dyStringAppend(dy, " class=\"noHighlight\"");
        dyStringAppend(dy, ">\n");
        dyStringAppend(dy, "<img alt=\"X\" width=\"16\" height=\"16\" style=\"float:left;");
        if (!sameString(node, myNode))
            dyStringAppend(dy, "visibility:hidden;");
        dyStringAppend(dy, "\" src=\"../images/greenChecksmCtr.png\">\n");
        if (!sameString(node, myNode))
            dyStringPrintf(dy, "<a href=\"http://%s/cgi-bin/hgGateway?redirect=manual\">", domain);
        dyStringPrintf(dy, "%s", shortLabel);
        if (!sameString(node, myNode))
            dyStringAppend(dy, "</a>");
        dyStringAppend(dy, "</li>\n");

	}
    sqlFreeResult(&sr);
    hDisconnectCentral(&conn);
    }
return dyStringCannibalize(&dy);
}
