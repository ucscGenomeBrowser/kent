#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



#include "common.h"
#include "linefile.h"
#include "nib.h"
#include "jksql.h"
#include "coordConv.h"
#include "fa.h"
#include "genoFind.h"
#include "psl.h"
#include "portable.h"
#include "hdb.h"
#include "dbDb.h"



#ifndef DNASEQ_H
    #include "dnaseq.h"
#endif

#ifndef DNAUTIL_H
    #include "dnautil.h"
#endif

#ifndef NIB_H
    #include "nib.h"
#endif





unsigned int flipNegStrand( char strand, unsigned int start, unsigned int size )
{
   if( strand == '-' )
       return( size - start );
   else
       return( start );
}

void printFa( unsigned char *FA, unsigned int maxFa )
{
    unsigned int i;
    for( i=0; i<maxFa; i++ )
        printf("%c", FA[i] );
    printf("\n");
}


void initFa( unsigned char *FA, unsigned int maxFa )
{
    unsigned int i;
    for( i=0; i<maxFa; i++ )
        FA[i] = '-';
}

void writeFa( unsigned char *FA, unsigned int maxFa, unsigned int theStart, unsigned int size, 
        char *qSeq )
{
    unsigned int i;


    if( theStart+size-1 >= maxFa || theStart < 0 )
    {
        fprintf( stderr, "maxFa(%d) is too short for [%d,%d]\n", 
                maxFa, theStart, theStart+size-1);
        exit(1);
    }
    
    for( i=theStart; i<theStart+size; i++ )
            FA[i] = qSeq[i-theStart];
}


int main( int argc, char **argv )
{
    unsigned int maxFa;
    unsigned char *FA;
    unsigned int i;
    unsigned int blockNum;
    unsigned int size;

    unsigned int ts, qs;
    
    
    struct dnaSeq *seq1;
    struct dnaSeq *seq2;
    struct dnaSeq *tSeq;
    struct dnaSeq *qSeq;

    struct dnaSeq *tSeqAll;

    struct psl *p;


    if( argc != 4 && argc != 5 )
    {
        printf( "Usage: pslToFa input.psl master.nib slave.nib [maxFa]\n" );
        exit(1);
    }

    tSeqAll = nibLoadAll( argv[2] );

    if( argc == 5 )
        maxFa = atoi(argv[5]);
    else
        maxFa = strlen(tSeqAll->dna);    //1877426 + 1 for zoo2

    FA = malloc( sizeof( unsigned char ) * ( maxFa + 1 ) );
    initFa(FA, maxFa);


    p = pslLoadAll(argv[1]);
    while( p )
    {
        qSeq = nibLoadPart( argv[3], p->qStart, p->qEnd - p->qStart );
        tSeq = nibLoadPart( argv[2], p->tStart, p->tEnd - p->tStart );

        for( i=0; i<p->blockCount; i++ )
        {
            size = p->blockSizes[i];
            qs = flipNegStrand( p->strand[0], p->qStarts[i], p->qSize);
            ts = flipNegStrand( p->strand[1], p->tStarts[i], p->tSize);

            seq1 = nibLoadPart( argv[2], ts, size );   /*master*/
            seq2 = nibLoadPart( argv[3], qs, size );   /*slave*/

            writeFa( FA, maxFa, ts, size, seq2->dna );
        }

        p = p->next;
    }

    if( strlen(tSeqAll->dna) != maxFa )
    {
        printf("Sequence sizes must match!!! (strlen(tSeqAll->dna) = %d, maxFa = %d)\n",  
                strlen(tSeqAll->dna), maxFa );
    }
    printFa(tSeqAll->dna, maxFa );
    printFa(FA, maxFa );
    return(0);
}

 
