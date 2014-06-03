/* bioImage.h - Interface to bioImage database. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BIOIMAGE_H
#define BIOIMAGE_H

char *bioImageFullSizePath(struct sqlConnection *conn, int id);
/* Fill in path for full image bioImage of given id.  FreeMem
 * this when done */

char *bioImageScreenSizePath(struct sqlConnection *conn, int id);
/* Fill in path for screen-sized (~700x700) image bioImage of given id. 
 * FreeMem when done. */

char *bioImageThumbSizePath(struct sqlConnection *conn, int id);
/* Fill in path for thumbnail image (~200x200) bioImage of given id. 
 * FreeMem when done. */

char *bioImageOrganism(struct sqlConnection *conn, int id);
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */

char *bioImageStage(struct sqlConnection *conn, int id, boolean doLong);
/* Return string describing growth stage of organism 
 * FreeMem this when done. */

char *bioImageGeneName(struct sqlConnection *conn, int id);
/* Return gene name  - HUGO if possible, RefSeq/GenBank, etc if not. 
 * FreeMem this when done. */

char *bioImageAccession(struct sqlConnection *conn, int id);
/* Return RefSeq/Genbank accession or n/a if none available. 
 * FreeMem this when done. */

char *bioImageSubmitId(struct sqlConnection *conn, int id);
/* Return submitId  for image.  (Internal id for data set)
 * FreeMem this when done. */

char *bioImageType(struct sqlConnection *conn, int id);
/* Return RefSeq/Genbank accession or NULL if none available. 
 * FreeMem this when done. */

char *bioImageBodyPart(struct sqlConnection *conn, int id);
/* Return body part if any.  FreeMem this when done. */

char *bioImageSliceType(struct sqlConnection *conn, int id);
/* Return slice type if any.  FreeMem this when done. */

char *bioImageTreatment(struct sqlConnection *conn, int id);
/* Return treatment if any.  FreeMem this when done.*/

char *bioImageContributors(struct sqlConnection *conn, int id);
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. 
 * FreeMem this when done. */

char *bioImagePublication(struct sqlConnection *conn, int id);
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */

char *bioImagePubUrl(struct sqlConnection *conn, int id);
/* Return url of publication associated with image if any.
 * FreeMem this when done. */

char *bioImageSetUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */

char *bioImageItemUrl(struct sqlConnection *conn, int id);
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */

struct slInt *bioImageOnSameGene(struct sqlConnection *conn, int id);
/* Get bio images that are on same gene.  Do slFreeList when done. */

#endif /* BIOIMAGE_H */

