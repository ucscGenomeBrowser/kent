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
 "<P>To use this tool please type something into the search column and "
 "hit the 'Go' button. You can search for many types of things including "
 "the gene name, the SwissProt protein name, a word or phrase "
 "that occurs in the description of a gene, or a GenBank mRNA accession. ");

hPrintf("%s", genomeSetting("examples"));
hPrintf("%s", 
 "</P>"
 "<P>After the search a table appears containing the gene and it's relatives, "
 "one gene per row.  The gene matching the search will be hilighted in light "
 "green.  In the case of search by genome position, the hilighted gene will "
 "be in the middle of the table. In other cases it will be the first row. "
 "Some of the columns in the table including the BLAST 'E-value' and "
 "'%ID' columns will be calculated relative to the hilighted gene.  You can "
 "select a different gene in the list by clicking on the gene's name. "
 "Clicking on the 'Genome Position' will open the Genome Browser on that "
 "gene.  Clicking on the 'Description' will open a details page on the "
 "gene.</P>"
 "<P>To control which columns are displayed in the table use the 'configure' "
 "button. To control the number of rows displayed use the 'display' drop "
 "down. The 'as sequence' button will fetch protein, mRNA, promoter, or "
 "genomic sequence associated with the genes in the table.  The 'as text' "
 "button fetches the table in a simple tab-delimited format suitable for "
 "import into a spreadsheet or relational database. The advanced filter "
 "button allows you to select which genes are displayed in the table "
 "in a very detailed and flexible fashion.</P>"
 "<P>The UCSC Gene Family Browser was designed and implemented by Jim Kent, "
 "Fan Hsu, David Haussler, and the UCSC Genome Bioinformatics Group. This "
 "work is supported by a grant from the National Human Genome Research "
 "Institute and by the Howard Hughes Medical Institute.</P>"
 );
}

