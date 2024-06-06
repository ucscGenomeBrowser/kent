/* hgHubConnect - User interfaces for connecting and managing track hubs */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGHUBCONNECT_H
#define HGHUBCONNECT_H

#include "cart.h"
#include "cartJson.h"

//extern struct cart *cart;	/* This holds cgi and other variables between clicks. */

// the variables for various track hub wizard methods:
#define hgHubDeleteFile "deleteFile"
#define hgHubCreateHub "createHub"
#define hgHubEditHub "editHub"
#define hgHubMoveFile "moveFile"

void doRemoveFile(struct cartJson *cj, struct hash *paramHash);
/* Process the request to remove a file */

void doMoveFile(struct cartJson *cj, struct hash *paramHash);
/* Move a file to a new hub */

void doCreateHub(struct cartJson *cj, struct hash *paramHash);
/* Make a new hub.txt with the parameters from the JSON request */

void doEditHub(struct cartJson *cj, struct hash *paramHash);
/* Edit the hub.txt for a hub */

void doTrackHubWizard();
/* Print out the html to allow a user to upload some files from their machine to us */

#endif /* HGHUBCONNECT_H */
