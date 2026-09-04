/* snapshotSession - lightweight, shareable "view snapshot" sessions.  See snapshotSession.h. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "cart.h"
#include "hgConfig.h"
#include "portable.h"
#include "md5.h"
#include "htmshell.h"
#include "sessionData.h"
#include "snapshotSession.h"

/* Bits of randomness in a server-generated snapshot token.  128 bits -> ~24 URL-safe chars, so
 * collisions are astronomically unlikely; snapshotNewName() also checks the DB and retries, so the
 * name is guaranteed unique regardless. */
#define snapshotTokenBits 128

/* ---- Registry of snapshot types ------------------------------------------------------------- */

/* BLAT: a single alignment (or the hgBlat results table) rebuilds from just the pinned bigPsl file;
 * the query sequence lives inside the bigPsl, so no .fa/.pslx is needed.  "db" is added implicitly. */
static char *blatVars[] = { "blatLastBigBed", NULL };

static struct snapshotType snapshotTypes[] =
    {
    { "blat", blatVars },
    };

struct snapshotType *snapshotTypeFind(char *name)
/* Return the registered snapshot type, or NULL if name is not a known type. */
{
if (isEmpty(name))
    return NULL;
int i;
for (i = 0;  i < ArraySize(snapshotTypes);  i++)
    if (sameString(name, snapshotTypes[i].name))
        return &snapshotTypes[i];
return NULL;
}

boolean snapshotIsSnapshotName(char *sessionName)
/* Return TRUE if sessionName is a snapshot name (starts with the "__" prefix). */
{
return sessionName != NULL && startsWith(snapshotNamePrefix, sessionName);
}

static char *snapshotSessionDir(char *sessionDataDir, char *encUserName, char *encSessionName)
/* Alloc and return the durable data directory for one snapshot, or NULL if sessionDataDir is empty.
 * Like sessionData's sessionDirFromNames but with two extra hash levels drawn from the session name,
 * so a single high-volume user - the anonymous "l", which owns most snapshots - never accumulates
 * millions of entries in one directory.  Layout:
 *   sessionDataDir / <2hex md5(user)> / <encUser> / <2hex sess> / <2hex sess> / <8hex md5(session)>
 * Snapshots use their own layout (not sessionDirFromNames), so this never affects normal sessions. */
{
if (isEmpty(sessionDataDir))
    return NULL;
if (sessionDataDir[0] != '/')
    errAbort("config setting sessionDataDir must be an absolute path (starting with '/')");
char *userHash = md5HexForString(encUserName);
char *sessHash = md5HexForString(encSessionName);
char fan1[3], fan2[3];
safencpy(fan1, sizeof fan1, sessHash, 2);           /* first 2 hex of the session hash  */
safencpy(fan2, sizeof fan2, sessHash + 2, 2);       /* next 2 hex -> 65536 buckets total */
userHash[2] = '\0';
sessHash[8] = '\0';
struct dyString *dy = dyStringCreate("%s/%s/%s/%s/%s/%s",
                                     sessionDataDir, userHash, encUserName, fan1, fan2, sessHash);
freeMem(userHash);
freeMem(sessHash);
return dyStringCannibalize(&dy);
}

char *snapshotNewName(struct sqlConnection *conn, char *encUserName)
/* See snapshotSession.h. */
{
char query[512];
int tries;
for (tries = 0;  tries < 100;  tries++)
    {
    char *tok = makeRandomKey(snapshotTokenBits);
    char *name = catTwoStrings(snapshotNamePrefix, tok);
    freeMem(tok);
    sqlSafef(query, sizeof query,
             "select count(*) from %s where userName='%s' and sessionName='%s'",
             namedSessionTable, encUserName, name);
    if (sqlQuickNum(conn, query) == 0)
        return name;
    freez(&name);
    }
errAbort("snapshotNewName: could not find an unused name for user '%s' after %d tries",
         encUserName, tries);
return NULL;
}

/* ---- Saving --------------------------------------------------------------------------------- */

static void appendVar(struct dyString *dy, char *var, char *val)
/* Append "var=cgiEncode(val)" to dy, with a leading '&' if dy is non-empty. */
{
if (dy->stringSize > 0)
    dyStringAppendC(dy, '&');
dyStringAppend(dy, var);
dyStringAppendC(dy, '=');
char *e = cgiEncode(val);
dyStringAppend(dy, e);
freez(&e);
}

static char *snapshotMakeContents(struct snapshotType *type, char *encUserName, char *encSessionName,
                                  struct cart *cart)
/* Build the minimal CGI-encoded contents for a snapshot: "db" plus the type's declared vars.  For
 * each var present in the cart, move any backing trash file into durable sessionData storage (so the
 * snapshot outlives trash cleaning) and store the durable path.  Returns a string to free. */
{
char *sessionDataDir = cfgOption("sessionDataDir");
char *sessionDir = snapshotSessionDir(sessionDataDir, encUserName, encSessionName);
struct dyString *dy = dyStringNew(512);

char *db = cartOptionalString(cart, "db");
if (isNotEmpty(db))
    appendVar(dy, "db", db);

int i;
for (i = 0;  type->vars[i] != NULL;  i++)
    {
    char *var = type->vars[i];
    char *val = cartOptionalString(cart, var);
    if (isEmpty(val))
        continue;
    /* Move the referenced trash file into durable storage when sessionData is configured, and store
     * the durable path.  sessionDataSaveTrashFile returns NULL if the file is gone (expired) - in
     * that case keep the original value so the reconstruct path can report a clean "expired". */
    char *durable = NULL;
    if (isNotEmpty(sessionDir))
        durable = sessionDataSaveTrashFile(val, sessionDir);
    appendVar(dy, var, isNotEmpty(durable) ? durable : val);
    freez(&durable);
    }
freez(&sessionDir);
return dyStringCannibalize(&dy);
}

