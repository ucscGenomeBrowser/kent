/* port genome annotations on the fly between assemblies using chain files */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef INSTAPORT_H      
#define INSTAPORT_H      

struct bigBedInterval *instaIntervals(char *instaPortFile, struct bbiFile *bbi,   char *chrom, int start, int end, struct hash **pChainHash);
/* Return intervals from "other" species that will map to the current window.
 * These intervals are NOT YET MAPPED to the current assembly.
 */

struct bed *instaBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb);
/* Using chains stored in chainHash, port a bigBedInterval from another assembly to a bed
 * on the reference.
 */
#endif
