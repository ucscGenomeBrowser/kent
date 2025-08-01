/* genark.h was originally generated by the autoSql program, which also 
 * generated genark.c and genark.sql.  This header links the database and
 * the RAM representation of objects. */

#ifndef GENARK_H
#define GENARK_H

#include "jksql.h"
#define GENARK_NUM_COLS 8

#define defaultGenarkTableName "genark"
/* Name of table that maintains the names of hubs we'll automatically attach if referenced. */

#define genarkTableConfVariable    "hub.genArkTableName"
/* the name of the hg.conf variable to use something other than the default */


extern char *genarkCommaSepFieldNames;

struct genark
/* index to UCSC assembly hubs */
    {
    struct genark *next;  /* Next in singly linked list. */
    char *gcAccession;	/* GC[AF] accssion identifier, e.g.: GCF_000001405.39 */
    char *hubUrl;	/* path name to hub.txt: GCF/000/001/405/GCF_000001405.39/hub.txt */
    char *asmName;	/* assembly name: GRCh38.p13 */
    char *scientificName;	/* scientific name: Homo sapiens */
    char *commonName;	/* common name: human */
    int taxId;	/* taxon id: 9606 */
    int priority;	/* search priority to order hgGateway results */
    char *clade;	/* clade group in the GenArk system */
    };

void genarkStaticLoad(char **row, struct genark *ret);
/* Load a row from genark table into ret.  The contents of ret will
 * be replaced at the next call to this function. */

struct genark *genarkLoadByQuery(struct sqlConnection *conn, char *query);
/* Load all genark from table that satisfy the query given.  
 * Where query is of the form 'select * from example where something=something'
 * or 'select example.* from example, anotherTable where example.something = 
 * anotherTable.something'.
 * Dispose of this with genarkFreeList(). */

void genarkSaveToDb(struct sqlConnection *conn, struct genark *el, char *tableName, int updateSize);
/* Save genark as a row to the table specified by tableName. 
 * As blob fields may be arbitrary size updateSize specifies the approx size
 * of a string that would contain the entire query. Arrays of native types are
 * converted to comma separated strings and loaded as such, User defined types are
 * inserted as NULL. This function automatically escapes quoted strings for mysql. */

struct genark *genarkLoad(char **row);
/* Load a genark from row fetched with select * from genark
 * from database.  Dispose of this with genarkFree(). */

struct genark *genarkLoadAll(char *fileName);
/* Load all genark from whitespace-separated file.
 * Dispose of this with genarkFreeList(). */

struct genark *genarkLoadAllByChar(char *fileName, char chopper);
/* Load all genark from chopper separated file.
 * Dispose of this with genarkFreeList(). */

#define genarkLoadAllByTab(a) genarkLoadAllByChar(a, '\t');
/* Load all genark from tab separated file.
 * Dispose of this with genarkFreeList(). */

struct genark *genarkCommaIn(char **pS, struct genark *ret);
/* Create a genark out of a comma separated string. 
 * This will fill in ret if non-null, otherwise will
 * return a new genark */

void genarkFree(struct genark **pEl);
/* Free a single dynamically allocated genark such as created
 * with genarkLoad(). */

void genarkFreeList(struct genark **pList);
/* Free a list of dynamically allocated genark's */

void genarkOutput(struct genark *el, FILE *f, char sep, char lastSep);
/* Print out genark.  Separate fields with sep. Follow last field with lastSep. */

#define genarkTabOut(el,f) genarkOutput(el,f,'\t','\n');
/* Print out genark as a line in a tab-separated file. */

#define genarkCommaOut(el,f) genarkOutput(el,f,',',',');
/* Print out genark as a comma separated list including final comma. */

void genarkJsonOutput(struct genark *el, FILE *f);
/* Print out genark in JSON format. */

/* -------------------------------- End autoSql Generated Code -------------------------------- */

char *genarkUrl(char *accession);
/* Return the URL to the genark assembly with this accession if present,
 * otherwise return NULL
 * */

char *genArkPath(char *genome);
/* given a GenArk hub genome name, e.g. GCA_021951015.1 return the path:
 *               GCA/021/951/015
 * prefix that with desired server URL: https://hgdownload.soe.ucsc.edu/hubs/
 *   if desired.  Or suffix add /hub.txt to get the hub.txt URL
 *   The path returned does not depend upon this GCx_ naming scheme,
 *   it simply uses the hub URL as returned from genarkUrl(genome) and
 *   returns the middle part without the https://... prefix
 */

char *genarkTableName();
/* return the genark table name from the environment, 
 * or hg.conf, or use the default.  Cache the result */

/* temporary function while the genark table is in transistion with
 * new coluns being added, July 2024.  Allows compatibility with existing
 * genark table.
 */
int genArkColumnCount();
/* return number of columns in genark table */

boolean isGenArk(char *genome);
/* given a genome name, see if it is in the genark table to determine
 *  yes/no this is a genark genome assembly
 */

struct dbDb *genarkLiftOverDbs(char *listOfAccs);
/* return list of dbDb structures for the genark genomes that match listOfAccs */

struct dbDb *genarkLiftOverDb(char *acc);
/* return dbDb structure for GC* acc */

struct hash *genarkGetOrgHash();
/* read table that maps gcAccession to UCSC org. */
#endif /* GENARK_H */

