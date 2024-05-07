/* hgHubConnect - User interfaces for connecting and managing track hubs */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGHUBCONNECT_H
#define HGHUBCONNECT_H

#include "cart.h"

//extern struct cart *cart;	/* This holds cgi and other variables between clicks. */

// the variables for various track hub wizard methods:
#define hgHubDeleteFile "deleteFile"
#define hgHubCreateHub "createHub"
#define hgHubEditHub "editHub"

void doRemoveFile();
/* Process the request to remove a file */

void doCreateHub();
/* Make a new hub.txt with the parameters from the JSON request */

void doEditHub();
/* Edit the hub.txt for a hub */

void doTrackHubWizard();
/* Print out the html to allow a user to upload some files from their machine to us */

#endif /* HGHUBCONNECT_H */
