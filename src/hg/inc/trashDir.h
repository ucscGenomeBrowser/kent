/* trashDir.c - temporary file creation and directory creation in /trash */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

#endif	/*	TRASHDIR_H	*/
