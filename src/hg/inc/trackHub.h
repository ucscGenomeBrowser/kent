/* trackHub - supports collections of tracks hosted on a remote site.
 * The basic layout of a data hub is:
 *        hub.ra - contains information about the hub itself
 *        genomes.ra - says which genomes are supported by hub
 *                 Contains file name of trackDb.ra for each genome
 *        trackDb.ra - contains a stanza for each track.  Stanzas
 *                 are in a subset of the usual trackDb format. 
 * How you use the routines here most commonly is as so:
 *     struct trackHub *hub = trackHubOpen(hubRaUrl);
 *     struct trackHubGenome *hubGenome = trackHubFindGenome(hub, "hg19");
 *     struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
 *          // do something with tdbList
 *     trackHubClose(&hub);
 * Note that the tdbList returned does not have the parent/subtrack pointers set.
 * It is just a simple list of tracks, not a tree.  
 */

#ifndef TRACKHUB_H
#define TRACKHUB_H

struct trackHub 
/* A track hub. */
    {
    struct trackHub *next;

    char *url;		/* URL of hub.ra file. */
    struct trackHubGenome *genomeList;	/* List of associated genomes. */
    struct hash *genomeHash;	/* Hash of genomeList keyed by genome name. */
    struct hash *settings;	/* Settings from hub.ra file. */

    /* Required settings picked out for convenience. All allocated in settings hash */
    char *shortLabel;	/* Hub short label. Not allocated here. */
    char *longLabel;	/* Hub long label. Not allocated here. */
    char *genomesFile;	/* URL to genome.ra file. Not allocated here. */

    char *name;	/* Symbolic name of hub in cart, etc.  From trackHubOpen hubName parameter. */
    };

struct trackHubGenome
/* A genome serviced within a track hub. */
    {
    struct trackHubGenome *next;
    char *name;	/* Something like hg18 or mm9 - a UCSC assembly database name. */
    char *trackDbFile;	/* The base trackDb.ra file. */
    };

void trackHubClose(struct trackHub **pHub);
/* Close up and free resources from hub. */

struct trackHub *trackHubOpen(char *url, char *hubName);
/* Open up a track hub from url.  Reads and parses hub.ra and the genomesFile. 
 * The hubName is generally just the asciified ID number. */

struct trackHubGenome *trackHubFindGenome(struct trackHub *hub, char *genomeName);
/* Return trackHubGenome of given name associated with hub.  Return NULL if no
 * such genome. */

struct trackDb *trackHubTracksForGenome(struct trackHub *hub, struct trackHubGenome *genome);
/* Get list of tracks associated with genome.  Check that it only is composed of legal
 * types.  Do a few other quick checks to catch errors early. */

void trackHubAddNamePrefix(char *hubName, struct trackDb *tdbList);
/* For a hub named "xyz" add the prefix "hub_xyz_" to each track and parent field. 
 * This is useful to the genome browser which directly puts tracks into it's
 * user settings name space.... */

void trackHubAddGroupName(char *hubName, struct trackDb *tdbList);
/* Add group tag that references the hubs symbolic name. */

char *trackHubSetting(struct trackHub *hub, char *name);
/* Return setting if it exists, otherwise NULL. */

char *trackHubRequiredSetting(struct trackHub *hub, char *name);
/* Return named setting.  Abort with error message if not found. */

char *trackHubRelativeUrl(char *hubUrl, char *path);
/* Return full path (in URL form if it's a remote hub) given
 * path possibly relative to hubUrl. Do a freeMem of result
 * when done. */

void trackHubGenomeFree(struct trackHubGenome **pGenome);
/* Free up genome info. */

void trackHubGenomeFreeList(struct trackHubGenome **pList);
/* Free a list of dynamically allocated trackHubGenome's */

int trackHubCheck(char *hubUrl, struct dyString *errors);
/* trackHubCheck - Check a track data hub for integrity. Put errors in dyString.
 *      return 0 if hub has no errors, 1 otherwise */


#endif /* TRACKHUB_H */

