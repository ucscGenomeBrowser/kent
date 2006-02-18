/* runErr - handle run-time errors (and try/catch). */

#ifndef RUNERR_H
#define RUNERR_H

#include <setjmp.h>
#define boolean int
#include "../../../inc/errCatch.h"

struct _pf_err_catch
/* Something to help catch errors.   */
    {
    struct _pf_err_catch *next;	 /* Next in stack. */
    jmp_buf jmpBuf;		 /* Where to jump back to for recovery. */
    struct _pf_activation *act;	 /* Activation record of our function. */
    boolean gotError;		 /* If true we have an error. */
    int level;			 /* We can handle errors of this level or more. */
    char *message;		 /* Error message, filled by puntCatcher. */
    char *source;		 /* Error source,  filled by puntCatcher. */
    };
typedef struct _pf_err_catch *_pf_Err_catch;

struct _pf_err_catch *_pf_err_catch_new(struct _pf_activation *act, int level);
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

#define _pf_err_catch_got_err(e) (e->gotError)
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
