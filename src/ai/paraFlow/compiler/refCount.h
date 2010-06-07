/* refCount - stuff to manipulate object reference counts.
 * We have to use a little inline assembler to get these to
 * happen in a thread-safe manner. */
    
#ifndef REFCOUNT_H
#define REFCOUNT_H

void codeVarIncRef(FILE *f, char *varName);
/* Generate code to bump ref count of a variable. */

void codeStackIncRef(FILE *f, int stack);
/* Bump ref count of something on expression stack. */

void codeVarDecRef(FILE *f, char *varName);
/* Generate code to decrement ref count of a variable. */

void codeStackDecRef(FILE *f, int stack);
/* Decrement ref count of something on expression stack. */

#endif /* REFCOUNT_H */


