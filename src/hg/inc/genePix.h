/* genePix.h - Interface to genePix database. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GENEPIX_H
#define GENEPIX_H

char *genePixFullSizePath(struct sqlConnection *conn, int id);
/* Fill in path for full image genePix of given id.  FreeMem
 * this when done */

char *genePixScreenSizePath(struct sqlConnection *conn, int id);
/* Fill in path for screen-sized (~700x700) image genePix of given id. 
 * FreeMem when done. */

char *genePixThumbSizePath(struct sqlConnection *conn, int id);
/* Fill in path for thumbnail image (~200x200) genePix of given id. 
 * FreeMem when done. */

char *genePixOrganism(struct sqlConnection *conn, int id);
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */

char *genePixStage(struct sqlConnection *conn, int id, boolean doLong);
/* Return string describing growth stage of organism.  The description
 * will be 5 or 6 characters wide if doLong is false, and about
 * 20 characters wide if it is true. FreeMem this when done. */

struct slName *genePixGeneName(struct sqlConnection *conn, int id);
/* Return list of gene names  - HUGO if possible, RefSeq/GenBank, etc if not. 
 * FreeMem this when done. */

struct slName *genePixRefSeq(struct sqlConnection *conn, int id);
/* Return list of RefSeq accessions or n/a if none available. 
 * FreeMem this when done. */

struct slName *genePixGenbank(struct sqlConnection *conn, int id);
/* Return list of Genbank accessions or n/a if none available. 
 * FreeMem this when done. */

struct slName *genePixUniProt(struct sqlConnection *conn, int id);
/* Return list of UniProt accessions or n/a if none available. 
 * FreeMem this when done. */

char *genePixSubmitId(struct sqlConnection *conn, int id);
/* Return submitId  for image.  (Internal id for data set)
 * FreeMem this when done. */

char *genePixBodyPart(struct sqlConnection *conn, int id);
/* Return body part if any.  FreeMem this when done. */

char *genePixSliceType(struct sqlConnection *conn, int id);
/* Return slice type if any.  FreeMem this when done. */

char *genePixTreatment(struct sqlConnection *conn, int id);
/* Return treatment if any.  FreeMem this when done.*/

char *genePixContributors(struct sqlConnection *conn, int id);
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. 
 * FreeMem this when done. */

char *genePixPublication(struct sqlConnection *conn, int id);
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */

char *genePixPubUrl(struct sqlConnection *conn, int id);
/* Return url of publication associated with image if any.
 * FreeMem this when done. */

char *genePixSetUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */

char *genePixItemUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */

enum genePixSearchType 
    {
    gpsNone = 0,
    gpsExact = 1,
    gpsPrefix = 2,
    gpsLike = 3,
    };
   
struct slInt *genePixSelectNamed(struct sqlConnection *conn,
	char *name, enum genePixSearchType how);
/* Return ids of images that have probes involving gene that match name. */

struct slInt *genePixSelectRefSeq(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving refSeq mRNA seq. */

struct slInt *genePixSelectLocusLink(struct sqlConnection *conn, char *id);
/* Return ids of images that have probes involving locusLink (entrez gene)
 * id. */

struct slInt *genePixSelectGenbank(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving genbank accessioned 
 * sequence */

struct slInt *genePixSelectUniProt(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving UniProt accessioned
 * sequence. */

#define genePixMaxAge 1.0e12

struct slInt *genePixSelectMulti(
        /* Note most parameters accept NULL or 0 to mean ignore parameter. */
	struct sqlConnection *conn, 
	struct slInt *init,	/* Restrict results to those with id in list. */
	int taxon,		/* Taxon of species to restrict it to. */
	struct slName *contributors,  /* List of contributers to restrict it to. */
	enum genePixSearchType how,   /* How to match contributors */
	boolean startIsEmbryo, double startAge,
	boolean endIsEmbryo, double endAge);
/* Return selected genes according to a number of criteria. */

struct slInt *genePixOnSameGene(struct sqlConnection *conn, int id);
/* Get bio images that are on same gene.  Do slFreeList when done. */


#endif /* GENEPIX_H */
