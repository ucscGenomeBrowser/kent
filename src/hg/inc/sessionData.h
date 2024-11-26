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

void sessionDataSaveSession(struct cart *cart, char *encUserName, char *encSessionName,
                            char *dbSuffix);
/* If hg.conf specifies safe places to store files and/or tables that belong to user sessions,
 * then scan cart for trashDir files and/or customTrash tables, store them in safe locations,
 * and update cart to point to the new locations. */

#endif // SESSIONDATA_H
