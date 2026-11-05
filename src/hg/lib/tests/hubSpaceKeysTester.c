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

/* A fixed moment, so the signatures this prints do not change from run to run.  Long enough
 * ago that it is also well outside the window a peer will accept. */
#define OLD_TIME 1000000000L

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

static void checkSig(char *what, char *userName, char *apiKey, char *timeStamp, char *sig,
                     boolean expected)
/* Print whether a peer would accept this (userName, apiKey, time, sig), and whether that is
 * what we wanted.  This is the whole gate on writing somebody else's api key into our own
 * central, so both answers matter: a forgery accepted is the hole, a real sync refused is
 * the feature not working. */
{
boolean got = hubSpaceApiKeySyncSigOk(userName, apiKey, timeStamp, sig);
printf("  %-44s %s\n", what,
       got == expected ? (got ? "accepted" : "refused")
                       : (got ? "accepted -- FAIL" : "refused -- FAIL"));
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
// A fixed timestamp, so the printed signatures are the same on every run.  It is decades in
// the past, which is also what makes the staleness test below mean something.
printf("  %-44s %s\n", "over the adopted key",
       hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, OLD_TIME));
printf("  %-44s %s\n", "over a revoke, which carries no key",
       hubSpaceApiKeySyncSig(TEST_USER, "", OLD_TIME));
printf("  %-44s %s\n", "a different key signs differently",
       sameString(hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, OLD_TIME),
                  hubSpaceApiKeySyncSig(TEST_USER, "another-key", OLD_TIME)) ? "no -- FAIL" : "yes");
printf("  %-44s %s\n", "a different user signs differently",
       sameString(hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, OLD_TIME),
                  hubSpaceApiKeySyncSig("someoneElse", PEER_KEY, OLD_TIME)) ? "no -- FAIL" : "yes");
printf("  %-44s %s\n", "a different second signs differently",
       sameString(hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, OLD_TIME),
                  hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, OLD_TIME + 1)) ? "no -- FAIL" : "yes");

printf("\nwhat a peer accepts\n");
// Signed now, which is the only thing a peer should take
long now = (long)time(NULL);
char nowString[32];
safef(nowString, sizeof nowString, "%ld", now);
checkSig("a signature made now", TEST_USER, PEER_KEY, nowString,
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now), TRUE);
checkSig("a revoke signed now", TEST_USER, "", nowString,
         hubSpaceApiKeySyncSig(TEST_USER, "", now), TRUE);
// Everything a forgery or a replay would look like
checkSig("a made-up signature", TEST_USER, PEER_KEY, nowString,
         "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", FALSE);
checkSig("a signature for another user", TEST_USER, PEER_KEY, nowString,
         hubSpaceApiKeySyncSig("someoneElse", PEER_KEY, now), FALSE);
checkSig("a signature for another key", TEST_USER, PEER_KEY, nowString,
         hubSpaceApiKeySyncSig(TEST_USER, "another-key", now), FALSE);
checkSig("the key swapped after signing", TEST_USER, "another-key", nowString,
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now), FALSE);
checkSig("a replay from long ago", TEST_USER, PEER_KEY, "1000000000",
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, 1000000000L), FALSE);
// Just outside the window either way, and just inside it, so the edge is pinned down
char skewString[32];
safef(skewString, sizeof skewString, "%ld", now - HUB_APIKEY_SYNC_WINDOW - 5);
checkSig("signed just before the window", TEST_USER, PEER_KEY, skewString,
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now - HUB_APIKEY_SYNC_WINDOW - 5), FALSE);
safef(skewString, sizeof skewString, "%ld", now + HUB_APIKEY_SYNC_WINDOW + 5);
checkSig("signed too far in the future", TEST_USER, PEER_KEY, skewString,
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now + HUB_APIKEY_SYNC_WINDOW + 5), FALSE);
// A peer whose clock is a little ahead of ours still has to be able to talk to us
safef(skewString, sizeof skewString, "%ld", now + 30);
checkSig("a peer clock 30s ahead", TEST_USER, PEER_KEY, skewString,
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now + 30), TRUE);
checkSig("no timestamp at all", TEST_USER, PEER_KEY, "",
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now), FALSE);
checkSig("a timestamp that is not a number", TEST_USER, PEER_KEY, "yesterday",
         hubSpaceApiKeySyncSig(TEST_USER, PEER_KEY, now), FALSE);
checkSig("no signature at all", TEST_USER, PEER_KEY, nowString, "", FALSE);
// A newline would let one signature cover two different (userName, apiKey) splits
checkSig("a newline in the userName", "a\nb", PEER_KEY, nowString,
         "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", FALSE);

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
