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

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *db = cartUsualString(cart, "db", hGetDb());
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
cgiMakeTextVar("position", cartUsualString(cart, "position", "SFRS4"), 30);
printf(" pixel width ");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", 610), 4);
printf(" ");
cgiMakeButton("Submit", "Submit");
cartSetString(cart, "db", db);
printf("</CENTER>");

puts(
"<P>A genome position can be specified by the accession number of a sequenced human genomic clone, an mRNA or EST or STS marker, or \n"
"a cytological band, a chromosomal coordinate range, or keywords from the Genbank description of an mRNA. See the <A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<CENTER> <TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr19</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays all of chromosome 19</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>20p13</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region for band p13 on chr 20</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>4q28</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays band q28 on chr 4, gene region determining red hair color</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr3:1-1000000</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays first million bases of chr 3, counting from p arm telomere</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>D16S3046</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region around STS marker D16S3046 from the Genethon/Marshfield maps\n"
"	(open \"STS Markers\" track by clicking to see this marker)</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>AA205474</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of EST with GenBank acc. AA205474 in BRCA1 cancer gene on chr 17\n"
"	(open \"spliced ESTs\" track by clicking to see this EST)</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>ctg13698</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of the fingerprint clone contig ctg13698 from the Wash. U. map\n"
"	(set \"FPC contigs\" track to \"dense\" and refresh to see FPC contigs)</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>AP001670</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of clone with GenBank accession AP001670\n"
"	(open \"coverage\" track by clicking to see this clone)</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AF083811</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Offers 2 coordinate choices on chr 7 and chr 22 for this mitotic checkpoint mRNA</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>PRNP</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD>Offers coordinates choices for nested mRNAs for prion gene, chr 20p</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>homeobox caudal</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs for caudal homeobox genes</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>valyl-tRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs for valyl tRNA synthetases but not unsubmitted tRNA</TD></TR>\n"
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
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author J.E. Evans</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- even though Genbank searches require Evans JE format, GenBank entries themselves use Evans,J.E. internally.\n"
"</TABLE></CENTER>\n"
"\n");

webNewSection("Add Your Own Tracks");

puts(
"<P>You can add your own annotation tracks to the browser using \n"
"the <A HREF=\"../goldenPath/help/customTrack.html\"> \n"
"formats described here</A>.  Annotations can be stored in files or\n"
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
cartWebStart("Human Genome Browser Gateway");
hgGateway();
webEnd();
cartHtmlEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, "hguid", excludeVars);
return 0;
}
