/* pslTransMap - transitive mapping of an alignment another sequence, via a
 * common alignment */
#ifndef PSLTRANSMAP_H
#define PSLTRANSMAP_H

enum pslType
/* type of PSL needed protein coordinate conversion */
{
    // n.b. order must match pslTypeDesc
    pslTypeUnspecified = 0,  /* not specified */
    pslTypeProtProt    = 1,  /* protein to protein */
    pslTypeProtNa      = 2,  /* protein to nucleic acid */
    pslTypeNaNa        = 3   /* NA to NA */
};

enum pslTransMapOpts
/* option set for pslTransMap */
{
    pslTransMapNoOpts        = 0x00,   /* no options */
    pslTransMapKeepTrans     = 0x01    /* keep translated alignment strand */
};

struct psl* pslTransMap(unsigned opts, struct psl *inPsl, enum pslType inPslType,
                        struct psl *mapPsl, enum pslType mapPslType);
/* map a psl via a mapping psl, a single psl is returned, or NULL if it
 * couldn't be mapped. */

void pslProtToNAConvert(struct psl *psl);
/* convert a protein/NA or protein/protein alignment to a NA/NA alignment */

#endif
