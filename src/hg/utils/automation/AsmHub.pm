# AsmHub: common routines for building assembly hubs
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/AsmHub.pm instead.

package AsmHub;

use warnings;
use strict;
use Carp;
use File::Basename;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

# This is a listing of the public methods and variables (which should be
# treated as constants) exported by this module:
@EXPORT_OK = (
    # Support for common command line options:
    qw( commify asmSize ncbiGeneDescription
      ),
);

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters, input is a large number with no commas:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

# given an asmId.chrom.sizes, return the assembly size from the
# sum of column 2:
sub asmSize($) {
    my ($chromSizes) = @_;
    my $asmSize=`/cluster/bin/x86_64/ave -col=2 $chromSizes | grep "total" | sed -e 's/total //; s/.000000//;'`;
    chomp $asmSize;
    return $asmSize;
}

# given a fully qualified asmId, e.g.: GCA_018504075.1_HG02723.alt.pat.f1_v2
# return the string representating the path: GCA/018/504/075
sub asmIdToPath($) {
  my ($asmId) = @_;
  my $gcX = substr($asmId, 0, 3);
  my $d0 = substr($asmId, 4, 3);
  my $d1 = substr($asmId, 7, 3);
  my $d2 = substr($asmId, 10, 3);
  my $ret = sprintf("%s/%s/%s/%s", $gcX, $d0, $d1, $d2);
  return $ret;
}

# Build the 'ncbiGene' track description HTML body.  Used both by
# asmHubNcbiGene.pl for the live track, and by doNcbiGene.pl for the
# one-track archive hubs it writes under trackData/ncbiGene/archive/ --
# which is why every path is taken explicitly rather than assumed from
# the usual trackData/ layout.  $archiveNote, if given, is printed as a
# lead-in banner ahead of the normal description (used for the archived
# copies; the live page passes it undef).
sub ncbiGeneDescription($$$$$;$) {
  my ($bbPath, $statsPath, $chromSizes, $namesFile, $ncbiAsmId, $archiveNote) = @_;

  if ( ! -s $bbPath ) {
    printf STDERR "ERROR: can not find %s file\n", $bbPath;
    exit 255;
  }

  my @partNames = split('_', $ncbiAsmId);
  my $ftpDirPath = sprintf("%s/%s/%s/%s/%s", $partNames[0],
     substr($partNames[1],0,3), substr($partNames[1],3,3),
     substr($partNames[1],6,3), $ncbiAsmId);
  my $asmType = ($partNames[0] =~ m/GCA/) ? "genbank" : "refseq";

  my $totalBases = asmSize($chromSizes);
  my $geneStats = `cat $statsPath | awk '{printf "%d\\n", \$2}' | xargs echo`;
  chomp $geneStats;
  my ($itemCount, $basesCovered) = split('\s+', $geneStats);
  my $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
  $itemCount = commify($itemCount);
  $basesCovered = commify($basesCovered);
  my $totalBasesText = commify($totalBases);

  my $em = "<em>";
  my $noEm = "</em>";
  my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
  chomp $assemblyDate;
  my $organism = `grep -v "^#" $namesFile | cut -f5`;
  chomp $organism;

  my $html = "";
  $html .= "<p><b>$archiveNote</b></p>\n\n" if ($archiveNote);

  if ( "${asmType}" eq "refseq" ) {
    $html .= <<_EOF_;
<h2>Description</h2>
<p>
The NCBI Gene track for the $assemblyDate $em${organism}$noEm/$ncbiAsmId
genome assembly is constructed from the gff file <b>${ncbiAsmId}_genomic.gff.gz</b>
supplied with the genome assembly at the FTP location:<br>
<a href='https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/' target='_blank'>https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/</a>
</p>

_EOF_
  } else {
    $html .= <<_EOF_;
<h2>Description</h2>
<p>
The Gene model track for the $assemblyDate $em${organism}$noEm/$ncbiAsmId
genome assembly is constructed from the gff file <b>${ncbiAsmId}_genomic.gff.gz</b>
supplied with the genome assembly at the FTP location:<br>
<a href='https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/' target='_blank'>https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/</a>
</p>
<p>
The gene models were constructed by the submitter of the assembly to the
NCBI assembly release system.
</p>

_EOF_
  }

  $html .= <<_EOF_;
<h2>Track statistics summary</h2>
<p>
<b>Total genome size: </b>$totalBasesText<br>
<b>Gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>% $percentCoverage<br>
</p>

_EOF_

  # Only the live page advertises archives (an archived page's own
  # description already carries $archiveNote and has nothing further
  # under it to list).  Archived copies are published to hgdownload at
  # https://hgdownload.soe.ucsc.edu/hubs/<hashedPath>/<accession>/archive/ncbiGene/<version>/hub.txt
  # -- one self-contained hub.txt per version -- regardless of where they
  # sit in the local build tree, so we just need the version list.
  if ( ! $archiveNote ) {
    my $archiveBase = dirname($bbPath) . "/archive";
    my @versions = ();
    if ( -d $archiveBase ) {
      opendir(my $dh, $archiveBase);
      if ($dh) {
        @versions = sort { $b cmp $a }
                    grep { -d "$archiveBase/$_" && $_ !~ m/^\./ } readdir($dh);
        closedir($dh);
      }
    }
    if (@versions) {
      my @parts = split('_', $ncbiAsmId);
      my $accession = "$parts[0]_$parts[1]";
      my $hashedPath = asmIdToPath($ncbiAsmId);
      $html .= "<h2>Archived versions</h2>\n<p>\n";
      $html .= "Earlier versions of this track, from before NCBI updated the source annotation, remain available as standalone track hubs:\n";
      $html .= "</p>\n<ul>\n";
      foreach my $version (@versions) {
        my $url = "https://hgdownload.soe.ucsc.edu/hubs/$hashedPath/$accession/archive/ncbiGene/$version/hub.txt";
        $html .= "<li><a href='$url' target='_blank'>$version</a></li>\n";
      }
      $html .= "</ul>\n\n";
    }
  }

  return $html;
} # ncbiGeneDescription

1;
