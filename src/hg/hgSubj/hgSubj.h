/* hgSubj.h - important structures and interfaces between
 * modules of the gene details viewer. */

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

//#ifndef GENEPRED_H
//#include "genePred.h"
//#endif

#define GSBLANKS "&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp"

struct section
/* A section of the details page - put in the page
 * index. */
    {
    struct section *next;
    char *name;		/* Symbolic name. */
    char *shortLabel;	/* Short human readable label - 2 words or less. */
    char *longLabel;	/* Long human readable label. */
    double priority;	/* Order of section. */
    struct hash *settings;	/* Settings hash from section.ra. */

    boolean (*exists)(struct section *section, struct sqlConnection *conn,
    	char *geneId);
    /* Return TRUE if this section needs to be printed. */

    void (*print)(struct section *section, struct sqlConnection *conn,
    	char *geneId);
    /* Print section given connection to genome database. */

    /* Some section-specific data. */
    char *raFile;	/* Ra file to load additional info from. */
    void *items;	/* Some list of items. */

    /* Some FlyBase specific stuff. */
    char *flyBaseTable;	/* Which table to use. */
    };

struct section *sectionNew(struct hash *sectionRa, char *name);
/* Create a section loading all common parts but the methods 
 * sectionRa. */

struct section *linksSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create links section. */

struct section *sequenceSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create sequence section. */

struct section *demogSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create Demographic section. */

struct section *vaccineSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create Vaccine section. */

struct section *clinicalSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create Clinical section. */

#ifdef EXAMPLE
struct section *xyzSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create xyz section. */
#endif /* EXAMPLE */

/* -------- Helper functions ---------- */

char *getGeneName(char *id, struct sqlConnection *conn);
/* Return gene name associated with ID.  Freemem
 * this when done. */

char *genomeSetting(char *name);
/* Return genome setting value.   Aborts if setting not found. */

char *genomeOptionalSetting(char *name);
/* Returns genome setting value or NULL if not found. */

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

struct hash *readRa(char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database. */

boolean checkDatabases(char *databases);
/* Check all databases in space delimited string exist. */

/* -------- Commands ---------- */

/* -------- CGI Data Variables ---------- */
#define hgsSubj "hgs_subj"	/* Main gene id. */

/* -------- Global Variables --------*/
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldCart;	/* Old cart hash. */
extern char *database;		/* Name of genome database - hg15, mm3, or the like. */
extern char *genome;		/* Name of genome - mouse, human, etc. */
