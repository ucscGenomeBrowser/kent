/* checkPara - Stuff to check that flow and para statements
 * are good in the sense that there are no inappropriate writes
 * to variables. */
/* Copyright 2005-6 Jim Kent.  All rights reserved. */

#ifndef CHECKPARA_H
#define CHECKPARA_H

void checkParaFlow(struct pfCompile *pfc, struct pfParse *program);
/* Check para and flow declarations throughout program. */

#endif /* CHECKPARA_H */

