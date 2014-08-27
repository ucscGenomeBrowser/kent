/* pthreadDoList - do something to all items in list using multiple threads in parallel. */

#ifndef PTHREADDOLIST_H
#define PTHREADDOLIST_H

/* The general usage of this routine is to make up a singly-linked list of items 
 * with the convention that the first field of the item is the next pointer.
 * Then make a routine that takes a pointer to an item, and optionally a second
 * pointer to a context for data that is constant across the list.  Then call the
 * pthreadDoList routine,  which will return after all items on the list have been
 * processed.  In general the list items will contain output as well as input fields.
 * The routine that works on an item should not alter any variables except for local
 * variables, or output fields for an item.  See below for an example. */

typedef void PthreadListWorker(void *item, void *context);
/* This gets passed a variable item from list, and a constant context */

void pthreadDoList(int threadCount, void *workList,  PthreadListWorker *worker, void *context);
/* Work through list with threadCount workers each in own thread. 
 * The worker will be called in parallel with an item from work list and the context
 *       worker(item, context)
 * The context is constant across all threads and items. */

#ifdef EXAMPLE

/* In this example we'll raise each element from 1 to 10 to the 4th power */

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

void main()
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

#endif /* EXAMPLE */

#endif /* PTHREADDOLIST_H */
