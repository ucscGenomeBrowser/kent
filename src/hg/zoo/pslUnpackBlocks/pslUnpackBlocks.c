#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



#include "common.h"
#include "psl.h"


int main( int argc, char **argv )
{
    int i,j;
    unsigned int blockNum;
    unsigned int size;

    unsigned int qStartOffset, tStartOffset;
    unsigned int ts, qs;
    struct psl *p;

    if( argc != 2 )
    {
        printf( "Usage: pslUnpackBlocks input.psl\n" );
        printf("\nExtracts blocks of (tStart, tEnd, qStart, qEnd) out of a psl file,
                converting to positive strand coordinates.\n");
        printf("Output is given in biologist coordinates (starting at 1, start/stop included).\n");
        exit(1);
    }

    p = pslLoadAll(argv[1]);
    while( p )
    {
        for( i=0; i<p->blockCount; i++ )
        {
            size = p->blockSizes[i];

            if( p->strand[1] == '-' )
                ts = p->tSize - p->tStarts[i];
            else
                ts = p->tStarts[i];

            if( p->strand[0] == '-' )
                qs = p->qSize - p->qStarts[i];
            else
                qs = p->qStarts[i];

            printf("%d\t%d\t%d\t%d\n", ts + 1, ts + size, qs + 1, qs + size );
        }

        p = p->next;
    }

    return(0);
}

 
