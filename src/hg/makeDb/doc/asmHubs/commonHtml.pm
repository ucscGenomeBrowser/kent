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
  qw( otherHubLinks htmlFooter ncbiSciNames )
);

# otherHubLinks: arg one: vgpIndex, arg two: asmHubName, arg three: orderList
# arguments allow decision on customization of the table for different
# types of assembly hubs

sub otherHubLinks($$) {

  my ($vgpIndex, $asmHubName) = @_;

my %asmCounts;	# key is hubName, value is number of assemblies
my @hubList = qw(primates mammals birds fish vertebrate invertebrate plants fungi viral archaea bacteria);
foreach my $hubName (@hubList) {
  my $asmCount = `grep -v "^#" ../${hubName}AsmHub/${hubName}.orderList.tsv | wc -l`;
  chomp $asmCount;
  $asmCounts{$hubName} = $asmCount;
}
my $vgpCount = `grep -h -v "^#" ../vgpAsmHub/vgp.*.orderList.tsv | wc -l`;
chomp $vgpCount;
$asmCounts{'vgp'} = $vgpCount;
my $hprcCount = `grep -c -h -v "^#" ../hprcAsmHub/hprc.orderList.tsv`;
chomp $hprcCount;
$asmCounts{'hprc'} = $hprcCount;
my $brcCount = `grep -c -h -v "^#" ../brcAsmHub/brc.orderList.tsv`;
chomp $brcCount;
$asmCounts{'brc'} = $brcCount;
my $ccgpCount = `grep -c -h -v "^#" ../ccgpAsmHub/ccgp.orderList.tsv`;
chomp $ccgpCount;
$asmCounts{'ccgp'} = $ccgpCount;
my $legacyCount = `grep -c -h -v "^#" ../legacyAsmHub/legacy.orderList.tsv`;
chomp $legacyCount;
$asmCounts{'legacy'} = $legacyCount;

# different table output for VGP index

if ((0 == $vgpIndex)) {
  printf "<p>\n<table style='border: 1px solid black; margin-left:auto; margin-right:auto;'><thead>\n";
  printf "<tr><th colspan=4 style='text-align:center;'>Additional hubs with collections of assemblies</th></tr>\n";
  printf "<tr><th>Collection</th>\n";
  printf "    <th>Hub index pages:</th>\n";
  printf "    <th>Assembly statistics:</th>\n";
  printf "    <th>Track statistics:</th>\n";
  printf "</tr>\n";
  foreach my $hubName (@hubList) {
    if ($hubName =~ m/^vertebrate/) {
      printf "<tr><th>other vertebrates</th>\n";
    } elsif ($hubName =~ m/fish/) {
      printf "<tr><th>Fishes</th>\n";
    } elsif ($hubName =~ m/invertebrate/) {
      printf "<tr><th>Invertebrates</th>\n";
    } elsif ($hubName =~ m/viral/) {
      printf "<tr><th>Viruses</th>\n";
    } elsif ($hubName =~ m/archaea/) {
      printf "<tr><th>Archaea</th>\n";
    } elsif ($hubName =~ m/bacteria/) {
      printf "<tr><th>Bacteria</th>\n";
    } else {
      printf "<tr><th>%s</th>\n", ucfirst($hubName);
    }
    printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", $hubName, $asmCounts{$hubName};
    printf "    <th><a href='../%s/asmStats.html'>assembly stats</a></th>\n", $hubName;
    printf "    <th><a href='../%s/trackData.html'>track stats</a></th>\n", $hubName;
    printf "</tr>\n";
  }
  printf "<tr><th>legacy/superseded</th>\n";
  printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", "legacy", $asmCounts{'legacy'};
  printf "    <th><a href='../legacy/asmStats.html'>assembly stats</a></th>\n";
  printf "    <th><a href='../legacy/trackData.html'>track stats</a></th>\n";
  printf "</tr>\n";

  printf "<tr>
    <th style='text-align:center;' colspan=4>collections below are subsets of the assemblies above</th>
</tr>\n";

  printf "<tr><th>VGP - Vertebrate Genome Project</th>\n";
  printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", "VGP", $asmCounts{'vgp'};
  printf "    <th><a href='../VGP/asmStats.html'>assembly stats</a></th>\n";
  printf "    <th><a href='../VGP/trackData.html'>track stats</a></th>\n";
  printf "</tr>\n";
  printf "<tr><th>CCGP - The California Conservation Genomics Project</th>\n";
  printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", "CCGP", $asmCounts{'ccgp'};
  printf "    <th><a href='../CCGP/asmStats.html'>assembly stats</a></th>\n";
  printf "    <th><a href='../CCGP/trackData.html'>track stats</a></th>\n";
  printf "</tr>\n";
  printf "<tr><th>HPRC - Human Pangenome Reference Consortium</th>\n";
  printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", "HPRC", $asmCounts{'hprc'};
  printf "    <th><a href='../HPRC/asmStats.html'>assembly stats</a></th>\n";
  printf "    <th><a href='../HPRC/trackData.html'>track stats</a></th>\n";
  printf "</tr>\n";
  printf "<tr><th>BRC - BRC Analytics - Bioinformatics Research Center</th>\n";
  printf "    <th style='text-align:right'><a href='../%s/index.html'>%d assemblies</a></th>\n", "BRC", $asmCounts{'brc'};
  printf "    <th><a href='../BRC/asmStats.html'>assembly stats</a></th>\n";
  printf "    <th><a href='../BRC/trackData.html'>track stats</a></th>\n";
  printf "</tr></thead>\n</table>\n</p>\n";
} elsif (1 == $vgpIndex) {
  printf "<p>\n<table style='border: 1px solid black;'><thead>\n";
  printf "<tr><th colspan=5 style='text-align:center;'>Alternate sets of VGP assemblies</th><th style='text-align:center;'>NCBI Refseq</th></tr>\n";
  printf "<tr><th>Index pages:&nbsp;</th>\n";
  printf "<th><a href='index.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAlt.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrio.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacy.html'>legacy/superseded</a></th>\n";
  printf "<th><a href='../primates/index.html'>other NCBI Refseq assemblies</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Assembly statistics:&nbsp;</th>\n";
  printf "<th><a href='asmStats.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAltStats.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrioStats.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacyStats.html'>legacy/superseded</a></th>\n";
  printf "<th><a href='../primates/asmStats.html'>other NCBI Refseq assemblies</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Track statistics:&nbsp;</th>\n";
  printf "<th><a href='trackData.html'>primary assembly</a></th>\n";
  printf "<th><a href='vgpAltData.html'>alternate/haplotype</a></th>\n";
  printf "<th><a href='vgpTrioData.html'>trio mat/pat</a></th>\n";
  printf "<th><a href='vgpLegacyData.html'>legacy/superseded</a></th>\n";
  printf "<th><a href='../primates/trackData.html'>other NCBI Refseq assemblies</a></th>\n";

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
<script src="<!--#echo var="ROOT" -->/js/sorttable.js"></script>
</body></html>
END
}
############################################################################
# ncbiSciNames: given a list of asmIds, return a hash reference with
# key accession (GCA_012345678.1), value [ organismName, taxId ] from the
# genark database assemblySummary* tables.  Those tables follow NCBI
# taxonomy, the assembly_report.txt files keep the organism name and taxId
# from when the assembly was submitted and become obsolete.
# Only GCA/GCF asmIds are looked up, accessions not found are not in the
# hash, and the caller keeps its assembly_report values for those.
sub ncbiSciNames(@) {
  my @asmIds = @_;
  my %table = ( "GCA" => "assemblySummaryGenbank",
                "GCF" => "assemblySummaryRefseq" );
  my %accList;	# key is GCA or GCF, value is list of accessions
  foreach my $asmId (@asmIds) {
    next if ($asmId !~ m/^(GC[AF])_([0-9]+\.[0-9]+)/);
    push @{$accList{$1}}, "$1_$2";
  }
  my %ncbiNames;
  foreach my $gcX (sort keys %accList) {
    my @accs = @{$accList{$gcX}};
    foreach my $t ($table{$gcX}, "$table{$gcX}Historical") {
      for (my $i = 0; $i < scalar(@accs); $i += 1000) {
        my $j = $i + 999;
        $j = $#accs if ($j > $#accs);
        my $inList = join(",", map { "'$_'" } @accs[$i..$j]);
        my $query = "select assemblyAccession,organismName,taxId from $t where assemblyAccession in ($inList)";
        open (my $fh, "-|", "/cluster/bin/x86_64/hgsql", "-N", "-e", $query, "genark") or die "can not run hgsql on genark.$t";
        while (my $line = <$fh>) {
          chomp $line;
          my ($acc, $orgName, $taxId) = split('\t', $line);
          $ncbiNames{$acc} = [ $orgName, $taxId ];
        }
        if (! close ($fh)) {
          printf STDERR "# warning: hgsql query failed on genark.%s, using assembly_report names\n", $t;
        }
      }
    }
  }
  printf STDERR "# %d of %d accessions found in genark assemblySummary tables\n", scalar(keys %ncbiNames), scalar(@asmIds);
  return \%ncbiNames;
}	#	sub ncbiSciNames(@)

############################################################################
