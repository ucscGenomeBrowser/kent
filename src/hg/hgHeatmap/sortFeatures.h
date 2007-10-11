/* sortFeatures.h - interfaces to plug features into feature sorter. */

#ifndef SORTFEATURES_H
#define SORTFEATURES_H
#include "cart.h"
#include "jksql.h"


struct sortList
/* List of sortNodes */
{
    struct sortList *next;
    double val;
    int sortDirection;
};


struct sortNode
/* A node in the sorted tree. The central data structure for
 * sortFeatures. */
{
    struct sortNode *next;
    char *name;
    struct sortList *list;
};

struct slName *sortPatients(struct sqlConnection *conn, struct column *colList, struct slName *patientList);
/* Sort a list of patients based on active columns */ 


#endif /* SORTFEATURES_H */

