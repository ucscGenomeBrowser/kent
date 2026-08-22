# AsmHub: common routines for building assembly hubs
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/AsmHub.pm instead.

package AsmHub;

use warnings;
use strict;
use Carp;
use File::Basename;
use File::stat;
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

# Look up NCBI's own annotation provider/name/date for an accession from
# the 'genark' database's assemblySummary{Genbank,Refseq} table, falling
# back to the ...Historical variant when the accession isn't in the
# current one (a superseded/suppressed assembly).  Returns ("", "", "")
# if found in neither, so callers always get three defined strings.
sub fetchAnnotationInfo($$) {
  my ($asmType, $accession) = @_;
  my $table = "assemblySummary" . ucfirst($asmType);
  foreach my $t ($table, "${table}Historical") {
    my $result = `hgsql -N -e 'select annotationProvider,annotationName,annotationDate from $t where assemblyAccession="$accession";' genark 2> /dev/null`;
    chomp $result;
    next if ($result eq "");
    my ($provider, $name, $date) = split('\t', $result);
    $provider = "" if ( ! defined $provider || $provider eq "NULL" );
    $name = "" if ( ! defined $name || $name eq "NULL" );
    $date = "" if ( ! defined $date || $date eq "NULL" );
    return ($provider, $name, $date) if ($provider ne "" || $name ne "" || $date ne "");
  }
  return ("", "", "");
} # fetchAnnotationInfo

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

  # bare accession (GCF_937001465.1), not the full asmId with its
  # _assemblyName suffix -- used both for the genark annotation lookup
  # below and for the archived-hub links further down.
  my $accession = "$partNames[0]_$partNames[1]";

  # the .bb's mtime is stamped from the source gff's own mtime (see the
  # 'touch -r $gffFile' step in doNcbiGene.pl), so it doubles as this
  # track's own version/build date -- the same convention archiving uses
  # to name archive/<date>/ directories, which is why this lines up with
  # the dates listed under "Archived versions" below.
  my ($mday,$mon,$year) = (localtime(stat($bbPath)->mtime))[3,4,5];
  my $dataVersion = sprintf("%04d-%02d-%02d", $year+1900, $mon+1, $mday);

  my $totalBases = asmSize($chromSizes);
  if ( ! $totalBases ) {
    printf STDERR "ERROR: asmSize returned no total from %s\n", $chromSizes;
    exit 255;
  }
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
<b>Data version: </b>$dataVersion<br>
<b>Total genome size: </b>$totalBasesText<br>
<b>Gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>% $percentCoverage<br>
</p>

_EOF_

  # Only the live page gets NCBI's own annotation source info -- the
  # genark tables hold NCBI's current-as-of-now metadata for this
  # accession, not a per-archived-snapshot history, so showing it under
  # an "archived version" banner would misleadingly imply it describes
  # that old snapshot specifically.
  if ( ! $archiveNote ) {
    my ($annotationProvider, $annotationName, $annotationDate) =
        fetchAnnotationInfo($asmType, $accession);
    if ($annotationProvider ne "" || $annotationName ne "" || $annotationDate ne "") {
      my $showProvider = $annotationProvider;
      my $showName = $annotationName;
      if ($showName ne "") {
        (my $stripped = $showName) =~ s/^Annotation submitted by\s*//i;
        $stripped =~ s/^\s+|\s+$//g;
        my $providerTrim = $showProvider;
        $providerTrim =~ s/^\s+|\s+$//g;
        # if the name is just "Annotation submitted by <provider>", the
        # name adds nothing over the provider field -- show it only when
        # the two are actually different
        $showName = ($providerTrim ne "" && lc($stripped) eq lc($providerTrim))
                    ? "" : $stripped;
      }
      $html .= "<h2>Annotation source</h2>\n<p>\n";
      $html .= "<b>Annotation provider: </b>$showProvider<br>\n" if ($showProvider ne "");
      $html .= "<b>Annotation name: </b>$showName<br>\n" if ($showName ne "");
      $html .= "<b>Annotation date: </b>$annotationDate<br>\n" if ($annotationDate ne "");
      $html .= "</p>\n\n";
    }
  }

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
      my $hashedPath = asmIdToPath($ncbiAsmId);
      $html .= "<h2>Archived versions</h2>\n<p>\n";
      $html .= "Earlier versions of this track, from before NCBI updated the source annotation, remain available as standalone track hubs:\n";
      $html .= "</p>\n<ul>\n";
      foreach my $version (@versions) {
        my $hubUrl = "https://hgdownload.soe.ucsc.edu/hubs/$hashedPath/$accession/archive/ncbiGene/$version/hub.txt";
        my $url = "/cgi-bin/hgTracks?genome=$accession&hubUrl=$hubUrl";
        $html .= "<li><a href='$url' target='_blank'>$version</a></li>\n";
      }
      $html .= "</ul>\n\n";
    }
  }

  return $html;
} # ncbiGeneDescription

1;
