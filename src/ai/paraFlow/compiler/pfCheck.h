/* pfCheck - Stuff to check that flow and para statements
 * are good in the sense that there are no inappropriate writes
 * to variables. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef PFCHECK_H
#define PFCHECK_H


void pfCheckParaFlow(struct pfCompile *pfc, struct pfParse *program);
/* Check para and flow declarations throughout program. */
#endif /* PFCHECK_H */
