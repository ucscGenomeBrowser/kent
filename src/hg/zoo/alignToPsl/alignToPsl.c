/* This file creates a psl file from a pairwise file */
/* Copyright 2002 Trevor Bruen */
/* This file reads a DNA alignment of two sequences and outputs a psl. */
/* It includes N count for matching bases but does not currently do anything */
/* for repmatches... */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"


#define BLOCKMAX 1000
#define MATCH 1
#define TDELETE 2
#define QDELETE 3
#define QUERY 0
#define TARGET 1

/* Variables that are set from the command line possibly*/

int query_length = 0;
int target_length = 0;
int offset1 = 0;
int offset2 = 0;
char *in_fname = NULL;
char *out_fname = NULL;
char *query_name = NULL;
char *target_name = NULL;

/* Prints out proper invocation and exits */
void print_usage();

/* Finds size of sequences and of alignment */
int find_size(FILE *in_file);

/* Processes command line inputs */
void process_inputs(int argc, char ***argv);

int main( int argc, char **argv)
{
int target_count = 0,query_count = 0,match = 0,mismatch = 0,qgapcount = 0,qgapbases = 0;
int tgapcount = 0, state;
int tgapbases = 0,bcount = 0,bsize = 0, repmatches = 0, ncount = 0;
int query_starts[BLOCKMAX],target_starts[BLOCKMAX],bsizes[BLOCKMAX];
int Master_Count=0;
int ncolumns,nrows,i,j,k;
FILE *output, *input;
char **seqs,c,qr,tr;

process_inputs(argc,&argv);

if((input = fopen(in_fname,"r")) == NULL)
    print_usage();
if((output = fopen(out_fname,"w")) == NULL)
    print_usage();

ncolumns = find_size(input);

/* Allocate for proper size */
seqs = safe_alloc(2 * sizeof(char *));

/* Allocate for proper size */
for(i=0;i<2;i++)
    seqs[i] = safe_alloc(ncolumns * sizeof(char));


/* Read alignments */
fclose(input);
if((input = fopen(in_fname,"r")) == NULL)
    print_usage();

for(i=0;i<2;i++)
    {
    for(j=0;j<ncolumns;j++)
	{
	seqs[i][j]=fgetc(input);
	}
    /* Get newline */
    fgetc(input);
    }

query_count = offset1;
target_count = offset2;

state=QDELETE;
    
for(j=0;j<ncolumns;j++)
    {
    if(isalpha(tr=seqs[TARGET][j]))
	{
	target_count++;
	
	if(isalpha(qr=seqs[QUERY][j]))
	    {
	    query_count++;
	    
	    /* Match state */
	    if(state != MATCH)
		{
  
		/* Write out blocks if number of blocks is maximized */
		if(bcount == BLOCKMAX)
		    {
		    fprintf(output, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t+\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t",match,mismatch,ncount,repmatches,qgapcount,qgapbases,tgapcount,tgapbases,query_name,query_length,query_starts[0]-1,query_count-1,target_name,target_length,target_starts[0]-1,target_count,bcount);
		    
		    for(k=1;k<=bcount;k++)
			fprintf(output, "%d,",bsizes[k]);
		    fprintf(output, "\t");
    
		    for(k=0;k<bcount;k++)
			fprintf(output, "%d,",query_starts[k]-1);
		    
		    fprintf(output, "\t");
		    for(k=0;k<bcount;k++)
			fprintf(output, "%d,",target_starts[k]-1);
		    fprintf(output, "\n");
		    
		    mismatch=0;
		    ncount=0;
		    repmatches = 0;
		    match=0;
		    tgapbases=0;
		    tgapcount=0;
		    qgapcount=0;
		    qgapbases=0;
		    bcount=0;		    
		    }
		
		/* Start new list of records */
		query_starts[bcount]=query_count;
		target_starts[bcount]=target_count;
		bcount++;
		}
	    
	    if((toupper(tr) == 'N') || (toupper(qr) == 'N'))
		ncount++;
	    else
		{
		if(tr==qr)
		    match++;
		else
		    mismatch++;
		
		bsize++;
		state=MATCH;
		}
	    }
	
	/* Qdelete */
	else 
	    {
	    /* Check previous state - see if time to write out */
	    if(state==MATCH)
		{
		bsizes[bcount]=bsize;
		bsize=0;
		tgapcount++;
		}
	    
	    else if(state==TDELETE)
		tgapcount++;
	    
	    tgapbases++;
	    state=QDELETE; 
	    }
	}
    /* Check for Tdelete */
    else 
	{
	if (isalpha(qr=seqs[QUERY][j]))
	    {
	    query_count++;
	    
	    /* Check previous state - see if have to write out */
	    if(state!=TDELETE)
		{
		qgapcount++;
		if(state==MATCH)
		    {
		    bsizes[bcount]=bsize;
		    bsize=0;
		    }
		}
	    qgapbases++;
	    state=TDELETE;
	    }
	}
    /* Otherwise a mutual delete - do nothing */
    }

/* Add final info if in match state */
if(state==MATCH)
    bsizes[bcount]=bsize;

/* Write out final record */
fprintf(output, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t+\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\t",match,mismatch,ncount,repmatches,qgapcount,qgapbases,tgapcount,tgapbases,query_name,query_length,query_starts[0]-1,query_count,target_name,target_length,target_starts[0]-1,target_count,bcount);

for(j=1;j<=bcount;j++)
    fprintf(output, "%d,",bsizes[j]);
fprintf(output, "\t");

for(j=0;j<bcount;j++)
    fprintf(output, "%d,",query_starts[j]-1);

fprintf(output, "\t");
for(j=0;j<bcount;j++)
    fprintf(output, "%d,",target_starts[j]-1);
fprintf(output, "\n");
fclose(output);
}


/* Prints out proper invocation and exits */
void print_usage()
{
fprintf(stderr,
"alignToPsl - Convert a straightforward DNA alignment to a PSL.
             File should have Query sequence on the first line 
             and the Target sequence on the second.  Each should
             be terminated by a newline.
             
usage:
  alignToPsl [options] alignmentfile QuerySequenceName TargetSequenceName out.psl
options:
  -lengths # # - add the actual lengths for proper psl creation.
  -offsets # # - add the actual offset for proper psl creation.\n\n");
exit(1);
}

/* Finds lengths of sequences and of alignment */
int find_size(FILE *in_file)
{
char c;
int size=0,max=0,i,alpha_size=0;


/* An ugly loop unrolling */
while((c=fgetc(in_file)) != '\n')
    {
    if(isalpha(c))
	alpha_size++;
    size++;
    }
if(query_length==0)
    query_length=alpha_size;

alpha_size=0;
max=size;
size=0;

while(((c=fgetc(in_file)) != '\n') && (c != EOF))
    {
    if(isalpha(c))
	alpha_size++;
    size++;
    }

if(target_length==0)
    target_length=alpha_size;

if(size>max)
    {
    fprintf(stderr,"Error: Size of both sequences in alignment should match\n");
    exit(1);
    }

max=size;
return max;
}



/* Processes command line inputs */
void process_inputs(int argc, char ***argv)
{
int adder=0;

if(argc == 8)
    adder = 3;
else if(argc == 11)
    adder = 6;
else if(argc !=5)
    print_usage();

in_fname = strClone((*argv)[(1+adder)]);
query_name = strClone((*argv)[(2+adder)]); 
target_name= strClone((*argv)[(3+adder)]);
out_fname = strClone((*argv)[(4+adder)]);

if(argc > 5)
    {
    if(!strcmp((*argv)[1],"-lengths"))
	{
	query_length=atoi((*argv)[2]);
	target_length=atoi((*argv)[3]);
	}
    if(!strcmp((*argv)[1],"-offsets"))
	{
	offset1=atoi((*argv)[2]);
	offset2=atoi((*argv)[3]);
	}
    if(!strcmp((*argv)[4],"-lengths"))
	{
	query_length=atoi((*argv)[5]);
	target_length=atoi((*argv)[6]);
	}
    if(!strcmp((*argv)[4],"-offsets"))
	{
	offset1=atoi((*argv)[5]);
	offset2=atoi((*argv)[6]);
	}
    }
}










