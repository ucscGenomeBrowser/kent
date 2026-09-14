/* snapshotSession - lightweight, shareable "view snapshot" sessions.
 *
 * A snapshot is a minimal named session in the central namedSessionDb.  Unlike a normal saved
 * session (which stores the whole cart), a snapshot stores ONLY the handful of cart variables a
 * feature needs to reconstruct one specific view - e.g. a single BLAT alignment - and moves only
 * those variables' backing trash files into durable sessionData storage.  This keeps the row tiny
 * and, crucially, avoids leaking the sharer's unrelated tracks/position to whoever opens the link.
 *
 * Snapshots are always shared-by-link.  Their session names are prefixed "__" so the snapshot
 * cleaner can recognize abandoned anonymous ones; what marks a row as a snapshot for anything else
 * (notably the My Sessions listings, which hide them) is the "snapshotType" line in its settings
 * column, because the name prefix alone is not ours - users have named sessions of their own that
 * way (refs #38313).
 *
 * Each feature that wants durable shareable links registers a snapshotType naming its variables;
 * the feature reconstructs its view from those variables, and the existing session-load path bumps
 * lastUse on every open so popular links stay alive under the "durable while used" policy.
 *
 * Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef SNAPSHOTSESSION_H
#define SNAPSHOTSESSION_H

#include "cart.h"
#include "jksql.h"

/* All snapshot session names start with this marker, which makes them eligible for TTL cleaning.
 * A single leading '_' is reserved for real, user-visible auto-named quick shares; the double '__'
 * means "machine-made, not a normal loadable session".  Note that this is a naming convention we
 * follow, not a namespace we own: users can and do name their own sessions "__something", so never
 * use the prefix alone to decide that a row is not the user's (refs #38313) - use
 * snapshotIsSnapshotSettings() for that. */
#define snapshotNamePrefix "__"

/* Key of the settings line saveSnapshotSession() writes into namedSessionDb.settings, e.g.
 * "snapshotType blat".  Its presence is the authoritative mark of a snapshot row; its value is the
 * struct snapshotType name, so the row says which feature's view it reconstructs. */
#define snapshotTypeSetting "snapshotType"

/* Reserved userName for logged-out (anonymous) snapshots, matching doSaveSessionJson's convention
 * and the /s/l/<name> short link. */
#define snapshotAnonUser "l"

/* Default cleaner TTL: an anonymous snapshot not opened within this many days is deleted.
 * 4 years ~ the length of a typical PhD, so a link in a thesis keeps working for its author's degree.
 * Override with the hg.conf setting "snapshot.ttlDays". */
#define snapshotDefaultTtlDays (4 * 365)

struct snapshotType
/* A registered kind of shareable view snapshot: the cart variables a given feature needs to
 * reconstruct one of its views.  Register one per feature and keep the list minimal. */
    {
    char *name;         /* type key sent by the client, e.g. "blat" */
    char **vars;        /* NULL-terminated cart variable names to persist (besides "db") */
    char *requiredVar;  /* if non-NULL, the snapshot is a dead link without this cart var, so the
                         * save is rejected when it is absent (e.g. results not built yet) */
    };

struct snapshotType *snapshotTypeFind(char *name);
/* Return the registered snapshot type, or NULL if name is not a known type. */

boolean snapshotHasRequired(struct snapshotType *type, struct cart *cart);
/* Return FALSE when type declares a requiredVar that is missing/empty in cart (saving it would make
 * a link that reopens to nothing), otherwise TRUE. */

boolean snapshotIsSnapshotName(char *sessionName);
/* Return TRUE if sessionName is a snapshot name (starts with the "__" prefix).  Only the snapshot
 * writer/cleaner should care: a user's own session can carry the same prefix, so this must not be
 * used to decide whether to show a row to its owner - see snapshotIsSnapshotSettings(). */

char *snapshotTypeFromSettings(char *settings);
/* Return the snapshot type recorded in a namedSessionDb settings string ("snapshotType blat" ->
 * "blat"), or NULL when there is none, i.e. the row is an ordinary saved session.  The value is not
 * checked against the registry, so a type written by a newer build still reads back.  Returns a
 * string to free. */

boolean snapshotIsSnapshotSettings(char *settings);
/* Return TRUE if settings marks this row as a snapshot (a share token), rather than a session the
 * user saved.  This is the test to use when deciding whether to list a row in My Sessions. */

char *snapshotNewName(struct sqlConnection *conn, char *encUserName);
/* Alloc and return a fresh "__"-prefixed snapshot name, server-generated and checked against
 * namedSessionDb so it is guaranteed unique for encUserName (share tokens must never collide and
 * overwrite each other).  The token is long (128 bits) and URL-safe, so it needs no CGI-encoding. */

int saveSnapshotSession(struct sqlConnection *conn, char *snapshotTypeName,
                        char *encUserName, char *encSessionName, struct cart *cart);
/* Save a minimal shared-by-link session named encSessionName (which must already start with "__")
 * under encUserName, holding only the variables declared by snapshotTypeName (plus "db"), and moving
 * just those variables' trash files into durable sessionData storage when it is configured.
 * Overwrites any existing row of that name, preserving its firstUse/useCount.  errAborts on an
 * unknown type or a name lacking the "__" prefix.  Returns the (post-increment) useCount. */

/* Note: no explicit "touch lastUse" is needed - cartLoadUserSession() already bumps lastUse (via
 * sessionTouchLastUse) on every session open, so a link stays alive as long as it is used. */

int snapshotCleanAnon(struct sqlConnection *conn, int ttlDays, boolean dryRun);
/* Delete anonymous ("l") snapshot rows whose lastUse is older than ttlDays, and remove their durable
 * sessionData directories.  Never touches non-anonymous or non-snapshot rows.  Returns the count
 * cleaned (or that would be cleaned, when dryRun). */

#endif /* SNAPSHOTSESSION_H */
