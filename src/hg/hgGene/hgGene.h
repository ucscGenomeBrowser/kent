/* hgGene.h - important structures and interfaces between
 * modules of the gene details viewer. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef CART_H
#include "cart.h"
#endif 

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef GENEPRED_H
#include "genePred.h"
#endif

#ifndef HPRINT_H
#include "hPrint.h"
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

    /* Some FlyBase specific stuff. */
    char *flyBaseTable;	/* Which table to use. */
    char *rgdGeneTable;	/* Which table to use. */

    void *extras;        // Contains any section specific stuff
    int checkTime;      // Time to check existence (for performance tuning) */
    int printTime;      // Time to print (for performance tuning) */
    };

struct section *sectionNew(struct hash *sectionRa, char *name);
/* Create a section loading all common parts but the methods 
 * sectionRa. */

struct section *gencodeSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create gencode section. */

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

struct section *rgdGeneRawSection(struct sqlConnection *conn,
        struct hash *sectionRa);
/* Create rgdGeneRaw section */

struct section *flyBaseRolesSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create FlyBase roles section. */

struct section *flyBasePhenotypesSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create FlyBase Phenotypesroles section. */

struct section *flyBaseSynonymsSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create FlyBase synonyms section. */

struct section *bdgpExprInSituSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create BDGP Expression in situ image links section. */

struct section *goSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create GO annotations section. */

struct section *infoSection(struct sqlConnection *conn,
        struct hash *sectionRa);
/* Create UCSC KG Model Info section. */

struct section *methodSection(struct sqlConnection *conn,
        struct hash *sectionRa);
/* Create UCSC KG Method section. */

struct section *pseudoGeneSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create pseudoGene section. */

struct section *mrnaDescriptionsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create mrnaDescriptions section. */

struct section *microarraySection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create microarray section. */

struct section *altSpliceSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create altSplice section. */

struct section *ctdSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create CTD section. */

struct section *primersSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create primers section. */

