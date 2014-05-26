/* afiAli.c - show affine alignment. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "dnaseq.h"
#include "fa.h"
#include "axt.h"
#include "hgNear.h"


static bioSeq *getSeq(struct sqlConnection *conn, char *geneId, 
	struct column *nameCol, char *tableId)
/* Get sequence from table. */
{
struct genePos *gp = knownPosOne(conn, geneId);

if (gp == NULL)
    {
    warn("Can't find %s in database", geneId);
    return NULL;
    }
else
    {
    char *table = genomeSetting(tableId);
    char query[512];
    struct sqlResult *sr;
    char **row;
    bioSeq *seq = NULL;
    char *name;
    if (nameCol != NULL)
	name = nameCol->cellVal(nameCol, gp, conn);
    else
        name = cloneString(geneId);
    sqlSafef(query, sizeof(query), 
	"select seq from %s where name = '%s'", table, geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(seq);
	seq->name = name;
	seq->dna = cloneString(row[0]);
	seq->size = strlen(seq->dna);
	}
    sqlFreeResult(&sr);
    return seq;
    }
}

void doAffineAlignment(struct sqlConnection *conn)
/* Put up page that shows affine alignment. */
{
struct column *nameCol = findNamedColumn("name");
char *aId = cartString(cart, affineAliVarName);
char *bId = cartString(cart, searchVarName);
bioSeq *a = getSeq(conn, aId, nameCol, "pepTable");
bioSeq *b = getSeq(conn, bId, nameCol, "pepTable");
struct axtScoreScheme *ss = axtScoreSchemeProteinDefault();
makeTitle("Affine Alignment", NULL);

if (a != NULL && b != NULL)
    {
    printf("<TT><PRE>");
    if (axtAffineSmallEnough(a->size, b->size))
	{
	struct axt *axt = axtAffine(a, b, ss);
	if (axt != NULL)
	    {
	    printf("Alignment between %s (top %s %daa) and %s (bottom %s %daa) score %d\n\n",
		    a->name, aId, a->size, b->name, bId, b->size, axt->score);
	    axtPrintTraditional(axt, 60, ss, stdout);
	    axtFree(&axt);
	    }
	else
	    {
	    printf("%s and %s don't align\n", a->name, b->name);
	    }
	}
    else
	{
	printf("Sorry, %s (%d amino acids) and %s (%d amino acids) are too big to align",
	    a->name, a->size, b->name, b->size);
	}
    printf("</PRE></TT>");
    }
}

