/* visiGene.h - Interface to visiGene database. 
 * The id that almost every routine uses is the image.id
 * field. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef VISIGENE_H
#define VISIGENE_H

int visiGeneImageFile(struct sqlConnection *conn, int id);
/* Return image file ID associated with image ID.  A file
 * con contain multiple images. */

struct slInt *visiGeneImagesForFile(struct sqlConnection *conn, 
	int imageFile);
/* Given image file ID, return list of all associated image IDs. */

char *visiGenePaneLabel(struct sqlConnection *conn, int id);
/* Return label for pane of this image in file if any, NULL if none.
 * FreeMem this when done. */

boolean visiGeneImageSize(struct sqlConnection *conn, int id, int *imageWidth, int *imageHeight);
/* Get width and height of image with given id. 
 * Return FALSE with warning if not found. */

char *visiGeneFullSizePath(struct sqlConnection *conn, int id);
/* Fill in path for full image visiGene of given id.  FreeMem
 * this when done */

char *visiGeneThumbSizePath(struct sqlConnection *conn, int id);
/* Fill in path for thumbnail image (~200x200) visiGene of given id. 
 * FreeMem when done. */

char *visiGeneOrganism(struct sqlConnection *conn, int id);
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */

char *visiGeneStage(struct sqlConnection *conn, int id, boolean doLong);
/* Return string describing growth stage of organism.  The description
 * will be 5 or 6 characters wide if doLong is false, and about
 * 20 characters wide if it is true. FreeMem this when done. */

struct slName *visiGeneGeneName(struct sqlConnection *conn, int id);
/* Return list of gene names  - HUGO if possible, RefSeq/GenBank, etc if not. 
 * slNameFreeList this when done. */

struct slName *visiGeneRefSeq(struct sqlConnection *conn, int id);
/* Return list of RefSeq accessions or n/a if none available. 
 * slNameFreeList this when done. */

struct slName *visiGeneGenbank(struct sqlConnection *conn, int id);
/* Return list of Genbank accessions or n/a if none available. 
 * slNameFreeList this when done. */

struct slName *visiGeneUniProt(struct sqlConnection *conn, int id);
/* Return list of UniProt accessions or n/a if none available. 
 * slNameFreeList this when done. */

char *visiGeneSubmitId(struct sqlConnection *conn, int id);
/* Return submitId  for image.  (Internal id for data set)
 * FreeMem this when done. */

char *visiGeneBodyPart(struct sqlConnection *conn, int id);
/* Return body part if any.  FreeMem this when done. */

char *visiGeneSex(struct sqlConnection *conn, int id);
/* Return sex if known.  FreeMem this when done. */

char *visiGeneStrain(struct sqlConnection *conn, int id);
/* Return strain of organism if any.  FreeMem this when done. */

struct slName *visiGeneGenotypes(struct sqlConnection *conn, int id);
/* Return list of genotypes.  A genotype is either "wild type"
 * or gene:allele.  slFreeList this when done. */

char *visiGeneSliceType(struct sqlConnection *conn, int id);
/* Return slice type if any.  FreeMem this when done. */

char *visiGeneFixation(struct sqlConnection *conn, int id);
/* Return fixation conditions if any.  FreeMem this when done.*/

char *visiGeneEmbedding(struct sqlConnection *conn, int id);
/* Return fixation conditions if any.  FreeMem this when done.*/

char *visiGenePermeablization(struct sqlConnection *conn, int id);
/* Return permeablization conditions if any.  FreeMem this when done.*/

char *visiGeneCaption(struct sqlConnection *conn, int id);
/* Return free text caption if any. FreeMem this when done. */

char *visiGeneContributors(struct sqlConnection *conn, int id);
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. 
 * FreeMem this when done. */

int visiGeneYear(struct sqlConnection *conn, int id);
/* Return year of publication. */

char *visiGenePublication(struct sqlConnection *conn, int id);
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */

char *visiGenePubUrl(struct sqlConnection *conn, int id);
/* Return url of publication associated with image if any.
 * FreeMem this when done. */

char *visiGeneSetUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */

char *visiGeneItemUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */

char *visiGeneAbUrl(struct sqlConnection *conn, int id);
/* Return contributor antibody url. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */

char *visiGeneAcknowledgement(struct sqlConnection *conn, int id);
/* Return acknowledgement if any, NULL if none. 
 * FreeMem this when done. */

char *visiGeneSubmissionSource(struct sqlConnection *conn, int id);
/* Return name of submission source. */

char *visiGeneCopyright(struct sqlConnection *conn, int id);
/* Return copyright statement if any,  NULL if none.
 * FreeMem this when done. */

enum visiGeneSearchType 
    {
    vgsNone = 0,
    vgsExact = 1,
    vgsPrefix = 2,
    vgsLike = 3,
    };
   
struct slInt *visiGeneSelectNamed(struct sqlConnection *conn,
	char *name, enum visiGeneSearchType how);
/* Return ids of images that have probes involving gene that match name. */

struct slInt *visiGeneSelectRefSeq(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving refSeq mRNA seq. */

struct slInt *visiGeneSelectLocusLink(struct sqlConnection *conn, char *id);
/* Return ids of images that have probes involving locusLink (entrez gene)
 * id. */

struct slInt *visiGeneSelectGenbank(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving genbank accessioned 
 * sequence */

struct slInt *visiGeneSelectUniProt(struct sqlConnection *conn, char *acc);
/* Return ids of images that have probes involving UniProt accessioned
 * sequence. */

char *vgGeneNameFromId(struct sqlConnection *conn, int geneId);
/* Return gene name associated with gene ID  - HUGO if possible, 
 * RefSeq/GenBank, etc if not. You can freeMem this when done.  */

#endif /* VISIGENE_H */
