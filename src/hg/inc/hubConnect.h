/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubConnect table in the hgCentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubConnect 
 * table by design.  We just want field-by-field access to this. */

#ifndef HUBCONNECT_H
#define HUBCONNECT_H

#define hubConnectTableName "hubConnect"
/* Name of our table. */

#define hubTrackPrefix "hub_"
/* The names of all hub tracks begin with this.  Use in cart. */

boolean isHubTrack(char *trackName);
/* Return TRUE if it's a hub track. */

struct hubConnectStatus
/* Basic status on hubConnect.  Note it is *not* the same as the
 * hubConnect table, that has a bunch of extra fields to help 
 * keep track of whether the hub is alive. */
    {
    struct hubConnectStatus *next;
    int id;	/* Hub ID */
    char *shortLabel;	/* Hub short label. */
    char *longLabel;	/* Hub long label. */
    char *hubUrl;	/* URL to hub.ra file. */
    char *errorMessage;	/* If non-empty hub has an error and this describes it. */
    unsigned dbCount;	/* Number of databases hub has data for. */
    char **dbArray;	/* Array of databases hub has data for. */
    };

void hubConnectStatusFree(struct hubConnectStatus **pHub);
/* Free hubConnectStatus */

void hubConnectStatusFreeList(struct hubConnectStatus **pList);
/* Free a list of dynamically allocated hubConnectStatus's */

struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id);
/* Given a hub ID return associated status. */

struct hubConnectStatus *hubConnectStatusFromCart(struct cart *cart);
/* Return list of track hubs that are turned on by user in cart. */

#define hubConnectTrackHubsVarName "trackHubs"
/* Name of cart variable with list of track hubs. */

#define hgHubConnectCgiDestUrl "hgHubConnect.destUrl"
/* Cart variable to tell hgHubConnect where to go on submit. */

#define hgHubConnectRemakeTrackHub "hgHubConnect.remakeTrackHub"
/* Cart variable to indicate trackHub cart variable needs refreshing. */

#define hgHubConnectHubVarPrefix "hgHubConnect.hub."
/* Prefix to temporary variable holding selected cart names. */

boolean hubConnectTableExists();
/* Return TRUE if the hubConnect table exists. */

struct slName  *hubConnectHubsInCart(struct cart *cart);
/* Return list of track hub ids that are turned on by user. */

int hubIdFromTrackName(char *trackName);
/* Given something like "hub_123_myWig" return 123 */

struct trackDb *hubConnectAddHubForTrackAndFindTdb(char *database, char *trackName,
	struct trackDb **pTdbList, struct hash *trackHash);
/* Go find hub for trackName (which will begin with hub_), and load the tracks
 * for it, appending to end of list and adding to trackHash.  Return the
 * trackDb associated with trackName. */

#endif /* HUBCONNECT_H */
