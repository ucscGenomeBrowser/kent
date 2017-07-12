/* peptideAtlasTracks - Peptide identifications from tandem MS mapped to the genome,
 *                      from PeptideAtlas (peptideatlas.org) */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"

struct dnaBuf
/* Hold DNA sequence for translating to peptide */
{
    DNA *dna;
    int chromStart;
    int chromEnd;
};

static DNA *dnaForLinkedFeature(struct linkedFeatures *lf, struct dnaBuf *dnaBuf)
/* Return DNA string for all simple features in a linked feature using buffer of
 * genomic sequence in the browser window */
{
struct dyString *dna = dyStringNew(0);
struct simpleFeature *sf;

// NOTE: odd this is needed
slReverse(&lf->components);
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    dyStringAppendN(dna, dnaBuf->dna + sf->start - dnaBuf->chromStart, sf->end - sf->start);
    }
slReverse(&lf->components);
if (lf->orientation == -1)
    {
    reverseComplement(dna->string, strlen(dna->string));
    }
return dyStringCannibalize(&dna);
}

void linkedFeaturesRange(struct track *tg, int *minStart, int *maxEnd)
/* Return minimum chromStart and maximum chromEnd for linked features in the track */
{
if (minStart == NULL || maxEnd == NULL)
    errAbort("Internal error: peptideAtlas feature comparison (hide this track)");
*minStart = hChromSize(database, chromName);
*maxEnd = 0;
struct linkedFeatures *lf;
for (lf = (struct linkedFeatures *)tg->items; lf != NULL; lf = lf->next)
    {
    *minStart = min(*minStart, lf->start);
    *maxEnd = max(*maxEnd, lf->end);
    }
}

static char *peptideAtlasItemName(struct track *tg, void *item)
/* Display peptide sequence translated from genome as item name.
 * This suffices since all PeptideAtlas peptide mappings (in Aug 2014 human build) 
 * are perfect matches to genome */
{
struct dnaSeq *dnaSeq;
if (tg->customPt == NULL)
    {
    int minStart, maxEnd;
    linkedFeaturesRange(tg, &minStart, &maxEnd);
    struct dnaBuf *dnaBuf;
    AllocVar(dnaBuf);
    dnaSeq = hDnaFromSeq(database, chromName, minStart, maxEnd, dnaLower);
    dnaBuf->dna = dnaSeq->dna;
    dnaBuf->chromStart = minStart;
    dnaBuf->chromEnd = maxEnd;
    tg->customPt = dnaBuf;
    }
struct linkedFeatures *lf = (struct linkedFeatures *)item;
DNA *dna = dnaForLinkedFeature(lf, (struct dnaBuf *)tg->customPt);

/* Too bad this lib function fails here, so a bit more code needed */
//AA *peptide = needMem(size);
//dnaTranslateSome(dna, peptide, size);

AllocVar(dnaSeq);
dnaSeq->dna = dna;
dnaSeq->size = (int)strlen(dna);
dnaSeq->name = "";
aaSeq *peptide = translateSeqN(dnaSeq, 0, 0, FALSE);
return peptide->dna;
}

void peptideAtlasMethods(struct track *tg)
/* Item label is peptide sequence translated from genome */
{
tg->itemName = peptideAtlasItemName;
}



