#ifndef FINDKGALIAS_H
#define FINDKGALIAS_H

static void addKgAlias(struct sqlConnection *conn, struct dyString *query,
       struct kgAlias **pList);
struct kgAlias *findKGAlias(char *dataBase, char *spec, char *mode);
#endif
