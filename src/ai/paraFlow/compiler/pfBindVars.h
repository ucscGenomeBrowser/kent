/* pfBindVars - bind variables into parse tree. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef PFBINDVARS_H
#define PFBINDVARS_H

void pfBindVars(struct pfCompile *pfc, struct pfParse *pp);
/* pfBindVars - massage parse tree slightly to regularize
 * variable and function declarations.  Then put all declarations
 * in the appropriate scope, and bind variable uses and function
 * calls to the appropriate declaration.  This will complain
 * about undefined symbols. */

#endif /* PFBINDVARS_H */
