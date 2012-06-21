/* geoMirror - support geographic based mirroring (e.g. euronode) */

#include "common.h"
#include "geoMirror.h"
#include "hgConfig.h"
#include "internet.h"

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

int defaultNode(struct sqlConnection *centralConn, char *ipStr)
// return default node for given IP
{
char query[1024];
bits32 ip = 0;
int defaultNode = 1;
internetDottedQuadToIp(ipStr, &ip);

// We (sort-of) assume no overlaps in geoIpNode table, so we can use limit 1 to make query very efficient;
// we do accomodate a range that is completely contained in another (to accomodate the hgroaming entry for testing);
// this is accomplished by "<= ipEnd" in the sql query.
safef(query, sizeof query, "select ipStart, ipEnd, node from geoIpNode where %u >= ipStart and %u <= ipEnd order by ipStart desc limit 1", ip, ip);
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
