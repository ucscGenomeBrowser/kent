/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubConnect table in the hgCentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubConnect 
 * table by design.  We just want field-by-field access to this. */

#ifndef HUBCONNECT_H
#define HUBCONNECT_H

#define hubPublicTableName "hubPublic"
/* Name of our table with list of public hubs. read only */
#define hubStatusTableName "hubStatus"
/* Name of table that maintains status of hubs  read/write. */

#define hgHubDataText      "hubUrl"
/* name of cgi variable containing new hub name */

#define hubTrackPrefix "hub_"
/* The names of all hub tracks begin with this.  Use in cart. */

boolean isHubTrack(char *trackName);
/* Return TRUE if it's a hub track. */

struct hubConnectStatus
/* Basic status in hubStatus.  Note it is *not* the same as the
 * hubStatus table, that has a bunch of extra fields to help 
 * keep track of whether the hub is alive. */
    {
    struct hubConnectStatus *next;
    unsigned id;	/* Hub ID */
    char *shortLabel;	/* Hub short label. */
    char *longLabel;	/* Hub long label. */
    char *hubUrl;	/* URL to hub.ra file. */
    char *errorMessage;	/* If non-empty hub has an error and this describes it. */
    unsigned dbCount;	/* Number of databases hub has data for. */
    char **dbArray;	/* Array of databases hub has data for. */
    unsigned  status;   /* 1 if private */
    };

/* status bits */
#define HUB_UNLISTED    (1 << 0)

boolean isHubUnlisted(struct hubConnectStatus *hub) ;
/* Return TRUE if it's an unlisted hub */

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

int hubIdFromCartName(char *trackName);
/* Given something like "hgHubConnect.hub.123" return 123 */

unsigned hubIdFromTrackName(char *trackName);
/* Given something like "hub_123_myWig" return 123 */

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

unsigned hubCheckForNew(char *database, struct cart *cart);
/* see if the user just typed in a new hub url, return hubId if so */

struct trackHub *trackHubFromId(unsigned hubId);
/* Given a hub ID number, return corresponding trackHub structure. 
 * ErrAbort if there's a problem. */

void hubSetErrorMessage(char *errorMessage, unsigned id);
/* set the error message in the hubStatus table */
#endif /* HUBCONNECT_H */
