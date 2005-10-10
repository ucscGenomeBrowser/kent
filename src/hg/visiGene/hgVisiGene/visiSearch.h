/* visiSearch - free form search of visiGene database. */

#ifndef VISISEARCH_H
#define VISISEARCH_H

struct slInt *visiSearch(struct sqlConnection *conn, char *searchString);
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */

#endif /* VISISEARCH_H */
