/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Routines to get data from relational version of
 * SwissProt database.  This database is generated
 * from SwissProt flat files by the spToDb program.
 *
 * Most of the routines take the primary SwissProt accession
 * as input, and will abort if this is not found in the database.
 * Use spIsPrimaryAcc to check for existence first if you want
 * to avoid this. The routines that return strings need them
 * free'd when you are done, and the routines that return lists
 * need them spFreeList'd when done unless you don't mind memory
 * leaks. */


#ifndef SPDB_H
#define SPDB_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#define UNIPROT_DB_NAME  "uniProt"
#define PROTEOME_DB_NAME "proteome"

typedef char SpAcc[20];	/* Enough space for accession and 0 terminator. */

struct spFeature
/* A swissProt feature - that is an annotation attatched to
 * part of a protein. */
    {
    struct spFeature *next;
    int start;	/* Start coordinate (zero-based) */
    int end;	/* End coordinate (non-inclusive) */
    int featureClass; /* ID of featureClass */
    int featureType;  /* ID of featureType */
    char softEndBits;/* 1 for start <, 2 for start ?, 4 for end >, 8 for end ? */
    };
#define spFeatureStartLess 1
#define spFeatureStartFuzzy 2
#define spFeatureEndGreater 4
#define spFeatureEndFuzzy 8

struct spCitation
/* A SwissProt citation of a reference.  Generally
 * you'll just grab the reference id out of this.
 * If you love SwissProt RP and RC lines you might use
 * the other fields. */
    {
    struct spCitation *next;
    int id;	/* Database (not SwissProt) ID */
    SpAcc acc;	/* SwissProt accession. */
    int reference;	/* ID in reference table. */
    int rp;	/* Id in rp table. */
    };

boolean spIsPrimaryAcc(struct sqlConnection *conn, char *acc);
/* Return TRUE if this is a primary accession in database. */

char *spFindAcc(struct sqlConnection *conn, char *id);
/* Return primary accession given either display ID,
 * primary accession, or secondary accession.  Return
 * NULL if not found. */

char *spAnyAccToId(struct sqlConnection *conn, char *acc);

char *spAccToId(struct sqlConnection *conn, char *acc);
/* Convert primary accession to SwissProt ID (which will
 * often look something like HXA1_HUMAN. */

char *spIdToAcc(struct sqlConnection *conn, char *id);
/* Convert SwissProt ID (things like HXA1_HUMAN) to
 * accession. Returns NULL if the conversion fails. 
 * (doesn't abort). */

char *spLookupPrimaryAccMaybe(struct sqlConnection *conn, 
	char *anyAcc); 	/* Primary or secondary accession. */
/* This will return the primary accession.  It's ok to pass in
 * either a primary or secondary accession. Return NULL if
 * not found. */

char *spLookupPrimaryAcc(struct sqlConnection *conn, 
	char *anyAcc); 	/* Primary or secondary accession. */
/* This will return the primary accession.  It's ok to pass in
 * either a primary or secondary accession. ErrAbort if
 * not found */

char *spDescription(struct sqlConnection *conn, char *acc);
/* Return protein description.  FreeMem this when done. */

boolean spIsCurated(struct sqlConnection *conn, char *acc);
/* Return TRUE if it is a curated entry. */

int spAaSize(struct sqlConnection *conn, char *acc);
/* Return number of amino acids. */

int spMolWeight(struct sqlConnection *conn, char *acc);
/* Return molecular weight in daltons. */

char *spCreateDate(struct sqlConnection *conn, char *acc);
/* Return creation date in YYYY-MM-DD format.  FreeMem 
 * this when done. */

char *spSeqDate(struct sqlConnection *conn, char *acc);
/* Return sequence last update date in YYYY-MM-DD format.  FreeMem 
 * this when done. */

char *spAnnDate(struct sqlConnection *conn, char *acc);
/* Return annotation last update date in YYYY-MM-DD format.  FreeMem 
 * this when done. */

char *spOrganelle(struct sqlConnection *conn, char *acc);
/* Return text description of organelle.  FreeMem this when done. 
 * This may return NULL if it's not an organelle. */

struct slName *spGeneToAccs(struct sqlConnection *conn, 
	char *gene, int taxon);
/* Get list of accessions associated with this gene.  If
 * taxon is zero then this will return all accessions,  if
 * taxon is non-zero then it will restrict it to a single
 * organism with that taxon ID. */

struct slName *spGenes(struct sqlConnection *conn, char *acc);
/* Return list of genes associated with accession */

struct slName *spTaxons(struct sqlConnection *conn, char *acc);
/* Return list of taxons associated with accession */

