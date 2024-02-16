/* hgHubConnect - User interfaces for connecting and managing track hubs */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGHUBCONNECT_H
#define HGHUBCONNECT_H

#include "cart.h"

//extern struct cart *cart;	/* This holds cgi and other variables between clicks. */

// the form variable for the track hub wizard
#define hgHubDeleteFile "deleteFile"
void doRemoveFile();
/* Process the request to remove a file */

void doTrackHubWizard();
/* Print out the html to allow a user to upload some files from their machine to us */

void doCreateHub();
/* User has told us about some local files, turn them into a hub with sensible defaults */

#endif /* HGHUBCONNECT_H */
