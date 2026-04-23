/* userdata.c - code for managing data stored on a per user basis */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef USERDATA_H
#define USERDATA_H

#include "hubSpace.h"

struct userFiles
{
char *userName;
struct fileInfo *fileList; // list of files for this user
};

struct userHubs
{
struct userHubs *next;
char *hubName; // name of this hub
char *genome; // only one genome allowed per hub for now
char *userName; // for convenience
time_t lastModified; // actually last access time but this works
struct userFiles *fileList; // list of files (tracks) in the hub
};

char *getUserName();
/* Query the right system for the users name */

char *emailForUserName(char *userName);
/* Fetch the email for this user from gbMembers hgcentral table */

// the various quota helper variables:
// 10 GiB in bytes, so the binary-unit pretty-printers display it as "10 GB".
#define HUB_SPACE_DEFAULT_QUOTA_BYTES (10LL * 1024 * 1024 * 1024)
#define HUB_SPACE_DEFAULT_QUOTA HUB_SPACE_DEFAULT_QUOTA_BYTES
// for defining the quota in hg.conf
#define HUB_SPACE_CONF_QUOTA_VAR "hubspace.quota"

char *webDataDir(char *userName);
/* Return a web accesible path to the userDataDir, this is different from the full path tusd uses */

char *urlForFile(char *userName, char *filePath);
/* Return a web accessible URL to filePath */

char *getEncodedUserNamePath(char *userName);
/* Compute the path for just the userName part of the users upload */

char *getDataDir(char *userName);
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/apache/userdata/userStore/hash/userName/
 * on the RR, this is /userdata/userStore/hash/userName/ */

char *stripDataDir(char *fname, char *userName);
/* Strips the getDataDir(userName) off of fname */

char *swapDataDir(char *userName, char *in);
/* Try replacing the current dataDir with what is defined in hg.conf:tusdMountPoint as
 * the data server may be somewhere else and mounted over NFS. In this case, when
 * tusd saves files, it is writing it's local tusdDataDir value into the hgcentral
 * file location. When the CGI running somewhere else needs to verify file existence,
 * the tusdDataDir won't exist on the CGI filesystem, but will instead be mounted as some
 * different path.  In this case, replace tusdDataDir with tusdMountPoint */

char *prefixUserFile(char *userName, char *fname, char *parentDir);
/* Allocate a new string that contains the full per-user path to fname. return NULL if
 * we cannot construct a full path because of a realpath(3) failure.
 * parentDir is optional and will go in between the per-user dir and the fname */

char *hubNameFromPath(char *path);
/* Return the last directory component of path. Assume that a '.' char in the last component
 * means that component is a filename and go back further */

char *writeHubText(char *path, char *userName, char *db, char *twoBitFileName);
/* Create a hub.txt file, optionally creating the directory holding it.
 * If twoBitFileName is non-NULL, write an assembly hub stanza referencing it
 * (with stub organism / scientificName / description / defaultPos derived from
 * the 2bit). For convenience, return the file name of the created hub, which
 * can be freed. */

void createNewTempHubForUpload(char *requestId, struct hubSpace *rowForFile, char *userDataDir, char *parentDir);
/* Creates a hub.txt for this upload, and updates the hubSpace table for the
 * hub.txt and any parentDirs we need to create. */

boolean userHasOwnNamedHubTxtInDir(char *userName, char *parentDir);
/* Return TRUE if user uploaded a *.hub.txt NOT literally named 'hub.txt' in parentDir.
 * Used to decide whether the backend can modify hub.txt (synthesize / append / upgrade)
 * or should leave it alone because the user has their own authoritative config. */

char *existingHubTypeForDir(char *userName, char *hubName);
/* Return the hubType of this user's hub dir row, or NULL if no such row exists. */

void upgradeExistingHubToAssembly(struct hubSpace *rowForFile, char *userDataDir, char *encodedParentDir);
/* Race-proofing: when a 2bit arrives into a hub that already has a synthesized
 * hub.txt, upgrade that hub.txt to include the assembly stanza and mark every
 * hubSpace row for this hub as hubType='assemblyHub'. No-op unless rowForFile
 * is a 2bit, or the synthesized hub.txt does not exist. */

boolean literalHubTxtExistsOnDisk(char *parentDir, char *userDataDir);
/* Return TRUE if path/hub.txt exists as a real file in this user's parentDir. */

int lockHubDir(char *hubDir);
/* Acquire an exclusive flock on hubDir/.hub.lock; returns a file descriptor.
 * Hold while mutating hub.txt to serialize parallel pre-finish processes. */

void unlockHubDir(int fd);
/* Release an exclusive hub lock acquired by lockHubDir. */

void addHubSpaceRowForFile(struct hubSpace *row);
/* We created a file for a user, now add an entry to the hubSpace table for it */

void makeParentDirRows(char *userName, time_t lastModified, char *db, char *parentDirStr, char *userDataDir, char *hubType);
/* For each '/' separated component of parentDirStr, create a row in hubSpace. Return the
 * final subdirectory component of parentDirStr */

void removeFileForUser(char *fname, char *userName);
/* Remove a file for this user if it exists */

struct hubSpace *listFilesForUser(char *userName);
/* Return the files the user has uploaded */

char *defaultHubNameForUser(char *userName);
/* Return a name to use as a default for a hub, starts with myFirstHub, then myFirstHub2, ... */

long long getMaxUserQuota(char *userName);
/* Return how much space is allocated for this user or the default */

long long checkUserQuota(char *userName);
/* Return the amount of space a user is currently using */

#endif /* USERDATA_H */
