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
 "each other.  The relationship can be of several types including "
 "protein-level homology, similarity of gene expression profiles, or "
 "genomic proximity.  The 'group by' drop-down controls "
 "which type of relationship is used.</P>"
 "<P>To use this tool please type something into the search box and "
 "hit the 'Go' button. You can search for many types of things including "
 "the gene name, the SwissProt protein name, a word or phrase "
 "that occurs in the description of a gene, or a GenBank mRNA accession. ");

hPrintf("%s", genomeSetting("examples"));
hPrintf("%s", 
 "</P>"
 "<P>After the search a table appears containing the gene and its relatives, "
 "one gene per row.  The gene matching the search will be hilighted in light "
 "green and in most cases will in the first row of the table. "
 "Some of the columns in the table including the BLAST 'E-value' and "
 "'%ID' columns will be calculated relative to the hilighted gene.  You can "
 "select a different gene in the list by clicking on the gene's name. "
 "Clicking on the 'Genome Position' will open the Genome Browser on that "
 "gene.  Clicking on the 'Description' will open a details page on the "
 "gene.</P>"
 "<P>There are many additional columns besides the ones that are initially "
 "displayed.  Click on the 'configure' button to see the full list. "
 "The 'sequence' button will fetch protein, mRNA, promoter, or "
 "genomic sequence associated with the genes in the table.  The 'text' "
 "button fetches the table in a simple tab-delimited format suitable for "
 "import into a spreadsheet or relational database. <P>"
 "<P>A particularly powerful feature of this program is the filter "
 "button.  This brings you to a page where you can select which genes "
 "are displayed in a very detailed and flexible fashion. For instance "
 "you could set the filter to select all human genes overexpressed in "
 "ther cerebellum that have GO-annotated G-protein coupled receptor activity."
 "</P>"
 "<P>The UCSC Gene Family Browser was designed and implemented by Jim Kent, "
 "Fan Hsu, David Haussler, and the UCSC Genome Bioinformatics Group. This "
 "work is supported by a grant from the National Human Genome Research "
 "Institute and by the Howard Hughes Medical Institute.</P>"
 );
}