int saveSnapshotSession(struct sqlConnection *conn, char *snapshotTypeName,
                        char *encUserName, char *encSessionName, struct cart *cart)
/* See snapshotSession.h. */
{
struct snapshotType *type = snapshotTypeFind(snapshotTypeName);
if (type == NULL)
    errAbort("saveSnapshotSession: unknown snapshot type '%s'", snapshotTypeName);
if (!snapshotIsSnapshotName(encSessionName))
    errAbort("saveSnapshotSession: name '%s' must start with the '%s' prefix",
             encSessionName, snapshotNamePrefix);

char *contents = snapshotMakeContents(type, encUserName, encSessionName, cart);
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

/* Preserve firstUse/useCount if we are overwriting an existing snapshot of this name. */
struct dyString *dy = dyStringNew(4096);
char *firstUse = NULL;
int useCount = 0;
sqlDyStringPrintf(dy, "SELECT firstUse, useCount FROM %s WHERE userName='%s' AND sessionName='%s'",
                  namedSessionTable, encUserName, encSessionName);
struct sqlResult *sr = sqlGetResult(conn, dy->string);
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    firstUse = cloneString(row[0]);
    useCount = atoi(row[1]) + 1;
    }
sqlFreeResult(&sr);

dyStringClear(dy);
sqlDyStringPrintf(dy, "DELETE FROM %s WHERE userName='%s' AND sessionName='%s'",
                  namedSessionTable, encUserName, encSessionName);
sqlUpdate(conn, dy->string);

/* settings records the snapshot type, so the row is self-describing for reconstruction/debugging. */
char settings[256];
safef(settings, sizeof settings, "snapshotType %s\n", type->name);

dyStringClear(dy);
sqlDyStringPrintf(dy,
    "INSERT INTO %s (userName, sessionName, contents, shared, firstUse, lastUse, useCount",
    namedSessionTable);
if (gotSettings)
    sqlDyStringPrintf(dy, ", settings");
sqlDyStringPrintf(dy, ") VALUES ('%s', '%s', '", encUserName, encSessionName);
sqlDyAppendEscaped(dy, contents);
sqlDyStringPrintf(dy, "', 1, ");                 /* shared = 1 (shareable by link) */
if (firstUse)
    sqlDyStringPrintf(dy, "'%s', ", firstUse);
else
    sqlDyStringPrintf(dy, "now(), ");
sqlDyStringPrintf(dy, "now(), %d", useCount);
if (gotSettings)
    {
    sqlDyStringPrintf(dy, ", '");
    sqlDyAppendEscaped(dy, settings);
    sqlDyStringPrintf(dy, "'");
    }
sqlDyStringPrintf(dy, ")");
sqlUpdate(conn, dy->string);

freez(&contents);
freez(&firstUse);
dyStringFree(&dy);
return useCount;
}

/* ---- Cleaning up abandoned anonymous snapshots ---------------------------------------------- */

static void removeDirTree(char *dir)
/* Best-effort recursive removal of a directory and its contents (files are hard-links into durable
 * storage; unlinking frees the space).  Missing paths are ignored. */
{
if (isEmpty(dir) || !fileExists(dir))
    return;
struct fileInfo *fiList = listDirX(dir, "*", TRUE), *fi;
for (fi = fiList;  fi != NULL;  fi = fi->next)
    {
    if (fi->isDir)
        removeDirTree(fi->name);
    else
        remove(fi->name);
    }
slFreeList(&fiList);
rmdir(dir);
}

int snapshotCleanAnon(struct sqlConnection *conn, int ttlDays, boolean dryRun)
/* See snapshotSession.h. */
{
char *sessionDataDir = cfgOption("sessionDataDir");
char query[1024];
/* '\_\_%' : the two leading underscores are literal (escaped, since '_' is a LIKE wildcard),
 * followed by the '%' wildcard for the random token. */
sqlSafef(query, sizeof query,
    "SELECT sessionName FROM %s WHERE userName='%s' AND sessionName LIKE '\\_\\_%%' "
    "AND lastUse < DATE_SUB(now(), INTERVAL %d DAY)",
    namedSessionTable, snapshotAnonUser, ttlDays);
struct slName *toClean = NULL;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slNameAddHead(&toClean, row[0]);
sqlFreeResult(&sr);

int n = 0;
struct slName *s;
for (s = toClean;  s != NULL;  s = s->next)
    {
    if (!dryRun)
        {
        /* Remove the durable files first, then the row, so a crash never orphans the DB pointer.
         * Minimal snapshots live under the fanned-out snapshotSessionDir; a full anonymous share
         * (e.g. the top-right "Share a link" when logged out) uses sessionData's flat layout - remove
         * whichever exists (removeDirTree ignores a missing path). */
        char *snapDir = snapshotSessionDir(sessionDataDir, snapshotAnonUser, s->name);
        char *flatDir = sessionDirFromNames(sessionDataDir, snapshotAnonUser, s->name);
        removeDirTree(snapDir);
        removeDirTree(flatDir);
        freez(&snapDir);
        freez(&flatDir);
        sqlSafef(query, sizeof query,
                 "DELETE FROM %s WHERE userName='%s' AND sessionName='%s'",
                 namedSessionTable, snapshotAnonUser, s->name);
        sqlUpdate(conn, query);
        }
    n++;
    }
slFreeList(&toClean);
return n;
}
