/* splitMultiMappings - split genes that mapped to multiple locations or have rearranged
 * exons into separate gene objects. */
#ifndef SPLITMULTIMAPPINGS_H
#define SPLITMULTIMAPPINGS_H

struct orgGenes;

void splitMultiMappings(struct orgGenes *genes);
/* check if genes are mapped to multiple locations, and if so, split them
 * into two or more genes */

#endif

