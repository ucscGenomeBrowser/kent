/* hubSpaceKeysTester - check how an api key is stored, replaced, adopted and revoked.
 *
 * An api key lets hubtools upload to hubSpace and lets a script past cloudflare, so the rules
 * about it are security rules: one key per user, a new key revokes the old one, and a revoked
 * key names nobody.  #38323 added the adopt path, hubSpaceSetApiKey, so that a key made on one
 * geo mirror works on all of them instead of the user finding their key rejected on whichever
 * mirror geography sent them to.
 *
 * Nothing here is visible.  A key that quietly still works after being replaced, or a revoke
 * that does not take, looks exactly like a key that behaves, right up until somebody uses the
 * old one.
 *
 * This is the first test in the tree that WRITES to a central database, so it writes to
 * hgcentralregress, which exists for tests and which nothing else reads.  It creates its own
 * rows and removes them at both ends, since the next run must not depend on what this one
 * left.  See makeHgCentralRegress.sh beside this file.
 *
 * The keys themselves are random, so nothing prints one.  What is printed is the relationship
 * between them -- same, different, gone -- which is what the rules are about.
 *
 * refs #38323 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "errCatch.h"
#include "hubSpaceKeys.h"

/* Recognisable in a stray row, and not a name anyone would log in with. */
#define TEST_USER "regressTest@example.org"
#define PEER_KEY "a-key-another-mirror-made"

static void cleanUp()
/* Take this test's rows out again, at both ends of the run. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query, "delete from %s where userName = '%s'",
         cfgOptionDefault("authTableName", AUTH_TABLE_DEFAULT), TEST_USER);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static void refuseNull(char *what, char *userName, char *apiKey, boolean isSet)
/* The calls that must abort rather than write a row with a hole in it. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (isSet)
        hubSpaceSetApiKey(userName, apiKey);
    else
        hubSpaceGenerateApiKey(userName);
    }
errCatchEnd(errCatch);
printf("  %-44s %s\n", what, errCatch->gotError ? "aborted" : "went through -- FAIL");
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
{
if (!sameString(cfgOption("central.db"), "hgcentralregress"))
    errAbort("this test writes rows, so it must be pointed at hgcentralregress, not %s",
             cfgOption("central.db"));
cleanUp();

printf("a new key\n");
char *first = cloneString(hubSpaceGenerateApiKey(TEST_USER));
printf("  %-44s %s\n", "the key comes back for that user",
       sameOk(first, hubSpaceGetApiKey(TEST_USER)) ? "yes" : "no -- FAIL");
printf("  %-44s %s\n", "and the key names the user",
       sameOk(TEST_USER, hubSpaceUserNameForApiKey(NULL, first)) ? "yes" : "no -- FAIL");

printf("\na second key replaces the first\n");
char *second = cloneString(hubSpaceGenerateApiKey(TEST_USER));
printf("  %-44s %s\n", "the two keys differ",
       sameString(first, second) ? "no -- FAIL" : "yes");
printf("  %-44s %s\n", "the old key now names nobody",
       hubSpaceUserNameForApiKey(NULL, first) == NULL ? "yes" : "no -- FAIL");
printf("  %-44s %s\n", "the new key names the user",
       sameOk(TEST_USER, hubSpaceUserNameForApiKey(NULL, second)) ? "yes" : "no -- FAIL");

printf("\nadopting a key another mirror made\n");
hubSpaceSetApiKey(TEST_USER, PEER_KEY);
printf("  %-44s %s\n", "the stored key is exactly the one given",
       sameOk(PEER_KEY, hubSpaceGetApiKey(TEST_USER)) ? "yes" : "no -- FAIL");
printf("  %-44s %s\n", "the mirror's own second key is gone",
       hubSpaceUserNameForApiKey(NULL, second) == NULL ? "yes" : "no -- FAIL");

printf("\nthe signature a peer checks\n");
printf("  %-44s %s\n", "over the adopted key",
       hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY));
printf("  %-44s %s\n", "over a revoke, which carries no key",
       hubSpaceApiKeySyncSig(TEST_USER, ""));
printf("  %-44s %s\n", "a different key signs differently",
       sameString(hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY),
                  hubSpaceApiKeySyncSig(TEST_USER, "another-key")) ? "no -- FAIL" : "yes");
printf("  %-44s %s\n", "a different user signs differently",
       sameString(hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY),
                  hubSpaceApiKeySyncSig("someoneElse", PEER_KEY)) ? "no -- FAIL" : "yes");

printf("\nrevoking\n");
hubSpaceRevokeApiKey(TEST_USER);
printf("  %-44s %s\n", "the user has no key",
       hubSpaceGetApiKey(TEST_USER) == NULL ? "yes" : "no -- FAIL");
printf("  %-44s %s\n", "the revoked key names nobody",
       hubSpaceUserNameForApiKey(NULL, PEER_KEY) == NULL ? "yes" : "no -- FAIL");

printf("\nrefused\n");
refuseNull("generate with no user name", NULL, NULL, FALSE);
refuseNull("adopt with no user name", NULL, PEER_KEY, TRUE);
refuseNull("adopt with no key", TEST_USER, NULL, TRUE);

cleanUp();
printf("\n");
return 0;
}
