/* hgGateway - Human Genome Browser Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"

struct cart *cart;
struct hash *oldVars = NULL;

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *db = cartUsualString(cart, "db", hGetDb());
char *oldDb = hashFindVal(oldVars, "db");
char *defaultPosition = "USP18";
char *position = cartUsualString(cart, "position", defaultPosition);

if (oldDb != NULL && !sameString(db, oldDb))
    position = defaultPosition;

puts(
"<FORM ACTION=\"../cgi-bin/hgTracks\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n"
"<CENTER>"
"Web tool created by "
"<A HREF=\"http://www.soe.ucsc.edu/~kent\">Jim Kent</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~sugnet\">Charles Sugnet</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~booch\">Terry Furey</A> and "
"<A HREF=\"http://www.soe.ucsc.edu/~haussler\">David Haussler</A> "
"of UC Santa Cruz.<BR>\n");
printf("%s draft assembly.", hFreezeFromDb(db));
printf("</CENTER><P>");
puts("Please enter a position in the genome, set your preferred window "
     "width, and press the submit button. ");
puts("<A HREF=\"../cgi-bin/cartReset\">Select here to reset</A> the "
      "browser user interface settings to their defaults.");
printf("</P><CENTER>");
printf("position ");
cartSaveSession(cart);
cgiMakeTextVar("position", position, 30);
printf(" pixel width ");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", 610), 4);
printf(" ");
cgiMakeButton("Submit", "Submit");
cartSetString(cart, "db", db);
printf("</CENTER>");

puts(
"<P>A genome position can be specified by the accession number of a "
"sequenced genomic clone, an mRNA or EST or STS marker, or \n"
"a cytological band, a chromosomal coordinate range, or keywords from "
"the Genbank description of an mRNA. The following list provides "
"examples of various types of position queries for the human genome. "
"Analogous queries can be made for many of these in the mouse genome. "
"See the "
"<A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>"
"User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<CENTER> <TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr7</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays all of chromosome 7</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>20p13</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region for band p13 on chr
20</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr3:1-1000000</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays first million bases of chr 3, counting from p arm telomere</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>D16S3046</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region around STS marker D16S3046 from the Genethon/Marshfield maps\n"
"	(open \"STS Markers\" track by clicking to see this marker) </TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>D22S586;D22S43</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region between STS markers "
"D22S586 and D22S43.  Includes 250,000 bases to either "
"side as well."
"</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>AA205474</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of EST with GenBank acc.  AA205474 in BRCA1 cancer gene on chr 17\n"
"	(open \"spliced ESTs\" track by clicking to see this EST)</TD></TR>\n"
"<!-- <TR><TD VALIGN=Top NOWRAP>ctgchr7_ctg</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of the clone contig ctgchr7_ctg\n"
"	(set \"Map contigs\" track to \"dense\" and refresh to see contigs)</TD></TR> -->\n"
"<TR><TD VALIGN=Top NOWRAP>AC008101</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of clone with GenBank accession AP001670\n"
"	(open \"coverage\" track by clicking to see this clone)</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AF083811</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of mRNA with GenBank accession number AF083811</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>PRNP</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD>Displays region of genome with HUGO identifier PRNP</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>homeobox caudal</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs for caudal homeobox genes</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists many zinc finger mRNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>kruppel zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists only kruppel-like zinc fingers</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>huntington</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists candidate genes associated with Huntington's disease</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zahler</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by scientist named Zahler</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Evans,J.E.</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author J.E.  Evans</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- "
"even though Genbank searches require Evans JE "
"format, GenBank entries themselves use Evans,J.E.  internally.\n"
"</TABLE></CENTER>\n"
"\n");

webNewSection("Add Your Own Tracks");

puts(
"<P>You can add your own annotation tracks to the browser using \n"
"the <A HREF=\"../goldenPath/help/customTrack.html\"> \n"
"procedure described here</A>.  Annotations can be stored in files or\n"
"pasted in. You can also paste in a URL or list of URLs which refer to \n"
"files in one of the supported formats.</P>\n"
"\n"
"	Annotation File: <INPUT TYPE=FILE NAME=\"hgt.customFile\"><BR>\n"
"	<TEXTAREA NAME=\"hgt.customText\" ROWS=14 COLS=80></TEXTAREA>\n"
"	</FORM>\n"
"	<BR></TD><TD WIDTH=15>&nbsp;</TD></TR></TABLE>\n"
"	</TD></TR></TABLE>\n"
"			\n"
"</TD></TR></TABLE>\n");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
cart = theCart;
cartWebStart("UCSC Genome Browser Gateway");
hgGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, "hguid", excludeVars, oldVars);
return 0;
}
