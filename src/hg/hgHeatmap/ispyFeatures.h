/* ispyFeatures.h - interfaces to plug features into feature sorter.  The must important
 * thing here is the column structure. */

#ifndef ISPYFEATURES_H
#define ISPYFEATURES_H

#include "cart.h"

#include "jksql.h"


#define colOrderVar "heat.colOrder"     /* Order of columns. */
#define colConfigPrefix "heat.col." /* Prefix for stuff set in configuration pages. */
#define colInfoVarName "heat.do.colInfo"/* Display column info. */

struct expId
/* Id for patient/experiment in ISPY database */
{
  struct expId *next;

  char *name;
};



struct column
/* A column in the big table. The central data structure for
 * hgNear. */
{
  /* Data set during initialization that is guaranteed to be in each column.  */
  struct column *next;/* Next column. */
  char *name;/* Column name, not allocated here. */
  char *shortLabel;/* Column label. */
  char *longLabel;/* Column description. */
  boolean on;/* True if turned on. */
  boolean defaultOn;/* On by default? */
  float priority;/* Order displayed. */
  char *type;/* Type - encodes which methods to used etc. */
  char *itemUrl;/* printf formatted URL string for linking.
                 * May be NULL.  Should contain one %s, which
                 * get's filled in with whatever cellVal
                 * returns. */
  char *itemUrlQuery;/* SQL query. Does lookup from cellVal
                      * to the desired value to use in itemUrl in
                      * place of cellVal */

  struct hash *settings;/* Settings from ra file. */

  /* -- Methods -- */
  boolean (*exists)(struct column *col, struct sqlConnection *conn);
  /* Return TRUE if column exists in database. */

  char *(*cellVal)(struct column *col, struct expId *id,
                   struct sqlConnection *conn);
  /* Get value of one cell as string.  FreeMem this when done.  Note that
   * gp->chrom may be NULL legitimately. */

  void (*cellPrint)(struct column *col, struct expId *id,
                    struct sqlConnection *conn);
  /* Print one cell of this column in HTML.  Note that gp->chrom may be
   * NULL legitimately. */

  void (*labelPrint)(struct column *col);
  /* Print the label in the label row. */

  int (*tableColumns)(struct column *col);
  /* How many html columns this uses. */

  void (*configControls)(struct column *col);
  /* Print out configuration controls. */

  /* -- Data that may be column-specific. -- */
  /* Most columns that need any extra data at all use the next few fields. */
  char *table;/* Name of associated table. */
  char *keyField;/* GeneId field in associated table. */
  char *valField;/* Value field in associated table. */

};

void setupColumnType(struct column *col);
/* Set up methods and column-specific variables based on
 * track type. */

struct column *getColumns(struct sqlConnection *conn);
     /* Return list of columns for big table. */

void doConfigure(struct sqlConnection *conn, struct column *colList,
		 char *bumpVar);
/* Configuration page. */

int columnCmpPriority(const void *va, const void *vb);
/* Compare to sort columns based on priority. */
     
void refinePriorities(struct hash *colHash);
/* Consult colOrderVar if it exists to reorder priorities. */

void refineVisibility(struct column *colList);
/* Consult cart to set on/off visibility. */




#endif /* ISPYFEATURES_H */
