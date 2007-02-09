/* trashDir.c - temporary file creation and directory creation in /trash */

#ifndef TRASHDIR_H
#define TRASHDIR_H

void trashDirFile(struct tempName *tn, char *dirName, char *base, char *suffix);
/*	obtain a trash file name trash/dirName/base*.suffix */

#endif	/*	TRASHDIR_H	*/
