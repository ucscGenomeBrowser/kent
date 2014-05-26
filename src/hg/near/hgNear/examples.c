/* examples.c - Put up text with quick usage and examples. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "htmshell.h"
#include "hgColors.h"
#include "hgNear.h"

void doExamples(struct sqlConnection *conn, struct column *colList)
/* Put up controls and then some helpful text and examples.
 * Called when search box is empty. */
{
displayData(conn, colList, NULL);
hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 BGCOLOR=\"#"HG_COL_BORDER"\"><TR><TD>");
hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=9 BGCOLOR=\"#"HG_COL_INSIDE"\"><TR><TD>\n");
hPrintf("%s",
 "<H3>About the Gene Sorter</H3>"
 "<P>This program displays a sorted table of genes that are related to "
 "one another.  The relationship can be one of several types, including "
 "protein-level homology, similarity of gene expression profiles, or "
 "genomic proximity. </P>"
 "<P>To display a gene and its relatives:"
 "<OL>"
 "<LI>Select a genome and assembly from the corresponding pull-down menus. " 
 "<LI>Type a word or phrase into the <em>search</em> text box to specify "
 "which gene should be displayed in the Gene Sorter. "
);

hPrintf("%s", genomeSetting("examples"));

hPrintf("%s", 
 "<LI>Choose the gene relationship with which you would like to sort the list "
 "by selecting an option from the <em>sort by</em> pull-down menu." 
 "<LI>Press the Go! button to display your results. " 
 "</OL>"
 "</P> "
 );

hPrintf("%s", 
 "<P>Following a successful search, the Gene Sorter displays a table containing "
 "the specified gene -- highlighted in light green -- and its relatives, "
 "each on a separate line.  To adjust the number of rows "
 "shown, select an option from the <em>display</em> pull-down menu. </P>"
 "<P>The default set of table columns -- "
 "which can be expanded, reduced, and rearranged via the <em>configure</em> "
 "button -- shows additional information about the genes. Some of the column "
 "data, such as those in the <em>BLAST E-value</em> and <em>%ID</em> columns, "
 "are calculated relative to the highlighted gene. To select a different gene "
 "in the list, click on its name. "
 "Clicking on a gene's <em>Genome Position</em> will open the UCSC Genome "
 "Browser to the location of that gene. Similarly, clicking on a gene's "
 "<em>Description</em> will open a page showing detailed information about "
 "the gene.</P>"
 "<P>One of the most powerful features of the Gene Sorter is its "
 "filtering capabilities, accessed via the <em>filter</em> button. Use the "
 "filter to fine-tune the list of displayed genes to a subset based on a "
 "selection of detailed and flexible criteria. For example, the filter may "
 "be used to select all human genes over-expressed in the cerebellum that have "
 "GO-annotated G-protein coupled receptor activity.</P>"
 "<P>The Gene Sorter offers two options for displaying and downloading sequence "
 "associated with the genes in the table. Clicking on the <em>sequence</em> "
 "button will fetch associated protein, mRNA, promoter, or genomic sequence.  "
 "To dump the table into a simple tab-delimited format suitable for "
 "import into a spreadsheet or relational database, click the <em>text</em> "
 "button. "
 "<P>The UCSC Gene Sorter was designed and implemented by Jim Kent, "
 "Fan Hsu, Donna Karolchik, David Haussler, and the UCSC Genome Bioinformatics "
 "Group. This work is supported by a grant from the National Human Genome "
 "Research Institute and by the Howard Hughes Medical Institute.</P>"
 );
hPrintf("</TD></TR></TABLE>");
hPrintf("</TD></TR></TABLE>");
}