struct section *ctdRgdGene2Section(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create CTD section. */

struct section *gadSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create GAD section. */

struct section *malaCardsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create malaCards section. */

struct section *domainsSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create domains section. */

struct section *pathwaysSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create pathways section. */

struct section *rnaStructureSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create rnaStructure section. */

struct section *localizationSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create Localization section. */

struct section *transRegCodeMotifSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create dnaBindMotif section. */

struct section *synonymSection(struct sqlConnection *conn,
	struct hash *sectionRa);
/* Create synonym (aka Other Names) section. */

struct section *geneReviewsSection(struct sqlConnection *conn,
        struct hash *sectionRa);
/* create geneReviews section - print out GeneReviews for this gene. */

#define HGG_GENE_ALLELES "geneAlleles"
struct section *allelesSection(struct sqlConnection *conn,
        struct hash *sectionRa);
// Create Common Gene Haplotype Alleles section.

void prGRShortKg(struct sqlConnection *conn, char *itemName);
/* print GeneReviews short label associated to this refGene item */

struct section *gtexSection(struct sqlConnection *conn, struct hash *sectionRa);
/* Create GTEx gene expression section. */

#ifdef EXAMPLE
struct section *xyzSection(struct sqlConnection *conn, 
	struct hash *sectionRa);
/* Create xyz section. */
#endif /* EXAMPLE */

/* -------- Helper functions ---------- */

char *getGeneName(char *id, struct sqlConnection *conn);
/* Return gene name associated with ID.  Freemem
 * this when done. */

char *getSwissProtAcc(struct sqlConnection *conn, struct sqlConnection *spConn, 
	char *geneId);
/* Look up SwissProt id.  Return NULL if not found.  FreeMem this when done.
 * spConn is existing SwissProt database conn.  May be NULL. */

char *genomeSetting(char *name);
/* Return genome setting value.   Aborts if setting not found. */

char *genomeOptionalSetting(char *name);
/* Returns genome setting value or NULL if not found. */

struct hash *readRa(char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database. */

int gpRangeIntersection(struct genePred *gp, int start, int end);
/* Return number of bases range start,end shares with genePred. */

boolean checkDatabases(char *databases);
/* Check all databases in space delimited string exist. */

boolean isFly();
/* Return true if organism is D. melanogaster. */

boolean isRgdGene(struct sqlConnection *conn);
/* Return true if the gene set is RGD Genes. */

char *getRgdGeneUniProtAcc(char *genId, struct sqlConnection *conn);

char *getFlyBaseId(struct sqlConnection *conn, char *geneId);
/* Return flyBase ID of gene if any. */

void showSeqFromTable(struct sqlConnection *conn, char *geneId,
	char *geneName, char *table);
/* Show some sequence from given table. */

void printPrimer3Anchor(char *table, char *itemName,
	char *chrom, int start, int end);
/* Print primer3 sequence export anchor. */

void printGenomicSeqLink(struct sqlConnection *conn, char *geneId,
	char *chrom, int start, int end);
/* Figure out known genes table, position of gene, link it. */

void printProteinSeqLink(struct sqlConnection *conn, char *geneId);
/* Print out link to fetch protein. */

void printMrnaSeqLink(struct sqlConnection *conn, char *geneId);
/* Print out link to fetch mRNA. */

char *descriptionString(char *id, struct sqlConnection *conn);
/* return description as it would be printed in html, can free after use */

char *aliasString(char *id, struct sqlConnection *conn);
/* return alias string as it would be printed in html, can free after use */

/* -------- CGI Command Variables ---------- */
#define hggDoPrefix "hgg_do_"	/* Prefix for all commands. */
#define hggDoKgMethod "hgg_do_kgMethod"
#define hggDoWikiTrack "hgg_do_wikiTrack"
#define hggDoWikiAddComment "hgg_do_wikiAddComment"
#define hggDoGetMrnaSeq "hgg_do_getMrnaSeq"
#define hggDoGetProteinSeq "hgg_do_getProteinSeq"
#define hggDoRnaFoldDisplay "hgg_do_rnaFoldDisplay"
#define hggDoOtherProteinSeq "hgg_do_otherProteinSeq"
#define hggDoOtherProteinAli "hgg_do_otherProteinAli"
#define hggDoTxInfoDescription "hgg_do_txInfoDescription"
#define hggAjaxSection         "ajaxSection"


#define geneCgi "../cgi-bin/hgGene"

/* -------- Commands ---------- */

void doKgMethod();
/* Present KG Method, Credits, and Data Use Restrictions. */

void doWikiTrack(struct sqlConnection *conn);
/* Put up wiki track editing controls */

void doTxInfoDescription();
/* Put up info on fields in txInfo table. */

void doGetMrnaSeq(struct sqlConnection *conn, char *geneId, char *geneName);
/* Get mRNA sequence in a simple page. */

void doGetProteinSeq(struct sqlConnection *conn, char *geneId, char *geneName);
/* Get mRNA sequence in a simple page. */

void doOtherProteinSeq(struct sqlConnection *conn, char *homologName);
/* Put up page that displays protein sequence from other organism. */

void doOtherProteinAli(struct sqlConnection *conn, 
	char *localId, char *localName);
/* Put up page that shows alignment between this protein sequence
 * and other species. */

void doRnaFoldDisplay(struct sqlConnection *conn, char *geneId, char *geneName);
/* Display mRNA sequence folding. */

void doSamT02(char *proteinId, char *database);
/* show SAM-T02 sub-section */

/* -------- CGI Data Variables ---------- */
#define hggOrg "org"		/* Organism we're working on. */
#define hggDb "db"		/* Database we're working on. */
#define hggPrefix "hgg_"	/* Prefix to all 'local' cart vars. */
#define hggGene "hgg_gene"	/* Main gene id. */
#define hggProt "hgg_prot"      /* Main protein id. */
#define hggChrom "hgg_chrom"	/* Chromosome gene is on. */
#define hggStart "hgg_start"	/* Start position. */
#define hggType "hgg_type"	/* Table name */
#define hggEnd	"hgg_end"	/* End position. */
#define hggItem	"hgg_item"	/* Gene item, used for ccdsGene */
#define hggExpRatioColors "hgg_expRatioColors" /* Expression Ratio coloring. */
#define hggMrnaFoldRegion "hgg_mrnaFoldRegion"	/* Which region in mRNA to show. */
#define hggMrnaFoldPs	"hgg_mrnaFoldPs"	/* PostScript file. */
#define hggOtherId "hgg_otherId"	/* Other organism gene id. */
#define hggOtherPepTable "hgg_otherPepTable"	/* Other organism peptide table. */

/* -------- Global Variables --------*/
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldCart;	/* Old cart hash. */
extern char *database;		/* Name of genome database - hg15, mm3, or the like. */
extern char *liftDb;		/* Name of database we're quickLifting from (NULL if we aren't lifting) */
extern char *genome;		/* Name of genome - mouse, human, etc. */
extern char *curGeneId;		/* Current Gene Id. */
extern char *curAlignId;		/* Current Align Id. */
extern char *curProtId;		/* Current protein Id. */
extern char *curGeneName;		/* Biological name of gene. */
extern char *curGeneChrom;	/* Chromosome current gene is on. */
extern struct genePred *curGenePred;	/* Current gene prediction structure. */
extern boolean isGencode;              /* is this based on the Gencode models */
extern boolean isGencode2;             /* is this based on the Gencode models and use ensembl id as primary id */
extern int curGeneStart,curGeneEnd;	/* Position in chromosome. */
extern struct sqlConnection *spConn;	/* Connection to SwissProt database. */
extern char *swissProtAcc;	/* SwissProt accession (may be NULL). */
extern struct trackDb *globalTdb;  /* The trackDb structure for the table containing the current item. */

#define KG_UNKNOWN 0
#define KG_I       1
#define KG_II      2
#define KG_III     3
extern int kgVersion;           /* KG version */

