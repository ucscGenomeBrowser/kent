/* mkMafFrames - build mafFrames objects for exons */
#ifndef MKMAFFRAMES_H
#define MKMAFFRAMES_H
struct orgGenes;

void mkMafFramesForMaf(char *targetDb, struct orgGenes *orgs,
                       char *mafFilePath);
/* create mafFrames objects from an MAF file */
#endif
