/* geoMirrorNode - Maps a list of IPs to their default geoMirror node. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "geoMirror.h"
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geoMirrorNode - Maps a list of hosts and/or IPs to their default geoMirror node; tab-delimited output is:\n\n"
  "host/ip node domain\n\n"
  "usage:\n"
  "   geoMirrorNode files(s)\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void processFiles(int fileCount, char *files[])
/* processFiles - Maps a list of IPs to their default geoMirror node. */
{
struct sqlConnection *centralConn = hConnectCentral();
int i, numNodes;
char **nodes = NULL;
char query[1024];
char **row;
struct sqlResult *sr;

char *thisNodeStr = geoMirrorNode();
if(thisNodeStr == NULL)
    errAbort("geoMirrors are not configured in this installation");

// make a lookup list for nodes so we can print out domains as well as nodes.

sqlSafef(query, sizeof query, "select max(node) from gbNode");
numNodes = sqlQuickNum(centralConn, query);
if(numNodes == 0)
    // This shouldn't happen, but just being really careful
    errAbort("Can't find any nodes in gbNode");

nodes = needMem(sizeof(int) * numNodes);
sqlSafef(query, sizeof query, "SELECT node, domain FROM gbNode ORDER BY CAST(node AS UNSIGNED)");
sr = sqlGetResult(centralConn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int node = sqlSigned(row[0]);
    nodes[node - 1] = cloneString(row[1]);
    }
sqlFreeResult(&sr);

for (i=0; i<fileCount; i++)
    {
    struct lineFile *lf = lineFileOpen(files[i], TRUE);
    char *host;
    while (lineFileNextReal(lf, &host))
        {
        struct hostent *hostent = gethostbyname(host);
        if(hostent == NULL)
            errAbort("gethostbyname failed for '%s'; errno: %d (%s)", host, errno, strerror(errno));
        if(hostent->h_length == 0)
            errAbort("gethostbyname failed for '%s'; h_length == 0", host);
        char *ip = inet_ntoa(*((struct in_addr *) hostent->h_addr_list[0]));
        int node = defaultNode(centralConn, ip);
        printf("%s\t%d\t%s\n", host, node, nodes[node - 1]);
        }
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
processFiles(argc-1, argv+1);
return 0;
}
