/* flyBase.c - handle flybase stuff. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "obscure.h"
#include "hgNear.h"


static char *flyTxToGene(char *tx)
/* Convert transcript to gene (by cutting off '-') */
{
char *e = strchr(tx, '-');
if (e == NULL)
    return cloneString(tx);
else
    return cloneStringZ(tx, e-tx);
}

static void flyBdgpCellPrint(struct column *col, struct genePos *gp,
	struct sqlConnection *conn)
/* Print trancript name and link to fruitfly.org. */
{
char *geneName = flyTxToGene(gp->name);
hPrintf("<TD><A HREF=\"");
hPrintf(col->itemUrl, geneName);
hPrintf("\" TARGET=_blank>");
hPrintEncodedNonBreak(gp->name);
hPrintf("</A></TD>");
}

void setupColumnFlyBdgp(struct column *col, char *parameters)
/* Set up Bdgp gene column. */
{
setupColumnAcc(col, parameters);
col->cellPrint = flyBdgpCellPrint;
}

