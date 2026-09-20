/* geoMirrorSelfTester - check which gbNode row a server takes for itself.
 *
 * The browser runs on several machines and each one offers the others in a menu, so it has to
 * know which of the gbNode rows is itself.  It used to answer that from browser.node in
 * hg.conf alone.  #27988: a machine serving a node other than the one browser.node names --
 * a sandbox, or two nodes behind one apache -- then took itself for its own peer and offered
 * the visitor a link to the site they were already on.
 *
 * Now the host the visitor typed decides, whenever that host is one of the nodes, and
 * browser.node is the fallback for a host that is in no gbNode row at all.
 *
 * The failure is invisible in the way that matters: every page renders, and the only symptom
 * is a menu entry pointing back at itself, which reads as a mirror being unavailable rather
 * than as a bug.
 *
 * Nothing here names a domain.  gbNode is configuration data and its rows change, so the test
 * reads two rows, points browser.node at the first, and asks whether the second can claim the
 * server.  What is printed is which of the two came back, not what they are called.
 *
 * refs #27988 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <stdlib.h>
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "geoMirror.h"

static char *configuredDomain = NULL;   /* the domain of the node browser.node names */
static char *otherDomain = NULL;        /* some other node's domain */

static void readTwoNodes()
/* Two rows of gbNode: the one browser.node names, and any other one. */
{
char *node = geoMirrorNode();
if (node == NULL)
    errAbort("browser.node is not set, so this test has nothing to ask about");
struct sqlConnection *conn = hConnectCentral();
char query[256];
sqlSafef(query, sizeof query, "select domain from gbNode where node = '%s'", node);
configuredDomain = sqlQuickString(conn, query);
sqlSafef(query, sizeof query, "select domain from gbNode where node != '%s' order by node "
         "limit 1", node);
otherDomain = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
if (isEmpty(configuredDomain) || isEmpty(otherDomain))
    errAbort("gbNode needs a row for browser.node and at least one other row");
}

static char *nameFor(char *domain)
/* Say which of the two a domain is, so no domain is printed. */
{
if (domain == NULL)
    return "(none)";
if (sameString(domain, configuredDomain))
    return "the node browser.node names";
if (sameString(domain, otherDomain))
    return "the other node";
return "a third node";
}

static void ask(char *what, char *httpHost)
/* Set HTTP_HOST as apache would, and report which node the server calls itself. */
{
if (httpHost == NULL)
    unsetenv("HTTP_HOST");
else
    setenv("HTTP_HOST", httpHost, 1);

struct slPair *self = geoMirrorThisNode();
struct slPair *others = geoMirrorOtherNodes();
printf("  %-44s self=%-32s others=%d\n", what,
       nameFor(self == NULL ? NULL : (char *)self->val), slCount(others));
}

int main(int argc, char *argv[])
{
readTwoNodes();
printf("browser.node names one row; another row exists\n");

ask("no HTTP_HOST at all, as on the command line", NULL);
ask("HTTP_HOST is the configured node", configuredDomain);

/* The case #27988 is about. */
char *other = otherDomain;
ask("HTTP_HOST is the OTHER node", other);

char withPort[256];
safef(withPort, sizeof withPort, "%s:8443", other);
ask("the other node with a port on it", withPort);

ask("a host in no gbNode row", "hgwdev-nobody.gi.ucsc.edu");
ask("an empty HTTP_HOST", "");

printf("\n");
return 0;
}
