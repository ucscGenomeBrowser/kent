/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "hex.h"
#include "pipeline.h"
#include "obscure.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen output\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *output)
/* Test something */
{
int i,j,k,l;
FILE *f = mustOpen(output, "w");

for (i=0; i<20; ++i)
    {
    fprintf(f, "meta experiment_%03d\n", i);
    fprintf(f, "lab lab_%d\n", i%3 + 1);
    fprintf(f, "assay %s\n", (i%1 == 0) ? "RNA-seq" : "WGBS");
    fprintf(f, "access all\n");
    fprintf(f, "organ brain\n");
    fprintf(f, "\n");
    for (j=0; j<20; ++j)
        {
	fprintf(f, "    donor %03X\n", j);
	fprintf(f, "    age %d\n", 5 + j%10);
	fprintf(f, "    age_units weeks\n");
	fprintf(f, "    life_stage embryo\n");
	fprintf(f, "    sex %s\n", (j%3 == 0) ? "male" : "female");
	fprintf(f, "    biosample_date 2015-01-%02d\n", j); 
	fprintf(f, "\n");
	for (k=0; k<20; ++k)
	    {
	    fprintf(f, "        organ %s\n", (k%3 == 0) ? "brain" : "liver");
	    fprintf(f, "        lab_kent_disassociation_protocol %s\n", 
		(k%3 == 0) ? "fetal_brain_digest.pdf" : "fetal_liver_digest.pdf");
	    fprintf(f, "        lab_kent_quality %g\n",  k*k%100 * 0.01);
	    fprintf(f, "\n");
	    for (l=0; l<20; ++l)
	        {
		fprintf(f, "            file ex%02ddo%02dor%02dfa%02d.fq.gz\n", i, j, k, l);
		fprintf(f, "            format fastq\n");
		fprintf(f, "            part %d\n", l+1);
		fprintf(f, "\n");
		}
	    }
	}
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

