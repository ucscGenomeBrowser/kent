/* examples.c - Put up text with quick usage and examples. */

#include "common.h"
#include "linefile.h"
#include "htmshell.h"
#include "hgNear.h"

void doExamples(struct sqlConnection *conn, struct column *colList)
/* Put up controls and then some helpful text and examples.
 * Called when search box is empty. */
{
displayData(conn, colList, NULL);
htmlHorizontalLine();
hPrintf("%s",
 "<P>This program displays a table of genes that are related to "
 "one another.  The relationship can be one of several types, including "
 "protein-level homology, similarity of gene expression profiles, or "
 "genomic proximity. </P>"
 "<P>To display a gene family:"
 "<OL>"
 "<LI>Select a genome and assembly from the corresponding pull-down menus. " 
 "<LI>Type a word or phrase into the <em>search</em> text box to specify "
 "which gene should be displayed in the browser. "
);

hPrintf("%s", genomeSetting("examples"));

hPrintf("%s", 
 "<LI>Choose the gene relationship that you would like to examine by "
 "selecting an option from the <em>sort by</em> pull-down menu." 
 "<LI>Press the Go! button to display your results. " 
 "</OL>"
 "</P> "
 );

hPrintf("%s", 
 "<P>Following a successful search, the Browser displays a table containing "
 "the specified gene -- highlighted in light green -- and its relatives, "
 "each on a separate line. If the search was based on genomic position, "
 "the selected gene is shown in the middle row of the table. In all other "
 "cases, the highlighted gene appears first. To adjust the number of rows "
 "shown, select an option from the <em>display</em> pull-down menu. "
 "To fine-tune the list of displayed genes to a subset based on a selection of "
 "detailed and flexible criteria, click the <em>filter</em> button. </P>"
 "<P>The default set of table columns -- "
 "which can be expanded, reduced, and rearranged via the <em>configure</em> "
 "button -- show additional information about the genes. Some of the column "
 "data, such as those in the <em>BLAST E-value</em> and <em>%ID</em> columns, "
 "are calculated relative to the highlighted gene. To select a different gene "
 "in the list, click on its name. "
 "Clicking on a gene's <em>Genome Position</em> will open the UCSC Genome "
 "Browser to the location of that gene. Similarly, clicking on a gene's "
 "<em>Description</em> will open a page showing detailed information about "
 "the gene.</P>"
 "<P>The Browser offers two options for displaying and downloading sequence "
 "associated with the genes in the table. Clicking on the <em>sequence</em> "
 "button will fetch associated protein, mRNA, promoter, or genomic sequence.  "
 "To dump the table into a simple tab-delimited format suitable for "
 "import into a spreadsheet or relational database, click the <em>text</em> "
 "button. "
 "<P>The UCSC Gene Family Browser was designed and implemented by Jim Kent, "
 "Fan Hsu, David Haussler, and the UCSC Genome Bioinformatics Group. This "
 "work is supported by a grant from the National Human Genome Research "
 "Institute and by the Howard Hughes Medical Institute.</P>"
 );
}

