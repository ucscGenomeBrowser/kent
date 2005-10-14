/* visiSearch - free form search of visiGene database. */

#ifndef VISISEARCH_H
#define VISISEARCH_H

struct visiMatch
/* Info on a score of an image in free format search. */
    {
    struct visiMatch *next;
    int imageId;	/* Image ID associated with search. */
    double weight;	/* The higher the weight the better the match */
    };

struct visiMatch *visiSearch(struct sqlConnection *conn, char *searchString);
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */

#endif /* VISISEARCH_H */
