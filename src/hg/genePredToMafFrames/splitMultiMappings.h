/* splitMultiMappings - split genes that mapped to multiple locations or have rearranged
 * exons into separate gene objects. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef SPLITMULTIMAPPINGS_H
#define SPLITMULTIMAPPINGS_H

struct orgGenes;

void splitMultiMappings(struct orgGenes *genes);
/* check if genes are mapped to multiple locations, and if so, split them
 * into two or more genes */

#endif

