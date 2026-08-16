/* trashDir.c - temporary file creation and directory creation in /trash */

/* Copyright (C) 2010 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef TRASHDIR_H
#define TRASHDIR_H

#include "portable.h"

void trashDirFile(struct tempName *tn, char *dirName, char *base, char *suffix);
/*	obtain a trash file name trash/dirName/base*.suffix */

void trashDirDateFile(struct tempName *tn, char *dirName, char *base, char *suffix);
/*	obtain a trash file name trash/dirName.dayOfYear/base*.suffix */

boolean trashDirReusableFile(struct tempName *tn, char *dirName, char *base, char *suffix);
/*      obtain a resusable trash file name as trash/dirName/base.suffix
 *      returns TRUE if already exists. */

void copyFileToTrash(char **pFileName, char *dirName, char *base, char *suffix);
/* If *pFileName is not NULL and exists, then create a new file in the
 * given dirName of trash/ with the given base and suffix, copy *pFileName's
 * contents to it, and set *pFileName to the new filename. */

boolean isTrashPath(char *path);
/* Return TRUE if path names a file inside the trash directory. */

boolean isTrashOrSessionDataPath(char *path);
/* Return TRUE if path is under the trash directory, or under one of the durable session-data
 * directories that trash files are moved to when a session is saved.  This is the allow-list
 * isValidBigDataUrl() uses for a bigDataUrl that names a local file. */

boolean isServerUserFilePath(char *path);
/* Return TRUE if path is under one of the directories where the server keeps files it made
 * for a user: the trash directory, the session-data directories, or a per-feature data
 * directory such as myVariantsDataDir.
 *
 * Use this on any file name that comes back out of the cart before opening, reading, writing
 * or deleting it.  Cart values are not ours: they arrive from CGI parameters, from an uploaded
 * or fetched hgSession file, and from another user's shared session.  Note that a trash file
 * may itself be a symbolic link pointing at session storage, so a realpath() check is not
 * usable here -- sessionDataSaveTrashFile() creates exactly those links on purpose. */

boolean isRemoteUrl(char *path);
/* Return TRUE if path is a URL fetched over the network rather than a file name. */

boolean isServerUserFileOrUrl(char *path);
/* Return TRUE if path is either a remote URL or a file the server made for a user.
 *
 * Use this instead of isServerUserFilePath() on the cart variables that legitimately hold
 * either one, such as multiRegionsBedUrl and the hgSession load-from-URL name.  Both are read
 * by code that decides between the two by looking for a protocol and falls through to opening
 * a local file, so a value with no protocol has to be one of ours. */

#endif	/*	TRASHDIR_H	*/
