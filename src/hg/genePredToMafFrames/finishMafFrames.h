/* finishMafFrames - link mafFrames objects to deal with spliced codons */
#ifndef FINISHMAFFRAMES_H
#define FINISHMAFFRAMES_H
struct orgGenes;

void finishMafFrames(struct orgGenes *genes);
/* Finish mafFrames build, linking mafFrames prev/next fields */

#endif
