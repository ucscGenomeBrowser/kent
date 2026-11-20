/* activationTokenTester - which activation links still activate an account.
 *
 * The activation mail says its link expires in seven days, and setupNewAccount sets
 * emailTokenExpires to match, but until #38302 nothing read that column: an old link worked
 * forever.  An empty token also matched an empty token= on the link, which is what an account
 * holds once it has been activated.  gbMembersActivationTokenOk now needs a token that is
 * not empty, matches, and has not expired.  Only an account with no password (made by a social
 * login, where opening the link signs the user in) has a token that expires; for an account with
 * a password the link only confirms the address, so it keeps working.
 *
 * The test writes its own gbMembers rows to hgcentralregress and removes them at both ends.
 * See hg/lib/tests/makeHgCentralRegress.sh.
 *
 * refs #38302 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgConfig.h"
#include "gbMembers.h"

/* Recognisable in a stray row, and not a name anyone would log in with. */
#define USER_PREFIX "regress38302"
#define TOKEN "0123456789abcdef0123456789abcdef"

static void cleanUp()
/* Take this test's rows out again, at both ends of the run. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof query, "delete from gbMembers where userName like '" USER_PREFIX "%%'");
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static void addMember(char *userName, char *password, char *token, int expiresInDays)
/* Add an unactivated member whose emailToken expires this many days from now.  An empty
 * password is an account made by a social login. */
{
struct sqlConnection *conn = hConnectCentral();
char query[1024];
sqlSafef(query, sizeof query,
    "insert into gbMembers (userName, realName, password, email, lastUse, newPassword, "
    "newPasswordExpire, dateActivated, emailToken, emailTokenExpires) values "
    "('%s', '', '%s', 'regressTest@example.org', NOW(), '', NOW(), NOW(), '%s', "
    "DATE_ADD(NOW(), INTERVAL %d DAY))", userName, password, token, expiresInDays);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static void showCase(struct sqlConnection *conn, char *what, char *userName, char *token)
/* Print whether this link would activate the account. */
{
printf("  %-52s %s\n", what,
       gbMembersActivationTokenOk(conn, userName, token) ? "activates" : "refused");
}

int main(int argc, char *argv[])
{
if (!sameString(cfgOption("central.db"), "hgcentralregress"))
    errAbort("this test writes rows, so it must be pointed at hgcentralregress, not %s",
             cfgOption("central.db"));
cleanUp();
addMember(USER_PREFIX "fresh", "", TOKEN, 7);
addMember(USER_PREFIX "stale", "", TOKEN, -1);
addMember(USER_PREFIX "used", "", "", 7);
addMember(USER_PREFIX "pwdStale", "someHash", TOKEN, -1);

struct sqlConnection *conn = hConnectCentral();
printf("a token mailed today\n");
showCase(conn, "the right token", USER_PREFIX "fresh", TOKEN);
showCase(conn, "a wrong token", USER_PREFIX "fresh", "fedcba9876543210fedcba9876543210");
showCase(conn, "an empty token", USER_PREFIX "fresh", "");
printf("a token past its seven days, account without a password\n");
showCase(conn, "the right token", USER_PREFIX "stale", TOKEN);
printf("a token past its seven days, account with a password\n");
showCase(conn, "the right token", USER_PREFIX "pwdStale", TOKEN);
showCase(conn, "a wrong token", USER_PREFIX "pwdStale", "fedcba9876543210fedcba9876543210");
printf("an account already activated, so its token is empty\n");
showCase(conn, "an empty token", USER_PREFIX "used", "");
printf("no such account\n");
showCase(conn, "an empty token", USER_PREFIX "nobody", "");
hDisconnectCentral(&conn);

cleanUp();
return 0;
}
