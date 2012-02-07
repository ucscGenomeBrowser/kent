/* read_malign.h - shared procedures for handling textual multiple alignments */

#include "util.h"
#include "read_malign.h"


int *position_in_seq1(char **A, int ncol)
{
int k, j, *col2pos = ckalloc(ncol*sizeof(int));

for (k = j = 0; j < ncol; ++j)
    col2pos[j] = (isalpha(A[0][j]) ? ++k : 0);
return col2pos;
}

int **read_sites(char *filename, int *nrow_ptr, int *ncol_ptr,
		 char ***seqname_ptr, int **sites)
{
int nrow, ncol, i, j, c, k,**A;
char  **seqname, buf[500], *s,*token;
FILE *fp = ckopen(filename, "r");

if (fgets(buf, 500, fp) == NULL ||
    sscanf(buf, "%d %d", &nrow, &ncol) != 2)
    fatal("Cannot determine numbers of rows and columns.");

seqname = ckalloc(nrow * sizeof(char *));

for (i = 0; i < nrow; ++i) 
    {
    if (fgets(buf, 500, fp) == NULL)
	fatal("Cannot determine sequence names.");
    if ((s = strchr(buf, ' ')) != NULL)
	*s = '\0';
    
    seqname[i] = copy_string(buf);
    }


(*sites) =ckalloc((ncol) * sizeof(int));
A =ckalloc((nrow) * sizeof(int *));
A[0] = ckalloc((nrow) * ncol * sizeof(int));

for (i=0 ; i<nrow; ++i)
    A[i] = ckalloc(ncol * sizeof(int));

/* Initialize Sites */

for (i=0;i<nrow;i++)
    for (j=0;j<ncol;j++)
	A[i][j]=0;

/* Read in all the sites into sites... */
for(i=0 ; i < ncol; )
    {
    if ((fgets(buf, 500, fp) == NULL) )
	{
	if(i!= 0)
	    {
	    fatal("Can't read sites.");
	    }
	}
    else
	{
	token = strtok(buf, " ");
	if(token[0] != '\n')
	    {
	    // Read species at given site...
	    (*sites)[i++]=atoi(token);
	    token = strtok(NULL, " ");
	    while(token[0] != '\n')
		{
		A[atoi(token)][i-1]=1;
		token = strtok(NULL, " ");
		}
	    }
	}
    }

*nrow_ptr = nrow;
*ncol_ptr = ncol;
*seqname_ptr = seqname;
return A;
}

char **read_malign(char *filename, int *nrow_ptr, int *ncol_ptr,
  char ***seqname_ptr)
{
	int nrow, ncol, i, j, c;
	char **A, **seqname, buf[500], *s;
	FILE *fp = ckopen(filename, "r");

	if (fgets(buf, 500, fp) == NULL ||
	    sscanf(buf, "%d %d", &nrow, &ncol) != 2)
		fatal("Cannot determine numbers of rows and columns.");
	seqname = ckalloc(nrow * sizeof(char *));
	for (i = 0; i < nrow; ++i) {
		if (fgets(buf, 500, fp) == NULL)
			fatal("Cannot determine sequence names.");
		if ((s = strchr(buf, '\n')) != NULL)
			*s = '\0';
		seqname[i] = copy_string(buf);
	}
	A = ckalloc((nrow) * sizeof(char *));
	A[0] = ckalloc((nrow) * ncol * sizeof(char));
	for (i = 1; i < nrow; ++i)
		A[i] = A[i-1] + ncol*sizeof(char);
	for (i = 0; i < nrow; ++i)
		for (j = 0; j < ncol; ) {
			if ((c = fgetc(fp)) == EOF)
				fatal("Alignment file is incomplete.");
			if (isalpha(c) || c == '-')
				A[i][j++] = c;
			else if (c == '\n') {
				if (j != 0)
					fatal("Corrupted alignment file.");
			} else
				fatalf("Improper character '%c' in alignment.",
				  c);
		}
	while ((c = fgetc(fp)) != EOF)
		if (strchr("ACGT-", c))
			fatal("Too many letters in the alignment file.");
	*nrow_ptr = nrow;
	*ncol_ptr = ncol;
	*seqname_ptr = seqname;
	return A;
}





