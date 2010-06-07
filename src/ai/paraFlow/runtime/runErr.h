/* runErr - handle run-time errors (and try/catch). */

#ifndef RUNERR_H
#define RUNERR_H

#include <setjmp.h>

struct _pf_error
/* A run time error. */
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct _pf_error *obj, int id); /* Called when refCount <= 0 */
    _pf_polyFunType *_pf_polyFun;	/* Polymorphic functions. */
    struct _pf_error *next;			/* Next error in chain. */
    struct _pf_string *message;		/* Human readable message. */
    struct _pf_string *source;		/* Associated error source. */
    _pf_Int code;			/* Error code. */
    };

struct _pf_err_catch
/* Something to help catch errors.   */
    {
    struct _pf_err_catch *next;	 /* Next in stack. */
    jmp_buf jmpBuf;		 /* Where to jump back to for recovery. */
    struct _pf_activation *act;	 /* Activation record of our function. */
    int catchType;		 /* Type of error we catch. */
    struct _pf_error *err;	 /* Information about the error. */
    };
typedef struct _pf_err_catch *_pf_Err_catch;

struct _pf_err_catch *_pf_err_catch_new(struct _pf_activation *act,
	int errTypeId);
/* Create a new error catcher. */

#define _pf_err_catch_start(e) \
	(_pf_err_catch_push(e) && setjmp(e->jmpBuf) == 0)
/* Start up error catcher.  For reasons I don't fully understand
 * the setjmp needs to be inline, not embedded in a function,
 * hence this macro. */

int _pf_err_catch_push(struct _pf_err_catch *err);
/* Pushes catcher on catch stack and returns TRUE. Not typically
 * used directly, but called vi _pf_err_catch_start macro. */

void _pf_err_catch_end(struct _pf_err_catch *err);
/* Pop error catcher off of stack.  Don't free it up yet, because
 * we want to examine gotErr still. */

#define _pf_err_catch_got_err(e) (e->err)
/* Return TRUE if catcher got an error. */

void _pf_err_catch_free(struct _pf_err_catch **pErr);
/* Free up memory associated with error catcher. */

void _pf_run_err(char *message);
/* Run time error.  Prints message, dumps stack, and aborts. 
 * Hard run-time error, like array access out of bounds or
 * something.  This is level -1. */

void _pf_punt_init();
/* Initialize punt/catch system.  Mostly just redirects the
 * errAbort handler. */

void pf_punt(_pf_Stack *stack);
/* Application punt request. */

#endif /* RUNERR_H */
