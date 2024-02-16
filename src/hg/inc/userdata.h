/* userdata.c - code for managing data stored on a per user basis */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef USERDATA_H
#define USERDATA_H

struct userFiles
{
char *userName;
struct fileInfo *file; // list of files for this user
};

char *getUserName();

char *getDataDir(char *userName);
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/apache/userdata/userStore/hash/userName/
 * on the RR, this is /userdata/userStore/hash/userName/ */

char *prefixUserFile(char *userName, char *fname);
/* Allocate a new string that contains the full per-user path to fname, NULL otherwise */

void removeFileForUser(char *fname, char *userName);
/* Remove a file for this user if it exists */

struct userFiles *listFilesForUser(char *userName);
/* Get all the files for a particular user */

long long checkUserQuota(char *userName);
/* Return the amount of space a user is currently using */

char *storeUserFile(char *userName, char *fileName, void *data, size_t dataSize);
/* Give a fileName and a data stream, write the data to:
 * userdata/userStore/hashedUserName/userName/fileName
 * where hashedUserName is based on the md5sum of the userName
 * to prevent proliferation of too many directories. 
 *
 * After sucessfully saving the file, return a web accessible url
 * to the file. */

#endif /* USERDATA_H */
