/* pbCalPi - Calculate pI values from a list of protein IDs */
#include "pbCalPi.h"
#include "math.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalPi - Calculate pI values from a list of protein IDs \n"
  "usage:\n"
  "   pbCalPi listFile spDb outFile\n"
  "      listFile is the input  file name of list of protein accession numbers\n"
  "      spDb     is the swissprot database name\n"
  "      outFile  is the output file name of tab delineated file of protein accession number and pI value\n"
  "example: pbCalPi prot.lis sp040115 pepPi.tab\n");
}

float myExp10(float x)
{
return(powf(10.0, x));
}

float calPi(char *sequence)
/* the calPi() is based on the parameters and algorithm obtained */
/* from SWISS-PROT.  The calculation results should be identical to that */
/* from the Web page: http://us.expasy.org/tools/pi_tool.html */
{
int   i, sequenceLength;
int   comp[ALPHABET_LEN];
int   nTermResidue, cTermResidue;
float charge, phMin, phMid = 0.0, phMax;
float carg, cter, nter, ctyr, chis, clys, casp, cglu, ccys;
int    R, H, K, D, E, C, Y;

R = (int)('R' - 'A');
H = (int)('H' - 'A');
K = (int)('K' - 'A');
D = (int)('D' - 'A');
E = (int)('E' - 'A');
C = (int)('C' - 'A');
Y = (int)('Y' - 'A');

sequenceLength = strlen(sequence);
for (i=0; i<ALPHABET_LEN; i++) comp[i] = 0.0;
      
/* Compute the amino-acid composition. */
for (i = 0; i < sequenceLength; i++)
    comp[sequence[i] - 'A']++;

/* Look up N-terminal and C-terminal residue. */
nTermResidue = sequence[0] - 'A';
cTermResidue = sequence[sequenceLength - 1] - 'A';

phMin = PH_MIN;
phMax = PH_MAX;

for (i = 0, charge = 1.0; i < MAXLOOP && (phMax - phMin) > EPSI; i++)
    {
    phMid = phMin + (phMax - phMin) / 2.0;

    cter = myExp10(-cPk[cTermResidue][0]) /
           (myExp10(-cPk[cTermResidue][0]) + myExp10(-phMid));
    nter = myExp10(-phMid) /
           (myExp10(-cPk[nTermResidue][1]) + myExp10(-phMid));

    carg = comp[R] * myExp10(-phMid) /
           (myExp10(-cPk[R][2]) + myExp10(-phMid));
    chis = comp[H] * myExp10(-phMid) /
           (myExp10(-cPk[H][2]) + myExp10(-phMid));
    clys = comp[K] * myExp10(-phMid) /
           (myExp10(-cPk[K][2]) + myExp10(-phMid));
    casp = comp[D] * myExp10(-cPk[D][2]) /
           (myExp10(-cPk[D][2]) + myExp10(-phMid));
    cglu = comp[E] * myExp10(-cPk[E][2]) /
           (myExp10(-cPk[E][2]) + myExp10(-phMid));
    ccys = comp[C] * myExp10(-cPk[C][2]) /
           (myExp10(-cPk[C][2]) + myExp10(-phMid));
    ctyr = comp[Y] * myExp10(-cPk[Y][2]) /
           (myExp10(-cPk[Y][2]) + myExp10(-phMid));

    charge = carg + clys + chis + nter -
             (casp + cglu + ctyr + ccys + cter);

    if (charge > 0.0)
	phMin = phMid;
    else
  	phMax = phMid;
    }
return(phMid);
}

int main(int argc, char *argv[])
{
char *infName, *spDb, *outfName;
float pI;
char *acc;
char *seq;
char cond_str[255];
struct lineFile *inf;
FILE *outf1;
char *row[1];

if (argc != 4)
   {
   usage();
   }

infName   = argv[1];
spDb      = argv[2];
outfName  = argv[3];

inf = lineFileOpen(infName, TRUE); /* input file */
outf1 = mustOpen(outfName, "w");

while (lineFileRow(inf, row))
    {
    acc = row[0];

    safef(cond_str, sizeof(cond_str), "acc='%s'", acc);
    seq  = sqlGetField(spDb, "protein", "val", cond_str);

    if (seq != NULL)
	{
    	pI = calPi(seq);
    	fprintf(outf1, "%s\t%.2f\n", acc, pI);
    	}
    }
fclose(outf1);
return(0);
}
