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

//TODO: this should probably come from hg.conf:
#define HUB_SPACE_URL "https://hgwdev.gi.ucsc.edu/hubspace"

// the various quota helper variables:
#define HUB_SPACE_DEFAULT_QUOTA_BYTES 1000000000
#define HUB_SPACE_DEFAULT_QUOTA HUB_SPACE_DEFAULT_QUOTA_BYTES 
// for defining the quota in hg.conf
#define HUB_SPACE_CONF_QUOTA_VAR "hubspace.quota"
// path to hg.conf file that has a list of hubspace.quota.userName entries
// for special per user quotas:
#define HUB_SPACE_QUOTA_LIST_VAR "hubspace.quotaList"

char *webDataDir(char *userName);
/* Return a web accesible path to the userDataDir, this is different from the full path tusd uses */

char *getDataDir(char *userName);
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/apache/userdata/userStore/hash/userName/
 * on the RR, this is /userdata/userStore/hash/userName/ */

char *prefixUserFile(char *userName, char *fname);
/* Allocate a new string that contains the full per-user path to fname, NULL otherwise */

char *writeHubText(char *path, char *userName, char *hubName, char *db);
/* Create a hub.txt file, optionally creating the directory holding it. For convenience, return
 * the file name of the created hub, which can be freed. */

void createNewTempHubForUpload(char *requestId, char *userName, char *db, char *trackFileName, char *trackType, char *parentDir);
/* Creates a hub.txt for this upload with a random hub name. Returns the full path to the hub
 * for convenience. */

void addHubSpaceRowForFile(struct hubSpace *row);
/* We created a file for a user, now add an entry to the hubSpace table for it */

void removeFileForUser(char *fname, char *userName);
/* Remove a file for this user if it exists */

void removeHubForUser(char *path, char *userName);
/* Remove a hub directory for this user (and all files in the directory), if it exists */

struct userHubs *listHubsForUser(char *userName);
/* Lists the directories for a particular user */

time_t getHubLatestTime(struct userHubs *hub);
/* Return the latest access time of the files in a hub */

struct userFiles *listFilesForUserHub(char *userName, char *hubName);
/* Get all the files for a particular hub for a particular user */

struct hubSpace *listFilesForUser(char *userName);
/* Return the files the user has uploaded */

char *defaultHubNameForUser(char *userName);
/* Return a name to use as a default for a hub, starts with myFirstHub, then myFirstHub2, ... */

long long getMaxUserQuota(char *userName);
/* Return how much space is allocated for this user or the default */

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
