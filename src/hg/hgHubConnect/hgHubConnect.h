/* hgHubConnect - User interfaces for connecting and managing track hubs */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGHUBCONNECT_H
#define HGHUBCONNECT_H

#include "cart.h"
#include "cartJson.h"

//extern struct cart *cart;	/* This holds cgi and other variables between clicks. */

// the variables for various track hub wizard methods:
#define hgHubGetHubSpaceUIState "getHubSpaceUIState"
#define hgHubDeleteFile "deleteFile"
#define hgHubCreateHub "createHub"
#define hgHubEditHub "editHub"
#define hgHubMoveFile "moveFile"
#define hgHubGenerateApiKey "generateApiKey"
#define hgHubRevokeApiKey "revokeApiKey"

void cjRevokeApiKey(struct cartJson *cj, struct hash *paramHash);
/* Remove any api keys for the user */

void cjGenerateApiKey(struct cartJson *cj, struct hash *paramHash);
/* Make a random (but not crypto-secure api key for use of hubtools to upload to hubspace */

void doRemoveFile(struct cartJson *cj, struct hash *paramHash);
/* Process the request to remove a file */

void doMoveFile(struct cartJson *cj, struct hash *paramHash);
/* Move a file to a new hub */

void getHubSpaceUIState(struct cartJson *cj, struct hash *paramHash);
/* Get all the data we need to make a users hubSpace UI table. The cartJson library
 * deals with printing the json */

void doEditHub(struct cartJson *cj, struct hash *paramHash);
/* Edit the hub.txt for a hub */

void doTrackHubWizard(char *database);
/* Print out the html to allow a user to upload some files from their machine to us */

#endif /* HGHUBCONNECT_H */
