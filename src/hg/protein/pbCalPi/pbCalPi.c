/* pbCalPi - Calculate pI values from a list of protein IDs */
#include "pbCalPi.h"

#ifdef __CYGWIN32__
int main(int argc, char *argv[])
{   
fprintf(stderr, "Sorry, necessary lib function exp10 not supported on cygwin.\n");
return 1;
}
#else

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

float calPi(char *sequence)
/* the calPi() is based on the parameters and algorithm obtained */
/* from SWISS-PROT.  The calculation results should be identical to that */
/* from the Web page: http://us.expasy.org/tools/pi_tool.html */
{
int   i, sequenceLength;
int   comp[ALPHABET_LEN];
int   nTermResidue, cTermResidue;
float charge, phMin, phMid, phMax;
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

    cter = exp10(-cPk[cTermResidue][0]) /
           (exp10(-cPk[cTermResidue][0]) + exp10(-phMid));
    nter = exp10(-phMid) /
           (exp10(-cPk[nTermResidue][1]) + exp10(-phMid));

    carg = comp[R] * exp10(-phMid) /
           (exp10(-cPk[R][2]) + exp10(-phMid));
    chis = comp[H] * exp10(-phMid) /
           (exp10(-cPk[H][2]) + exp10(-phMid));
    clys = comp[K] * exp10(-phMid) /
           (exp10(-cPk[K][2]) + exp10(-phMid));
    casp = comp[D] * exp10(-cPk[D][2]) /
           (exp10(-cPk[D][2]) + exp10(-phMid));
    cglu = comp[E] * exp10(-cPk[E][2]) /
           (exp10(-cPk[E][2]) + exp10(-phMid));
    ccys = comp[C] * exp10(-cPk[C][2]) /
           (exp10(-cPk[C][2]) + exp10(-phMid));
    ctyr = comp[Y] * exp10(-cPk[Y][2]) /
           (exp10(-cPk[Y][2]) + exp10(-phMid));

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
struct sqlConnection *conn;
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
conn  = hAllocConn();

while (lineFileRow(inf, row))
    {
    acc = row[0];

    safef(cond_str, sizeof(cond_str), "acc='%s'", acc);
    seq  = sqlGetField(conn, spDb, "protein", "val", cond_str);

    if (seq != NULL)
	{
    	pI = calPi(seq);
    	fprintf(outf1, "%s\t%.2f\n", acc, pI);
    	}
    }
fclose(outf1);
return(0);
}

#endif /* only works if not __CYGWIN32__ */
