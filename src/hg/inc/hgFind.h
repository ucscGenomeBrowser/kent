/* hgFind.h - Find things in human genome annotations. */

#ifndef HGFIND_H
#define HGFIND_H

#ifndef CART_H
#include "cart.h"
#endif

boolean handleTwoSites(char *spec, char **retChromName, int *retWinStart, int *retWinEnd, struct cart *cart);
/* Handle queries of the form site1,site2. */

boolean findGenomePos(char *spec, char **retChromName, int *retWinStart, int *retWinEnd, struct cart *cart);
/* Search for positions in genome that match user query.   
Return TRUE if the query results in a unique position.  
Otherwise display list of positions and return FALSE. */

boolean hgFindCytoBand(char *spec, char **retChromName, int *retWinStart, int *retWinEnd);
/* Return position associated with cytological band if spec looks to be in that form. */

boolean hgIsCytoBandName(char *spec, char **retChromName, char **retBandName);
/* Return TRUE if spec is a cytological band name including chromosome short name.  
 * Returns chromosome chrN name and band (with chromosome stripped off). */

void hgFindChromBand(char *chromosome, char *band, int *retStart, int *retEnd);
/* Return start/end of band in chromosome. */

void findContigPos(char *contig, char **retChromName, 
			  int *retWinStart, int *retWinEnd);

boolean hgFindClonePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd);


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

void hgPositionsFree(struct hgPositions **pEl);
/* Free up hgPositions. */

void hgPositionsFreeList(struct hgPositions **pList);
/* Free a list of dynamically allocated hgPos's */

struct hgPositions *hgPositionsFind(char *query, char *extraCgi, 
	boolean useHgTracks, struct cart *cart);
/* Return table of positions that match query or NULL if none such. */

void hgPositionsHtml(struct hgPositions *positions, FILE *f, boolean useHgTracks, 
	struct cart *cart);
/* Write out positions table as HTML to file. */

struct hgPosTable
/* A collection of position lists, one for each type of position. */
    {
    struct hgPosTable *next;	/* Next table in list. */
    char *name;			/* Name of table.  Not allocated here. */
    struct hgPos *posList;      /* List of positions in this table. */
    void (*htmlStart)(struct hgPosTable *table, FILE *f);   /* Print preamble to positions. */
    void (*htmlOnePos)(struct hgPosTable *table, struct hgPos *pos, FILE *f); /* Print one position. */
    void (*htmlEnd)(struct hgPosTable *table, FILE *f);    /* Print end. */
    };

void hgPosTableFree(struct hgPosTable **pEl);
/* Free up hgPosTable. */

void hgPosTableFreeList(struct hgPosTable **pList);
/* Free a list of dynamically allocated hgPos's */

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

void hgPosFree(struct hgPos **pEl);
/* Free up hgPos. */

void hgPosFreeList(struct hgPos **pList);
/* Free a list of dynamically allocated hgPos's */

char *hgPosBrowserRange(struct hgPos *pos, char range[64]);
/* Convert pos to chrN:123-456 format.  If range parameter is NULL it returns
 * static buffer, otherwise writes and returns range. */

#endif /* HGFIND_H */

