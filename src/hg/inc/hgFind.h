/* hgFind.h - Find things in human genome annotations. */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HGFIND_H
#define HGFIND_H

#ifndef CART_H
#include "cart.h"
#endif

#ifndef HGFINDSPEC_H
#include "hgFindSpec.h"
#endif

struct hgPositions *hgPositionsFind(char *db, char *query, char *extraCgi, 
	char *hgAppName, struct cart *cart, boolean multiTerm);
/* Return container of tracks and positions (if any) that match term. */

struct hgPositions *hgFindSearch(struct cart *cart, char **pPosition,
                                 char **retChrom, int *retStart, int *retEnd,
                                 char *hgAppName, struct dyString *dyWarn);
/* If *pPosition is a search term, then try to resolve it to genomic position(s).
 * If unable to find a unique position then revert pPosition to lastPosition (or default position).
 * Return a container of matching tables and positions.  Warnings/errors are appended to dyWarn. */

void hgPositionsHtml(char *db, struct hgPositions *hgp, char *hgAppName, struct cart *cart);
/* Write multiple search results as HTML. */


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


void hgPositionsHelpHtml(char *organism, char *database);
/* Display contents of dbDb.htmlPath for database, or print an HTML comment 
 * explaining what's missing. */

char *hCarefulTrackOpenVis(char *db, char *trackName);
/* If track is already in full mode, return full; otherwise, return
 * hTrackOpenVis. */

#endif /* HGFIND_H */

