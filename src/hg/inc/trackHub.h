/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

#include "dystring.h"

#define MAX_HUB_TRACKDB_FILE_SIZE    64*1024*1024
#define MAX_HUB_GROUP_FILE_SIZE     16*1024*1024
#define MAX_HUB_GENOME_FILE_SIZE    64*1024*1024

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
    char *defaultDb;    /* the default database  */

    char *name;	/* Symbolic name of hub in cart, etc.  From trackHubOpen hubName parameter. */

    char *descriptionUrl;  /* URL to description file */
    char *email;           /* email address of contact person */
    char *version;  /* version compliance of hub ("V1.0", etc.) */
    char *level;    /* support level of hub ("core", "full") */
    };

struct trackHubGenome
/* A genome serviced within a track hub. */
    {
    struct trackHubGenome *next;
    char *name;	/* Something like hg18 or mm9, a UCSC assembly database name. */
    char *trackDbFile;	/* The base trackDb.ra file. */
    struct hash *settingsHash;	/* Settings from hub.ra file. */
    char *twoBitPath;  /* URL to twoBit.  If not null, this is an assmebly hub*/
    struct twoBitFile *tbf;  /* open handle to two bit file */
    char *groups;	     /* URL to group.txt file */
    char *defaultPos;        /* default position */
    char *organism;          /* organism name, like Human */
    char *description;       /* description, also called freeze name */
    struct trackHub *trackHub; /* associated track hub */
    unsigned orderKey;   /* the orderKey for changing the order from the order in the file */
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

void trackHubGenomeFreeList(struct trackHub *hub);
/* Free a list of dynamically allocated trackHubGenome's. */

void trackHubPolishTrackNames(struct trackHub *hub, struct trackDb *tdbList);
/* Remove all the special characters from trackHub track names. */

char *trackHubCladeToGenome(char *clade);
/* Given a track hub clade(hub name) return the default genome. */

boolean trackHubDatabase(char *database);
/* Is this an assembly from an Assembly Data hub? */

char *trackHubDefaultChrom(char *database);
/* Return the default chromosome for this track hub assembly. */

char *trackHubAssemblyField(char *database, char *field);
/* Get data field from a assembly data hub. */

int trackHubChromCount(char *database);
/* Return number of chromosomes in a assembly data hub. */

struct slName *trackHubAllChromNames(char *database);
/* Return a list of all the chrom names in this assembly hub database. */

struct chromInfo *trackHubAllChromInfo(char *database);
/* Return a chromInfo structure for all the chroms in this database. */

struct chromInfo *trackHubMaybeChromInfo(char *database, char *chrom);
/* Return a chromInfo structure for just this chrom in this database. 
 * Return NULL if chrom doesn't exist. */

struct chromInfo *trackHubChromInfo(char *database, char *chrom);
/* Return a chromInfo structure for just this chrom in this database. 
 * errAbort if chrom doesn't exist. */

char *trackHubGenomeNameToDb(char *genome);
/* Return assembly name given a genome name if one exists, otherwise NULL. */

struct dbDb *trackHubGetDbDbs(char *clade);
/* Get a list of dbDb structures for all the tracks in this clade/hub. */

struct slPair *trackHubGetCladeLabels();
/* Get a list of labels describing the loaded assembly data hubs. */

char *trackHubAssemblyClade(char *genome);
/* Return the clade/hub_name that contains this genome. */

void trackHubFixName(char *name);
/* Change all characters other than alphanumeric, dash, and underbar
 * to underbar. */

struct grp *trackHubLoadGroups(char *database);
/* Load the grp structures for this track hub database. */

char *trackHubSkipHubName(char *name);
/* Skip the hub_#_ prefix in a hub name. */

struct dbDb *trackHubDbDbFromAssemblyDb(char *database);
/* Return a dbDb structure for just this database. */

struct hgPositions;
void trackHubFindPos(struct cart *cart, char *db, char *term, struct hgPositions *hgp);
/* Look for term in track hubs.  Update hgp if found */

void trackHubAddDescription(char *trackDbFile, struct trackDb *tdb);
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */

void trackHubAddOneDescription(char *trackDbFile, struct trackDb *tdb);
/* Fetch tdb->track's html description and store in tdb->html. */

struct trackHubGenome *trackHubGetGenome(char *database);
/* get genome structure for an assembly in a trackHub */

boolean trackHubGetBlatParams(char *database, boolean isTrans, char **pHost,
    char **pPort);
/* get "blat" and "transBlat" entries (if any) for an assembly hub */

struct dbDb *trackHubGetBlatDbDbs();
/* Get a list of connected track hubs that have blat servers */

struct slPair *trackHubDbDbToValueLabel(struct dbDb *hubDbDbList);
/* Given a trackHub (list of) track hub dbDb which may be missing some info,
 * return an slPair of value and label suitable for making a select/menu option. */

void hubCheckBigDataUrl(struct trackHub *hub, struct trackHubGenome *genome,
    struct trackDb *tdb);
/* Check remote file exists and is of correct type. Wrap this in error catcher */

#endif /* TRACKHUB_H */

