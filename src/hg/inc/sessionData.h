/* sessionData - functions for moving user data out of trash into permanent storage
 *
 * Copyright (C) 2019-2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef SESSIONDATA_H
#define SESSIONDATA_H

char *sessionDataSaveTrashFile(char *trashPath, char *sessionDir);
/* If trashPath exists and is not already a soft-link to sessionDir, alloc and return a new path in
 * sessionDir; move trashPath to new path and soft-link from trashPath to new path.
 * If trashPath is already a soft-link, return the path that it links to.
 * Return NULL if trashPath does not exist (can happen with expired custom track files). */

/* Number of hex characters of md5(encSessionName) used to name a session's data directory.
 * This was 8 (32 bits) until 2026.  That was safe while a directory only had to be unique among
 * one user's sessions, but anonymous share links all live under the single reserved user name
 * "l", which turns it into a birthday problem across every anonymous session: two of them share
 * a directory more likely than not at ~77,000 sessions, and cleaning one up would take the
 * other's files with it.  10 hex characters is 40 bits, which pushes that past 1,000,000.
 * Directories written before the change are named with sessionDirHashLenLegacy characters, so
 * code that deletes a session's directory must try both lengths. */
#define sessionDirHashLen 10
#define sessionDirHashLenLegacy 8

char *sessionDirFromNames(char *sessionDataDir, char *encUserName, char *encSessionName);
/* Alloc and return the per-session data directory under sessionDataDir (hashed by user and session
 * name), or NULL if sessionDataDir is empty.  errAborts if sessionDataDir is not an absolute path. */

char *sessionDirFromNamesHashLen(char *sessionDataDir, char *encUserName, char *encSessionName,
                                 int hashLen);
/* Like sessionDirFromNames but with the number of session-hash characters spelled out, so that
 * cleanup code can also name a directory written before sessionDirHashLen was widened. */

void sessionDataSaveSession(struct cart *cart, char *encUserName, char *encSessionName,
                            char *dbSuffix);
/* If hg.conf specifies safe places to store files and/or tables that belong to user sessions,
 * then scan cart for trashDir files and/or customTrash tables, store them in safe locations,
 * and update cart to point to the new locations. */

#endif // SESSIONDATA_H
