/* pfam - handle pfam columns.  This requires a join. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hgNear.h"

static boolean isPfamId(char *name)
/* Return TRUE if this is a pfam ID. */
{
return (name[0] == 'P' && name[1] == 'F' 
	&& isdigit(name[2]) && isdigit(name[3]) && isdigit(name[4]) 
	&& isdigit(name[5]) && isdigit(name[6]) && name[7] == 0);
}

static void pfamFilterControls(struct column *col, 
	struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
hPrintf(
  "Terms can include Pfam descriptions such as 'Cytochrome P450'<BR>"
  "or Pfam IDs such as PF00067. Please enclose term in single quotes<BR>"
  "if it contains multiple words. You may use * and ? wildcards.<BR>\n");
hPrintf("Term(s): ");
advFilterRemakeTextVar(col, "terms", 35);
hPrintf(" Include if ");
advFilterAnyAllMenu(col, "logic", FALSE);
hPrintf("terms match");
}


static struct genePos *pfamAdvFilter(struct column *col, 
	struct sqlConnection *defaultConn, struct genePos *list)
/* Do advanced filter on for pfam. */
{
char *terms = advFilterVal(col, "terms");
if (terms != NULL)
    {
    struct sqlConnection *conn = sqlConnect(col->protDb);
    char query[256];
    struct sqlResult *sr;
    struct dyString *dy = newDyString(1024);
    char **row;
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct slName *term, *termList = stringToSlNames(terms);
    struct hash *passHash = newHash(17);
    struct hash *prevHash = NULL;
    struct genePos *gp;

    /* Build up hash of all genes. */
    struct hash *geneHash = newHash(18);
    for (gp = list; gp != NULL; gp = gp->next)
        hashAdd(geneHash, gp->name, gp);
    for (term = termList; term != NULL; term = term->next)
        {
	/* Build up a list of IDs of descriptions that match term. */
	struct slName *idList = NULL, *id;
	if (isPfamId(term->name))
	    {
	    idList = slNameNew(term->name);
	    }
	else
	    {
	    char *sqlWild = sqlLikeFromWild(term->name);
	    sqlSafef(query, sizeof(query),
	    	"select pfamAC from pfamDesc where description like '%s'",
		sqlWild);
	    sr = sqlGetResult(conn, query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
	        id = slNameNew(row[0]);
		slAddHead(&idList, id);
		}
	    sqlFreeResult(&sr);
	    }

	if (idList != NULL)
	    {
	    /* Build up query that includes all IDs. */
	    dyStringClear(dy);
	    sqlDyStringPrintf(dy, "select name from %s where ", col->table);
	    sqlDyStringPrintf(dy, "value='%s'", idList->name);
	    for (id = idList->next; id != NULL; id = id->next)
		sqlDyStringPrintf(dy, "or value='%s'", id->name);

	    /* Execute query and put matchers into hash. */
	    sr = sqlGetResult(defaultConn, dy->string);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		gp = hashFindVal(geneHash, row[0]);
		if (gp != NULL)
		    {
		    char *name = gp->name;
		    if (prevHash == NULL || hashLookup(prevHash, name) != NULL)
			hashStore(passHash, name);
		    }
		}
	    sqlFreeResult(&sr);
	    slFreeList(&idList);
	    }
	if (!orLogic)
	    {
	    hashFree(&prevHash);
	    if (term->next != NULL)
		{
		prevHash = passHash;
		passHash = newHash(17);
		}
	    }
	}
    list = weedUnlessInHash(list, passHash);
    hashFree(&prevHash);
    hashFree(&passHash);
    dyStringFree(&dy);
    sqlDisconnect(&conn);
    }
return list;
}

void setupColumnPfam(struct column *col, char *parameters)
/* Setup Pfam column. */
{
setupColumnAssociation(col, parameters);
col->table = cloneString(parameters);
if ((col->protDb = columnSetting(col, "protDb", NULL)) == NULL)
    errAbort("Missing required protDb field in column %s", col->name);
col->advFilter = pfamAdvFilter;
col->filterControls = pfamFilterControls;
}

