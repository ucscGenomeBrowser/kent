package commonHtml;

# helper functions for common HTML output features

use warnings;
use strict;
use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

# This is a listing of the public methods and variables (which should be
# treated as constants) exported by this module:
@EXPORT_OK = (
  qw( otherHubLinks htmlFooter )
);

# otherHubLinks: arg one: vgpIndex, arg two: asmHubName, arg three: orderList
# arguments allow decision on customization of the table for different
# types of assembly hubs

sub otherHubLinks($$) {

  my ($vgpIndex, $asmHubName) = @_;

# different table output for VGP index

if ((0 == $vgpIndex) && ($asmHubName ne "viral")) {
  printf "<p>\n<table border='1'><thead>\n";
  printf "<tr><th colspan=6 style='text-align:center;'>Additional hubs with collections of assemblies</th></tr>\n";
  printf "<tr><th>Assembly hubs index pages:&nbsp;</th>\n";
  printf "<th><a href='../primates/index.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/index.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/index.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/index.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/index.html'>other vertebrates</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Hubs assembly statistics:&nbsp;</th>\n";
  printf "<th><a href='../primates/asmStats.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/asmStats.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/asmStats.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/asmStats.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/asmStats.html'>other vertebrates</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Hubs track statistics:&nbsp;</th>\n";
  printf "<th><a href='../primates/trackData.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/trackData.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/trackData.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/trackData.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/trackData.html'>other vertebrates</a></th>\n";

  printf "</tr></thead>\n</table>\n</p>\n";
} elsif (1 == $vgpIndex) {
  printf "<p>\n<table border='1'><thead>\n";
  printf "<tr><th colspan=5 style='text-align:center;'>Alternate sets of VGP assemblies</th></tr>\n";
  printf "<tr><th>Index pages:&nbsp;</th>\n";
  printf "<th><a href='index.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAlt.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrio.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacy.html'>legacy/superseded</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Assembly statistics:&nbsp;</th>\n";
  printf "<th><a href='asmStats.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAltStats.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrioStats.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacyStats.html'>legacy/superseded</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Track statistics:&nbsp;</th>\n";
  printf "<th><a href='trackData.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAltData.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrioData.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacyData.html'>legacy/superseded</a></th>\n";

  printf "</tr><tr>\n";
  printf "</tr></thead>\n</table>\n</p>\n";
}

}	#	sub otherHubLinks($$)

############################################################################
# common output at the bottom of an html index page
sub htmlFooter() {

print <<"END"
</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual="\$ROOT/inc/gbFooterHardcoded.html"-->
<script type="text/javascript" src="/js/sorttable.js"></script>
</body></html>
END
}
############################################################################
