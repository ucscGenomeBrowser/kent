/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "pthreadDoList.h"

int gThreadCount = 5; /* NUmber of threads */


void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen output\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct paraPower
/* Keep track of a number and it's Nth power in parallel */
    {
    struct paraPower *next;
    double in;	/* Input number */
    double out;  /* output number */
    };

void doPowerCalc(void *item, void *context)
/* This routine does the actual work. */
{
struct paraPower *p = item; // Convert item to known type
double *y = context;        // Convert context to a known type
p->out = pow(p->in, *y);    // Calculate and save output back in item.
}

void freen(char *input)
{
/* Make up list of items */
struct paraPower *list = NULL, *el;
int i;
for (i=1; i<=10; ++i)
    {
    AllocVar(el);
    el->in = i;
    slAddHead(&list, el);
    }

/* Do parallel 4th powering in 3 threads */
double context = 4;
pthreadDoList(3, list, doPowerCalc, &context);

/* Report results */
for (el = list; el != NULL; el = el->next)
    printf("%g^%g = %g\n", el->in, context, el->out);
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

