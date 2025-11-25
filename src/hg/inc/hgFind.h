/* hgFind.h - Find things in human genome annotations. */

/* Copyright (C) 2010 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGFIND_H
#define HGFIND_H

#ifndef CART_H
#include "cart.h"
#endif

#ifndef HGFINDSPEC_H
#include "hgFindSpec.h"
#endif


struct hgPositions
/* A bunch of positions in genome. */
    {
    struct hgPositions *next;	  /* Next in list. */
    char *query;		  /* Query string that led to positions. */
    char *database;               /* Name of database.  Not allocated here. */
    struct hgPosTable *tableList; /* List of tables. */
    int posCount;                 /* Number of positions in all tables. */
    struct hgPos *singlePos;      /* If resolves to a single position, reference to that here. */
    char *extraCgi;		  /* Extra info to embed in CGI requests to browser. */
    boolean useAlias;             /* Set if an alias is used */
    boolean shortCircuited;       /* Is this a result of a short circuit?  */
    };

struct hgPosTable
/* A collection of position lists, one for each type of position. */
    {
    struct hgPosTable *next;	/* Next table in list. */
    char *name;			/* Name of table.  Not allocated here. */
    char *description;          /* Table description. No allocated here */
    struct hgPos *posList;      /* List of positions in this table. */
    void (*htmlStart)(struct hgPosTable *table, FILE *f);   /* Print preamble to positions. */
    void (*htmlOnePos)(struct hgPosTable *table, struct hgPos *pos, FILE *f); /* Print one position. */
    void (*htmlEnd)(struct hgPosTable *table, FILE *f);    /* Print end. */
    long searchTime;          /* How long did this search take */
    };

struct hgPos
/* A list of positions. */
     {
     struct hgPos *next;	/* Next in list. */
     char *chrom;		/* Chromosome.  Not allocated here. */
     int chromStart;		/* Start position in chromosome. */
     int chromEnd;		/* End position in chromosome. */
     char *name;		/* Name of position - one word. */
     char *description;		/* Position description - a sentence or so. */
     char *browserName;		/* name as in hgTracks tg->itemName(). */
     bool canonical;		/* The gene is the canonical version. */
     struct tsrPos *tp;		/* The trix search associated with the gene. */
     char *highlight;		/* If non-empty, new value for highlight cart variable */
     };

struct searchCategory
/* struct searchCategory: a model for the category/filter selection in the UI */
    {
    struct searchCategory *next;
    char *id; // The id in the tree of the hgFindSpec to query, should be a valid hgFindSpec or trix name
    char *name; // the label in the tree hfs to query
    char *searchString;
    struct trix *trix; // the associated trix file if any
    char *label; // the label for this search, probably from hgFindSpec/trackDb
    char *description; // the description for this search, probably from hgFindSpec/trackDb
    char visibility; // whether this track is currently on
    char *groupName; // the group name this track belongs to if it's a track
    float priority; // the priority level from trackDb, used for sorting later
    struct slName *parents; // the parents of this track in order from nearest to furthest
    struct slName *errors; // Any error messages we received trying to construct the filter
    struct trackDb *tdb; // Save the trackDb here for easier conversion back to a trackDb list
    };

/* struct searchableTrack: essentially a few fields from a struct trackDb that we need to form the
 * searchCategory */
struct searchableTrack
    {
    struct searchableTrack *next;
    char *track;
    char *shortLabel;
    char *longLabel;
    char *description;
    unsigned char visibility;
    float priority;
    char *grp;
    };

boolean matchesHgvs(struct cart *cart, char *db, char *term, struct hgPositions *hgp,
                            boolean measureTiming);
/* Return TRUE if the search term looks like a variant encoded using the HGVS nomenclature
 * See http://varnomen.hgvs.org/
 * If search term is a pseudo hgvs term like GeneName AminoAcidPosition (RUNX2 Arg155) and
 * matches more than one transcript, fill out the hgp with the potential matches so the user
 * can choose where to go, otherwise return a singlePos */

void fixSinglePos(struct hgPositions *hgp);
/* Fill in posCount and if proper singlePos fields of hgp
 * by going through tables... */

struct hgPositions *hgPositionsFind(char *db, char *query, char *extraCgi,
	char *hgAppName, struct cart *cart, boolean multiTerm, boolean measureTiming, struct searchCategory *categories);
/* Return container of tracks and positions (if any) that match term. */

struct hgPositions *hgFindSearch(struct cart *cart, char **pPosition,
                                 char **retChrom, int *retStart, int *retEnd,
                                 char *hgAppName, struct dyString *dyWarn, struct searchCategory *categories);
/* If *pPosition is a search term, then try to resolve it to genomic position(s).
 * If unable to find a unique position then revert pPosition to lastPosition (or default position).
 * Return a container of matching tables and positions.  Warnings/errors are appended to dyWarn. */

void hgPositionsHtml(char *db, struct hgPositions *hgp, char *hgAppName, struct cart *cart);
/* Write multiple search results as HTML. */

void hgPosFree(struct hgPos **pEl);
/* Free up hgPos. */

void searchCategoryFree(struct searchCategory **el);
/* Free a searchCategory */

#define hgFixedTrix "/gbdb/hgFixed/search/"
#define publicHubsTrix "hubSearchTextRows"
#define helpDocsTrix "searchableDocs"

int cmpCategories(const void *a, const void *b);
/* Compare two categories for uniquifying */

struct trix *openStaticTrix(char *trixName);
/* Open up a trix file in hgFixed */


struct searchCategory *makeCategory(struct cart *cart, char *categName, struct searchableTrack *searchTrack, char *db, struct hash *groupHash);
/* Make a single searchCategory, unless the requested categName is a container
 * track or track group (for example all phenotype tracks), in which case we make
 * categories for each subtrack */

struct searchCategory *getCategsForNonDb(struct cart *cart, char *db, struct hash *groupHash);
/* Return the default categories for all databases */

struct searchCategory *getCategsForDatabase(struct cart *cart, char *db, struct hash *groupHash);
/* Get the default categories to search if user has not selected any before.
 * By default we search for gene loci (knownGene), track names, and track items */

struct searchCategory *getAllCategories(struct cart *cart, char *db, struct hash *groupHash);
/* If we have saved categories for this database from the last search, return those,
 * otherwise return the default selection */

void hgPositionsHelpHtmlCart(struct cart *cart, char *organism, char *database);
/* Display contents of dbDb.htmlPath for database, or print an HTML comment
 * explaining what's missing. */
#define hgPositionsHelpHtml(o, d) hgPositionsHelpHtmlCart(cart, o, d)

char *hCarefulTrackOpenVisCart(struct cart *cart, char *db, char *trackName);
/* If track is already in full mode, return full; otherwise, return
 * hTrackOpenVis. */
#define hCarefulTrackOpenVis(d, t) hCarefulTrackOpenVisCart(cart, d, t)

char *addHighlight(char *db, char *chrom, unsigned start, unsigned end);
/* Return a string that can be assigned to the cart var addHighlight, to add a yellow highlight
 * at db.chrom:start+1-end for search results. */

void hashTracksAndGroups(struct cart *cart, char *db);
/* get the list of tracks available for this assembly, along with their group names
 * and visibility-ness. Note that this implicitly makes connected hubs show up
 * in the trackList struct, which means we get item search for connected
 * hubs for free */
#endif /* HGFIND_H */

