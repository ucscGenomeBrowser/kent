/* pbCalPi - Calculate pI values from a list of protein IDs */
#include "common.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalPi - Calculate pI values from a list of protein IDs \n"
  "usage:\n"
  "   pbCalPi listFile protDb outFile\n"
  "      listFile is the input  file name of list of protein accession numbers\n"
  "      spDb     is the swissprot database name\n"
  "      outFile  is the output file name of tab delineated file of protein accession number and pI value\n"
  "example: pbCalPi prot.lis sp040115 pepPi.tab\n");
}

// the calPi() is based on the parameters and algorithm obtained
// from SWISS-PROT.  The calculation results should be identical to that
// from the Web page: http://us.expasy.org/tools/pi_tool.html

#define PH_MIN 0 /* minimum pH value */
#define PH_MAX 14 /* maximum pH value */
#define MAXLOOP 2000 /* maximum number of iterations */
#define EPSI 0.0001 /* desired precision */

float calPi(char *sequence)
{
// Table of pk values :
//  Note: the current algorithm does not use the last two columns. Each
//  row corresponds to an amino acid starting with Ala. J, O and U are
//  inexistant, but here only in order to have the complete alphabet.
//
//     Ct    Nt   Sm     Sc     Sn
//

static float cPk[26][5] = {
3.55, 7.59, 0.   , 0.   , 0.    , // A
3.55, 7.50, 0.   , 0.   , 0.    , // B
3.55, 7.50, 9.00 , 9.00 , 9.00  , // C
4.55, 7.50, 4.05 , 4.05 , 4.05  , // D
4.75, 7.70, 4.45 , 4.45 , 4.45  , // E
3.55, 7.50, 0.   , 0.   , 0.    , // F
3.55, 7.50, 0.   , 0.   , 0.    , // G
3.55, 7.50, 5.98 , 5.98 , 5.98  , // H
3.55, 7.50, 0.   , 0.   , 0.    , // I
0.00, 0.00, 0.   , 0.   , 0.    , // J
3.55, 7.50, 10.00, 10.00, 10.00 , // K
3.55, 7.50, 0.   , 0.   , 0.    , // L
3.55, 7.00, 0.   , 0.   , 0.    , // M
3.55, 7.50, 0.   , 0.   , 0.    , // N
0.00, 0.00, 0.   , 0.   , 0.    , // O
3.55, 8.36, 0.   , 0.   , 0.    , // P
3.55, 7.50, 0.   , 0.   , 0.    , // Q
3.55, 7.50, 12.0 , 12.0 , 12.0  , // R
3.55, 6.93, 0.   , 0.   , 0.    , // S
3.55, 6.82, 0.   , 0.   , 0.    , // T
0.00, 0.00, 0.   , 0.   , 0.    , // U
3.55, 7.44, 0.   , 0.   , 0.    , // V
3.55, 7.50, 0.   , 0.   , 0.    , // W
3.55, 7.50, 0.   , 0.   , 0.    , // X
3.55, 7.50, 10.00, 10.00, 10.00 , // Y
3.55, 7.50, 0.   , 0.   , 0.    }; // Z

int   i, sequenceLength;
int   comp[26];
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
for (i=0; i<26; i++) comp[i] = 0.0;
      
// Compute the amino-acid composition.
for (i = 0; i < sequenceLength; i++)
    comp[sequence[i] - 'A']++;

// Look up N-terminal and C-terminal residue.
nTermResidue = sequence[0] - 'A';
cTermResidue = sequence[sequenceLength - 1] - 'A';

phMin = PH_MIN;
phMax = PH_MAX;

for (i = 0, charge = 1.0; i < MAXLOOP && (phMax - phMin) > EPSI; i++)
    {
    phMid = phMin + (phMax - phMin) / 2;

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
	{	     
	phMin = phMid;
        }
     else
	{   
  	phMax = phMid;
        }
    }
return(phMid);
}

int main(int argc, char *argv[])
{
char *inLine;
char *infName, *spDb, *outfName;
int  i;
float pI;
char *acc;
struct sqlConnection *conn;
char *seq;
char cond_str[255];
char line[1000];
FILE *inf, *outf1;

if (argc != 4)
   {
   usage();
   }

infName   = argv[1];
spDb      = argv[2];
outfName  = argv[3];

if ((inf = fopen(infName, "r")) == NULL)
    {		
    fprintf(stderr, "Can't open file %s.\n", infName);
    exit(8);
    }

outf1 = mustOpen(outfName, "w");
conn  = hAllocConn();

while (1)
    {
    inLine = fgets(line, 1000, inf);
    if (inLine == NULL)break;
    *(inLine+strlen(inLine)-1) = '\0';

    acc = inLine;

    sprintf(cond_str, "acc='%s'", acc);
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

