/* hgSession - Manage information associated with a user identity. */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGSESSION_H
#define HGSESSION_H

/* NOTE: some of the original contents have been moved up to cart.h for 
 * sharing with hgTracks. */

#include "cart.h"

/* Global variables - generally set during initialization and then read-only. */
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern char *database;		/* Current database, often but not always dbDatabase. */

/* hgSession form inputs */
#define hgsNewSessionName hgSessionPrefix "newSessionName"
#define hgsNewSessionShare hgSessionPrefix "newSessionShare"
#define hgsNewSessionDescription hgSessionPrefix "newSessionDescription"
#define hgsDoNewSession hgSessionPrefix "doNewSession"

#define hgsSharePrefix hgSessionPrefix "share_"
#define hgsGalleryPrefix hgSessionPrefix "gallery_"
#define hgsEditPrefix hgSessionPrefix "edit_"
#define hgsLoadPrefix hgSessionPrefix "load_"
#define hgsDeletePrefix hgSessionPrefix "delete_"

#define hgsShowDownloadPrefix hgSessionPrefix "showDownload_"
#define hgsMakeDownloadPrefix hgSessionPrefix "makeDownload_"
#define hgsDoDownloadPrefix hgSessionPrefix "doDownload_"

#define hgsSaveLocalBackupFileName hgSessionPrefix "saveLocalBackupFileName"
#define hgsSaveLocalFileName hgSessionPrefix "saveLocalFileName"
#define hgsSaveLocalFileCompress hgSessionPrefix "saveLocalFileCompress"
#define hgsDoSaveLocal hgSessionPrefix "doSaveLocal"

#define hgsLoadLocalFileName hgSessionPrefix "loadLocalFileName"
#define hgsDoLoadLocal hgSessionPrefix "doLoadLocal"

#define hgsLoadUrlName hgSessionPrefix "loadUrlName"
#define hgsDoLoadUrl hgSessionPrefix "doLoadUrl"

#define hgsDoMainPage hgSessionPrefix "doMainPage"

#define hgsDoSessionDetail hgSessionPrefix "doSessionDetail"
#define hgsOldSessionName hgSessionPrefix "oldSessionName"
#define hgsDoSessionChange hgSessionPrefix "doSessionChange"

#define hgsCancel hgSessionPrefix "cancel"

#define hgsDo hgSessionPrefix "do"

// Non-UI reachable function to load a session and save it, for the purpose of moving files
// and tables out of trash/customTrash into userdata/customData after sessionData* params are
// added to hg.conf.
#define hgsDoReSaveSession hgSessionPrefix "doReSaveSession"

// Back-door CGI param to randomize the suffix (usually day of month) for sessionDataDbPrefix.
// This is for bulk re-saving old sessions to move files and tables to safe storage;
// we don't want all of the old sessions' tables to end up in the same day-of-month database.
#define hgsSessionDataDbSuffix hgSessionPrefix "sessionDataDbSuffix"

char *cgiDecodeClone(char *encStr);
/* Allocate and return a CGI-decoded copy of encStr. */

void startBackgroundWork(char *exec, char **pWorkUrl);
/* deal with forking off child for background work
 * and setting up the trash file for communicating
 * from the child to the browser */

void getBackgroundStatus(char *url);
/* fetch status as the latest complete html block available.
 * fetch progress info instead if background proc still running. */

// -----  htmlOpen 

void htmlOpen(char *format, ...);
/* Start up a page that will be in html format. */

void htmlClose();
/* Close down html format page. */

void showDownloadSessionCtData(struct hashEl *downloadList);
/* Show download page for the given session */

void makeDownloadSessionCtData(char *param1, char *backgroundProgress);
/* Download tables and data to save save in compressed archive. */

void doDownloadSessionCtData(struct hashEl *downloadPathList);
/* Download given table to browser to save. */

#endif /* HGSESSION_H */
