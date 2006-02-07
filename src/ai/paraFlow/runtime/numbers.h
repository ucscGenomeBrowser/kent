/* numbers.c Some numerically oriented built-in functions. */

#ifndef NUMBERS_H
#define NUMBERS_H

void pf_randInit(_pf_Stack *stack);
/* Change random number stream by initializing it with the current time. */

void pf_randNum(_pf_Stack *stack);
/* Return random number between 0 and 1 */

void pf_sqrt(_pf_Stack *stack);
/* Return square root of number. */

void pf_atoi(_pf_Stack *stack);
/* Return binary representation of ascii-encoded number. */

#endif /* NUMBERS_H */

