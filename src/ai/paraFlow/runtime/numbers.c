/* numbers.c Some numerically oriented built-in functions. */

#include "common.h"
#include "portable.h"
#include "../compiler/pfPreamble.h"
#include "numbers.h"

void pf_randInit(_pf_Stack *stack)
/* Change random number stream by initializing it with the current time. */
{
srand(clock1000());
rand();
}

void pf_randNum(_pf_Stack *stack)
/* Return random number between 0 and 1 */
{
static double scale = 1.0/RAND_MAX;
stack[0].Double = rand()*scale;
}

void pf_sqrt(_pf_Stack *stack)
/* Return square root of number. */
{
stack[0].Double = sqrt(stack[0].Double);
}

void pf_atoi(_pf_Stack *stack)
/* Return binary representation of ascii-encoded number. */
{
int result = 0;
char *s = stack[0].String->s;
if (s != NULL)
    result = atoi(s);
stack[0].Int = result;
}

