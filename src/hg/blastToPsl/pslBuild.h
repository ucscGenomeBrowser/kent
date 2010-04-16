/* object using in building a PSL from a blast record */
#ifndef pslBuild_h
#define pslBuild_h

enum
/* flags for build, including blast algorithm */
{
    blastn       = 0x001,   // blast algorithms
    blastp       = 0x002,
    blastx       = 0x004,
    tblastn      = 0x010,
    tblastx      = 0x020,
    psiblast     = 0x040,
    bldPslx      = 0x080,   // construct a PSLx with sequence
    cnvNucCoords = 0x100,
};

/* score file header */
extern char *pslBuildScoreHdr;

unsigned pslBuildGetBlastAlgo(char *program);
/* determine blast algorithm flags */

struct psl *pslBuildFromHsp(char *qName, int qSize, int qStart, int qEnd, char qStrand, char *qAln,
                            char *tName, int tSize, int tStart, int tEnd, char tStrand, char *tAln,
                            unsigned flags);
/* construct a new psl from an HSP.  Chaining is left to other programs. */

void pslBuildWriteScores(FILE* scoreFh, struct psl *psl, double bitScore, double eValue);
/* write scores for a PSL */

#endif

