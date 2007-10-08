/* sortFeatures.h - interfaces to plug features into feature sorter. */

#ifndef SORTFEATURES_H
#define SORTFEATURES_H
#include "cart.h"
#include "jksql.h"


struct sortList
/* List of sortNodes */
{
    struct sortList *next;
    struct sortNode *node;
};


struct sortNode
/* A node in the sorted tree. The central data structure for
 * sortFeatures. */
{
    struct sortList *children;
    struct sortNode *parent;

    char *name;
    double val;
};

char *sortPatients(struct sqlConnection *conn, struct column *colList, char *patientStr);
/* Sort a comma-separated set of patients based on active columns */ 


#endif /* SORTFEATURES_H */

