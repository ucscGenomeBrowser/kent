/* mkMafFrames - build mafFrames objects for exons */
#ifndef MKMAFFRAMES_H
#define MKMAFFRAMES_H
struct geneBins;

void mkMafFramesForMaf(char *geneDb, char *targetDb, struct geneBins *genes,
                       char *mafFilePath);
/* create mafFrames objects from an MAF file */
#endif
