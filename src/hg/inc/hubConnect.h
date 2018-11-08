/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubStatus table in the hgcentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubStatus 
 * table by design.  We just want field-by-field access to this. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HUBCONNECT_H
#define HUBCONNECT_H

#define defaultHubPublicTableName "hubPublic"
/* Name of our table with list of public hubs. read only */

#define hubPublicTableConfVariable    "hub.publicTableName"
/* the name of the hg.conf variable to use something other than the default */

#define defaultHubStatusTableName "hubStatus"
/* Name of table that maintains status of hubs  read/write. */

#define hubStatusTableConfVariable    "hub.statusTableName"
/* the name of the hg.conf variable to use something other than the default */

#define hgHubCheckUrl      "hubCheckUrl"
/* name of cgi variable containing hub name to check */

#define hgHubDataText      "hubUrl"
/* name of cgi variable containing new hub name */

#define hgHubGenome        "genome"
/* name of the cgi variable to specify a db within an assembly hub */

#define hgHubDataClearText      "hubClear"
/* name of cgi variable containing new hub name */

#define hubTrackPrefix "hub_"
/* The names of all hub tracks begin with this.  Use in cart. */

#define hgHubSearchTerms      "hubSearchTerms"
/* name of cart/cgi variable containing the current search terms */

#define hgHubDbFilter      "hubDbFilter"
/* name of cart/cgi variable containing text for filtering hubs by assembly */

#define hgHub             "hgHub_"  /* prefix for all control variables */
#define hgHubDo            hgHub   "do_"    /* prefix for all commands */
#define hgHubDoClear       hgHubDo "clear"
#define hgHubDoSearch      hgHubDo "search"
#define hgHubDoDeleteSearch      hgHubDo "deleteSearch"
#define hgHubDoFilter      hgHubDo "filter"
#define hgHubDoDisconnect  hgHubDo "disconnect"
#define hgHubDoFirstDb     hgHubDo "firstDb"
#define hgHubDoDecorateDb  hgHubDo "decorateDb"
#define hgHubDoRedirect    hgHubDo "redirect"

boolean isHubTrack(char *trackName);
/* Return TRUE if it's a hub track. */

struct hubConnectStatus
/* Basic status in hubStatus.  Note it is *not* the same as the
 * hubStatus table, that has a bunch of extra fields to help 
 * keep track of whether the hub is alive. */
    {
    struct hubConnectStatus *next;
    unsigned id;	/* Hub ID */
    char *hubUrl;	/* URL to hub.ra file. */
    char *errorMessage;	/* If non-empty hub has an error and this describes it. */
    struct trackHub *trackHub; /* pointer to structure that describes hub */
    unsigned  status;   /* 1 if private */
    };

void hubConnectStatusFree(struct hubConnectStatus **pHub);
/* Free hubConnectStatus */

void hubConnectStatusFreeList(struct hubConnectStatus **pList);
/* Free a list of dynamically allocated hubConnectStatus's */

struct hubConnectStatus *hubConnectStatusForId( struct sqlConnection *conn, 
    int id);
/* Given a hub ID return associated status. */

struct hubConnectStatus *hubConnectStatusListFromCart(struct cart *cart);
/* Return list of track hubs that are turned on by user in cart. */

struct hubConnectStatus *hubConnectStatusListFromCartAll(struct cart *cart);
/* Return list of all track hubs that are referenced by cart. */

#define hubConnectTrackHubsVarName "trackHubs"
/* Name of cart variable with list of track hubs. */

#define hgHubConnectRemakeTrackHub "hgHubConnect.remakeTrackHub"
/* Cart variable to indicate trackHub cart variable needs refreshing. */

#define hgHubConnectHubVarPrefix "hgHubConnect.hub."
/* Prefix to temporary variable holding selected cart names. */

boolean hubConnectTableExists();
/* Return TRUE if the hubConnect table exists. */

struct slName  *hubConnectHubsInCart(struct cart *cart);
/* Return list of track hub ids that are turned on by user. */

int hubIdFromCartName(char *trackName);
/* Given something like "hgHubConnect.hub.123" return 123 */

unsigned hubIdFromTrackName(char *trackName);
/* Given something like "hub_123_myWig" return 123 */

void hubConnectAddDescription(char *database, struct trackDb *tdb);
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */

struct trackDb *hubConnectAddHubForTrackAndFindTdb( char *database, 
    char *trackName, struct trackDb **pTdbList, struct hash *trackHash);
/* Go find hub for trackName (which will begin with hub_), and load the tracks
 * for it, appending to end of list and adding to trackHash.  Return the
 * trackDb associated with trackName. */

char *hubFileVar();
/* return the name of the cart variable that holds the name of the
 * file in trash that has private hubs */

boolean hubWriteToFile(FILE *f, struct hubConnectStatus *el);
/* write out a hubConnectStatus structure to a file */

unsigned hubFindOrAddUrlInStatusTable(char *database, struct cart *cart,
    char *url, char **errorMessage);
/* find or add a URL to the status table */

unsigned hubResetError(char *url);
/* clear the error for this url in the hubStatus table,return the id */

unsigned hubClearStatus(char *url);
/* drop the information about this url from the hubStatus table,return the id */

void hubDisconnect(struct cart *cart, char *url);
/* drop the information about this url from the hubStatus table, and 
 * the cart variable the references this hub */

void hubUpdateStatus(char *errorMessage, struct hubConnectStatus *hub);
/* set the error message in the hubStatus table */

boolean trackHubHasDatabase(struct trackHub *hub, char *database) ;
/* Return TRUE if hub has contents for database */

struct trackDb *hubAddTracks(struct hubConnectStatus *hub, char *database);
/* Load up stuff from data hub and append to list. The hubUrl points to
 * a trackDb.ra format file.  */

char *hubConnectLoadHubs(struct cart *cart);
/* load the track data hubs.  Set a static global to remember them */

struct hubConnectStatus *hubConnectGetHubs();
/* return the static global to the track data hubs */

struct trackHub *hubConnectGetHub(char *hubUrl);
/* Return the connected hub for hubUrl, or NULL if not found.  Do not free result. */

struct trackHub *hubConnectGetHubForDb(char *db);
/* Return the connected hub for db, or NULL if not found.  Do not free result. */

struct trackDb *hubCollectTracks( char *database, struct grp **pGroupList);
/* Generate trackDb structures for all the tracks in attached hubs.  
 * Make grp structures for each hub. Returned group list is reversed. */

char *hubConnectSkipHubPrefix(char *trackName);
/* Given something like "hub_123_myWig" return myWig.  Don't free this, it's not allocated */

struct hubConnectStatus *hubConnectNewHub();
/* return the hub of the hubUrl we added (if any) */

char *hubPublicTableName();
/* Get the name of the table that lists public hubs.  Don't free the result. */

char *hubNameFromUrl(char *hubUrl);
/* Given the URL for a hub, return its hub_# name. */

#endif /* HUBCONNECT_H */
