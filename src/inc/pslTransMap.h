/* pslTransMap - transitive mapping of an alignment another sequence, via a
 * common alignment */
#ifndef PSLTRANSMAP_H
#define PSLTRANSMAP_H

enum pslTransMapOpts
/* option set for pslTransMap */
{
    pslTransMapNoOpts     = 0x00,  /* no options */
    pslTransMapKeepTrans  = 0x01   /* keep translated alignment strand */
};

struct psl* pslTransMap(unsigned opts, struct psl *inPsl, struct psl *mapPsl);
/* map a psl via a mapping psl, a single psl is returned, or NULL if it
 * couldn't be mapped. */

#endif
