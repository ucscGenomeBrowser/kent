/* randMod.c - Create a list of random numbers of a certain
 * size. */
#include "common.h"


void initRandom()
/* Initialize random number generator */
{
/* Set up random number generator. */
srand( (unsigned)time( NULL ) );
}

int rangedRandom(int min, int max)
/* Return a random number between min and max (ends inclusive)
 * ie 1 6 for a dice simulation. */
{
int size = max-min+1;
return rand()%size + min;
}

void usage()
{
fprintf(stderr, 
    "randMod - generate a list of random numbers\n"
    "usage:\n"
    "    randMod min max [count]\n"
    "Generates a list of count (by default 1) random\n"
    "numbers between min and max.\n"
    "example:\n"
    "    randMod 1 6 2\n"
    "would simulate the roll of two six-sided dice.\n");
exit(-1);
}

int getInt(char *s)
/* Return int value of string s.  Print usage and abort if
 * it's not an int. */
{
if (s == NULL || !isdigit(*s) )
    usage();
return atoi(s);
}

int main(int argc, char *argv[])
{
int min, max, count = 1;
int i;

if (argc != 3 && argc != 4)
    usage();
min = getInt(argv[1]);
max = getInt(argv[2]);
if (min >= max)
    usage();
if (argc == 4)
    count = getInt(argv[3]);

initRandom();
for (i=0; i<count; ++i)
    printf("%d ", rangedRandom(min, max));
printf("\n"); 
return 0;
}