/* hgGene.h - important structures and interfaces between
 * modules of the gene details viewer. */

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

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
    };

struct section *sectionNew(struct hash *sectionRa, char *name);
/* Create a section loading all common parts but the methods 
 * sectionRa. */

struct section *linksSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create links section. */

struct section *otherOrgsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create other organisms section. */

#ifdef EXAMPLE
struct section *xyzOrgsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create xyz section. */
#endif /* EXAMPLE */

/* -------- Helper functions ---------- */

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

struct hash *readRa(char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database. */

/* -------- CGI Variables ---------- */
#define hggOrg "org"		/* Organism we're working on. */
#define hggDb "db"		/* Database we're working on. */
#define hggGene "hgg_gene"	/* Main gene id. */

/* -------- Global Variables --------*/
struct cart *cart;	/* This holds cgi and other variables between clicks. */
