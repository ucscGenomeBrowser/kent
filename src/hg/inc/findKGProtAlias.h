/* functions for searching Known Gene entries using protein aliases */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef FINDKGPROTALIAS_H
#define FINDKGPROTALIAS_H

void addKGProtAlias(struct sqlConnection *conn, struct dyString *query,
       struct kgProtAlias **pList);
/* Query database and add returned kgAlias to head of list. */

struct kgProtAlias *findKGProtAlias(char *dataBase, char *spec, char *mode);
/* findKGProtAlias Looks up protein aliases for Known Genes, given a seach spec */
#endif
