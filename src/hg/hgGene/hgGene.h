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
    void *items;	/* Some list of items. */
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

struct section *sequenceSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create sequence section. */

struct section *swissProtCommentsSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create SwissProt comments section. */

struct section *goSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create GO annotations section. */

struct section *mrnaDescriptionsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create mrnaDescriptions section. */

#ifdef EXAMPLE
struct section *xyzSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create xyz section. */
#endif /* EXAMPLE */

/* -------- Helper functions ---------- */

char *genomeSetting(char *name);
/* Return genome setting value.   Aborts if setting not found. */

char *genomeOptionalSetting(char *name);
/* Returns genome setting value or NULL if not found. */

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

void hPrintLinkTableStart();
/* Print link table start in our colors. */

void hPrintLinkTableEnd();
/* Print link table end in our colors. */

void hPrintLinkCellStart();
/* Print link cell start in our colors. */

void hPrintLinkCellEnd();
/* Print link cell end in our colors. */

struct hash *readRa(char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database. */

char *swissProtAcc(struct sqlConnection *conn, struct sqlConnection *spConn, char *geneId);
/* Look up SwissProt id.  Return NULL if not found.  FreeMem this when done.*/

/* -------- CGI Command Variables ---------- */
#define hggDoPrefix "hgg_do_"	/* Prefix for all commands. */
#define hggDoGetMrnaSeq "hgg_do_getMrnaSeq"
#define hggDoGetProteinSeq "hgg_do_getProteinSeq"

/* -------- Commands ---------- */

void doGetMrnaSeq(struct sqlConnection *conn, char *geneId, char *geneName);
/* Get mRNA sequence in a simple page. */

void doGetProteinSeq(struct sqlConnection *conn, char *geneId, char *geneName);
/* Get mRNA sequence in a simple page. */

/* -------- CGI Data Variables ---------- */
#define hggOrg "org"		/* Organism we're working on. */
#define hggDb "db"		/* Database we're working on. */
#define hggGene "hgg_gene"	/* Main gene id. */
#define hggChrom "hgg_chrom"	/* Chromosome gene is on. */
#define hggStart "hgg_start"	/* Start position. */
#define hggEnd	"hgg_end"	/* End position. */

/* -------- Global Variables --------*/
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldCart;	/* Old cart hash. */
extern char *database;		/* Name of genome database - hg15, mm3, or the like. */
extern char *genome;		/* Name of genome - mouse, human, etc. */
extern char *curGeneId;	/* Current Gene Id. */
extern char *curGeneName;		/* Biological name of gene. */
extern char *curGeneChrom;	/* Chromosome current gene is on. */
extern int curGeneStart,curGeneEnd;	/* Position in chromosome. */

