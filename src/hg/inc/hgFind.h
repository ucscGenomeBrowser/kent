/* hgFind.h - Find things in human genome annotations. */

#ifndef HGFIND_H
#define HGFIND_H

#ifndef CART_H
#include "cart.h"
#endif

#ifndef HGFINDSPEC_H
#include "hgFindSpec.h"
#endif


struct hgPositions *findGenomePos(char *spec, char **retChromName, int *retWinStart, int *retWinEnd, struct cart *cart);
/* Search for positions in genome that match user query.   
Return TRUE if the query results in a unique position.  
Otherwise display list of positions and return FALSE. */

struct hgPositions *findGenomePosWeb(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart,
	boolean useWeb, char *hgAppName);
/* Search for positions in genome that match user query.   
 * If useWeb, use the web library to print out HTML headers if necessary.
 * Use hgAppName when forming URLs (instead of "hgTracks").  
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */

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
     };

boolean hgFindUsingSpec(struct hgFindSpec *hfs, char *term,
			struct hgPositions *hgp, boolean relativeFlag,
			int relStart, int relEnd);
/* Perform the search described by hfs on term.  If successful, put results
 * in hgp and return TRUE.  (If not, don't modify hgp.) */

struct hgPositions *hgPositionsFind(char *query, char *extraCgi, 
	char *hgAppName, struct cart *cart);
/* Return table of positions that match query or NULL if none such. */

void hgPositionsHelpHtml(char *organism, char *database);
/* Display contents of dbDb.htmlPath for database, or print an HTML comment 
 * explaining what's missing. */

#endif /* HGFIND_H */

