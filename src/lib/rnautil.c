/* rnautil.c - functions for dealing with RNA and RNA secondary structure.  */
#include "rnautil.h"
#include "common.h"

const char *RNA_PAIRS[] = {"AU","UA","GC","CG","GU","UG",0};
/* Null terminated array of rna pairs */

void dna2rna(char *s)
/* Replace 't' with 'u' and 'T' with 'U' in s. */
{
for (;*s;s++)
    {
    if (*s == 't')
	*s = 'u';
    if (*s == 'T')
	*s = 'U';
    }
}

bool rnaPair(char a, char b)
/* Returns TRUE if a and b can pair, and false otherwise */
{
char pair[] = {a,b,'\0'};
int i;
dna2rna(pair);
touppers(pair);

for (i=0;RNA_PAIRS[i] != 0; i++)
    if (pair[0] == RNA_PAIRS[i][0] && pair[1] == RNA_PAIRS[i][1] )
	return TRUE;
return FALSE;
}

void reverseFold(char *s)
/* Reverse the order of the parenthesis defining an RNA secondary structure annotation. */
{
reverseBytes(s, strlen(s));
for (;*s;s++)
    {
    if (*s == '(')
	*s = ')';
    else if (*s == ')')
	*s = '(';
    }
}

void fold2pairingList(char *fold, int len, int **p2pairList)
/* take a parenthesis string, allocate and return an array of pairing
   positions: pairList[i] = j <=> i pair with j and pairList[i] = -1
   <=> i does not pair.*/
{
int i,j, stackSize = 0;
int *pairList      = needMem(len * sizeof(int));
*p2pairList        = pairList;

/* initialize array */
for (i = 0; i < len; i++)
    pairList[i] = -1;

/* fill out pairList */
for (i = 0; i < len; i++) 
    {
    if (fold[i] == '(')
	{
	stackSize = 1;
	for (j = i+1; j < len; j++) 
	    {
	    if (fold[j] == '(')
		stackSize += 1;
	    else if (fold[j] == ')')
		stackSize -= 1;
	    if (stackSize == 0)  /* found pair partner */
		{
		pairList[i] = j;
		pairList[j] = i;
		break;
		}
	    }
	}
    }
}

void mkPairPartnerSymbols(int *pairList, char *pairSymbols, int size)
{
/* Make a symbol string indicating pairing partner */
int i;
char symbols[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*=+{}|[]\\;'"; /* length 80 */
int  symbolMax = strlen(symbols);
int index;
for (i = 0, index = 0; i < size; i++)
    {
    pairSymbols[i] = ' ';
    if (pairList[i] >= 0)
	{
	if (pairList[i] < i)
	    {
            --index;
	    if (index<0)
		index = symbolMax-1;
	    }
	pairSymbols[i] = symbols[index];
	if (pairList[i] > i)
	    {
            ++index;
	    if (index>=symbolMax)
		index=0;
	    }
	}
    }
}

char * projectString(char *s, char *ref, char refChar, char insertChar)
/* Insert 'insertChar' in 's' at every position 'ref' has 'refChar'. */
{
int i,j,size = strlen(ref);
char *copy = (char *) needMem(size + 1);

if (strlen(s) != strlen(ref) - countChars(ref, refChar))
  errAbort("ERROR from rnautil::projectString: Input string 's' has wrong length.\n"); 

for (i = 0, j = 0; i < size; i++)
    {
    if (ref[i] == refChar)
	copy[i] = insertChar;
    else
	{	
	copy[i] = s[j];
	j++;
	}
    }
return copy;
}

char *gapAdjustFold(char *s, char *ref)
/* Insert space in s when there is a gap ('-') in ref. */
{
return projectString(s, ref, '-', ' ');
}


int *projectIntArray(int *in, char *ref, char refChar, int insertInt)
/* Insert 'insertChar' in 's' at every positin 'ref' has 'refChar'. */
{
int i,j,size = strlen(ref);
int   *copy = (int *) needMem(size *sizeof(int) );

for (i = 0, j = 0; i < size; i++)
    {
    if (ref[i] == refChar)
	copy[i] = insertInt;
    else
	{	
	copy[i] = in[j];
	j++;
	}
    }
return copy;
}

int * gapIntArrayAdjust(int *in, char *ref)
/* Insert space in s when there is a gap ('-') in ref. */
{
return projectIntArray(in, ref, '-', 0);
}

void markCompensatoryMutations(char *s, char *ref, int *pairList, int *markList)
/* Compares s to ref and pairList and sets values in markList
 * according to pairing properties. The value of markList[i] specifies
 * the pairing property of the i'th position. The following values are
 * used: 
 * 0: not pairing, no substitution (default) 
 * 1: not pairing, single substitution
 * 2: pairing, no substitutions 
 * 3: pairing, single substitution (one of: CG<->TG, GC<->GT, TA<->TG, AT<->GT)
 * 4: pairing, double substitution (i.e. a compensatory change)
 * 5: annotated as pairing but dinucleotide cannot pair, single substitution
 * 6: annotated as pairing but dinucleotide cannot pair, doubble substitution
 * 7: annotated as pairing but dinucleotide cannot pair, involves indel ('-' substitution)
 */
{
int i, size = strlen(s);
for (i = 0; i < size; i++)
    {
    if (pairList[i] == -1)
	if (toupper(s[i]) != toupper(ref[i]) && s[i] != '.' && s[i] != '-' && ref[i] != '-')
	    markList[i] = 1;
	else
	    markList[i] = 0;
    else
	{
	if (s[i] == '.' || s[pairList[i]] == '.') /* treat missing data as possible pair partner */
	    markList[i] = 2;
	else if (!rnaPair(s[i], s[pairList[i]]))
	  if (s[i] == '-' || s[pairList[i]] == '-')
	    markList[i] = 7;
	  else if (toupper( s[i] ) != toupper( ref[i] ) && toupper( s[pairList[i]] ) != toupper( ref[pairList[i]] ) )
	    markList[i] = 6;
	  else
	    markList[i] = 5;
	else if (toupper( s[i] ) != toupper( ref[i] ) && toupper( s[pairList[i]] ) != toupper( ref[pairList[i]] ) )
	    markList[i] = 4;
	else if (toupper( s[i] ) != toupper( ref[i] ) || toupper( s[pairList[i]] ) != toupper( ref[pairList[i]] ) )
	    markList[i] = 3;
	else
	    markList[i] = 2;
	}
    }
}

int assignBin(double val, double minVal, double maxVal, int binCount)
/* Divide range given by minVal and maxVal into binCount intervals
   (bins), and return index of the bin val falls into. */
{
double range = maxVal - minVal;
int maxBin   = binCount - 1;
int level    = (int) ( (val-minVal)*maxBin/range);
if (level <= 0) level = 0;
if (level > maxBin) level = maxBin;
return level;
}