struct slName *spBinomialNames(struct sqlConnection *conn, char *acc);
/* Return list of scientific names of organisms
 * associated with accessoin */

int spTaxon(struct sqlConnection *conn, char *acc);
/* Return taxon of first organism associated with accession. */

int spBinomialToTaxon(struct sqlConnection *conn, char *name);
/* Return taxon associated with binomial (Mus musculus) name. */

int spCommonToTaxon(struct sqlConnection *conn, char *commonName);
/* Return taxon ID associated with common (mouse) name or
 * binomial name. */

char *spTaxonToBinomial(struct sqlConnection *conn, int taxon);
/* Return Latin binomial name associated with taxon. */

char *spTaxonToCommon(struct sqlConnection *conn, int taxon);
/* Given NCBI common ID return first common name associated
 * with it if possible, otherwise return scientific name. */

struct slName *spKeywords(struct sqlConnection *conn, char *acc);
/* Return list of keywords for accession. */

struct slName *spKeywordSearch(struct sqlConnection *conn, char *keyword,
	int taxon);
/* Return list of accessions that use keyword.  If taxon is non-zero
 * then restrict accessions to given organism. */

struct slName *slComments(struct sqlConnection *conn,
	char *acc,	/* Primary accession. */
	char *type);	/* Comment type name, NULL for all comments. */
/* Get list of comments associated with accession. 
 * If optional type parameter is included it should be
 * something in the commentType table.  Some good types
 * are: DISEASE, FUNCTION, "SUBCELLULAR LOCATION" etc. */

struct slName *slCommentTypes(struct sqlConnection *conn);
/* Get list of comment types in database. */

char *spCommentType(struct sqlConnection *conn, int typeId);
/* Look up text associated with typeId. freeMem result when done. */

char *spCommentVal(struct sqlConnection *conn, int valId);
/* Look up text associated with valId. freeMem result when done. */

struct slName *spExtDbAcc1List(struct sqlConnection *conn, char *acc,
	char *db);
/* Get list of accessions from external database associated with this
 * swissProt entity.  The db parameter can be anything in the
 * extDb table.  Some common external databases are 'EMBL' 'PDB' 'Pfam'
 * 'Interpro'. */

struct slName *spEmblAccs(struct sqlConnection *conn, char *acc);
/* Get list of EMBL/Genbank mRNAaccessions associated with this. */

struct slName *spPdbAccs(struct sqlConnection *conn, char *acc);
/* Get list of PDB associations associated with this. */

char *spAccFromEmbl(struct sqlConnection *conn, char *acc);
/* Get SwissProt accession associated with EMBL mRNA. */

struct spFeature *spFeatures(struct sqlConnection *conn, char *acc,
	int classId, 	/* Feature class ID, 0 for all */
	int typeId);	/* Feature type ID, 0 for all */
/* Get feature list.  slFreeList this when done. */

char *spFeatureTypeName(struct sqlConnection *conn, int featureType);
/* Return name associated with featureType */

int spFeatureTypeId(struct sqlConnection *conn, char *name);
/* Return feature type id associated with given name. */

char *spFeatureClassName(struct sqlConnection *conn, int featureClass);
/* Return name associated with featureClass. */

int spFeatureClassId(struct sqlConnection *conn, char *name);
/* Return feature class id associated with given name. */

struct spCitation *spCitations(struct sqlConnection *conn, char *acc);
/* Get list of citations to references associated with this
 * accession.  You can slFreeList this when done. */

char *spRefTitle(struct sqlConnection *conn, int refId);
/* Get title of reference. This can be NULL legitimately. */

struct slName *spRefAuthors(struct sqlConnection *conn, int refId);
/* Get list of authors associated with reference. */

char *spRefCite(struct sqlConnection *conn, int refId);
/* Get journal/page/etc of reference. */

char *spRefPubMed(struct sqlConnection *conn, int refId);
/* Get PubMed id.  May be NULL legitimately. */

struct slName *spRefToAccs(struct sqlConnection *conn, int refId);
/* Get list of accessions associated with reference. */

char *oldSpDisplayId(char *newSpDisplayId);
/* Convert from new Swiss-Prot display ID to old display ID */

char *newSpDisplayId(char *oldSpDisplayId);
/* Convert from old Swiss-Prot display ID to new display ID */

char *uniProtFindPrimAcc(char *id);
/* Return primary accession given an alias. */

char *uniProtFindPrimAccFromGene(char *gene, char *db);
/* Return primary accession given gene name.
 * NULL if not found. */

struct slName *spProteinEvidence(struct sqlConnection *conn, char *acc);
/* Get list of evidence that protein exists for accession.  There will be at least one. */

#endif /* SPDB_H */
