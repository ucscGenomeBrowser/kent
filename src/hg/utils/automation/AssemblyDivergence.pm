# AssemblyDivergence: measure genome-to-genome divergence with mash and
# use it to help pick a pairwise alignment pipeline / minimap2 preset.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/AssemblyDivergence.pm instead.

# This is a *measurement* helper, not a pipeline in itself: it sketches
# two sequence files with mash, returns a mash distance, and offers a
# threshold-based recommendation of which UCSC pairwise pipeline fits
# that distance:
#   doMiniMap2.pl        (minimap2/chain/net) for closely related pairs
#   doBlastzChainNet.pl  (lastz/chain/net)     for everything else
#
# The thresholds below are a starting point, not calibrated against a
# survey of real pairs -- sanity-check against a few known cases (e.g. a
# T2T trio-binned maternal/paternal pair, and a known cross-species pair
# already run through doBlastzChainNet.pl) before trusting them blindly.

package AssemblyDivergence;

use warnings;
use strict;
use Carp;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use AsmHub qw(accessionFromPath mashSketchDir);
use File::Basename qw(basename);
use File::Path qw(make_path);

# default host to run hgcentraltest dbDb lookups on -- hgcentraltest is
# only reachable from hgwdev, never from a random cluster node, so any
# caller running this code somewhere else (e.g. inside a cluster job)
# MUST pass its own real $dbHost through to mashDistance()/sketch()
# rather than rely on this default.
our $defaultDbHost = 'hgwdev';
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(
    mashDistance
    choosePipeline
    $mashAsm5Max
    $mashAsm10Max
    $mashLastzMin
    $mashWarnMax
    );

# mash distance thresholds used by choosePipeline():
our $mashAsm5Max  = 0.01; # distance <  this -> minimap2 -x asm5  (~<1% div)
our $mashAsm10Max = 0.05; # distance <  this -> minimap2 -x asm10 (~1-5% div)
our $mashWarnMax  = 0.10; # distance above this (but still < mashLastzMin):
                           #   routed to asm20, but flagged as on the high
                           #   end -- worth a second look
our $mashLastzMin = 0.15; # distance >= this -> doBlastzChainNet.pl (lastz);
                           #   minimap2's asm* presets aren't meant for this
                           #   much divergence, and mash distance itself
                           #   gets less reliable as a linear divergence
                           #   proxy out here anyway

#########################################################################
# mashDistance($seqA, $seqB, $workDir, $regenerate, $dbHost) -> distance (float, ~0.0 .. 1.0)
#   $seqA, $seqB: paths to .2bit or fasta(.gz)/fastq(.gz) files -- anything
#                 mash itself or 'twoBitToFa ... stdout | mash sketch -'
#                 can consume.
#   $workDir:     fallback scratch directory to sketch into, used only
#                 when a sequence file can't be resolved to a persistent
#                 cache location (see sketch() below).  Fallback sketch
#                 files are named mashSketch.a.msh / .b.msh and are left
#                 behind (reused on a repeat call with the same
#                 $workDir); it's the caller's job to clean them up.
#   $regenerate:  optional; if true, re-sketch and overwrite even if a
#                 .msh already exists (cached or fallback) -- e.g. after
#                 changing sketch parameters, or to pick up a rebuilt
#                 assembly, or to fix an ID stored under an older format.
#   $dbHost:      optional; host to run the hgcentraltest dbDb lookup on,
#                 for recognizing a plain UCSC database's own .2bit (see
#                 sketch() below).  Defaults to $defaultDbHost ('hgwdev').
#                 hgcentraltest is ONLY reachable from hgwdev -- a caller
#                 running this from any other host (a cluster node, a
#                 workhorse, etc.) must pass the real dbHost explicitly;
#                 HgAutomate::runSSH takes care of actually running the
#                 query there regardless of where this code executes.
# Dies if mash/twoBitToFa can't be run or their output can't be parsed.
sub mashDistance {
  my ($seqA, $seqB, $workDir, $regenerate, $dbHost) = @_;
  $dbHost = $defaultDbHost if (! $dbHost);
  my $aMsh = &sketch($seqA, $workDir, 'a', $regenerate, $dbHost);
  my $bMsh = &sketch($seqB, $workDir, 'b', $regenerate, $dbHost);
  my $mashOut = `mash dist $aMsh $bMsh`;
  chomp $mashOut;
  my @fields = split(/\t/, $mashOut);
  my $dist = $fields[2];
  if (! defined $dist || $dist !~ /^[0-9.eE+-]+$/) {
    croak "mashDistance: couldn't parse 'mash dist $aMsh $bMsh' output: '$mashOut'\n";
  }
  return $dist;
} # mashDistance

# basename of $path with a trailing .2bit / .fa(.gz) / .fasta(.gz)
# stripped off -- used only to get a candidate UCSC db name to test.
sub seqBaseName {
  my ($path) = @_;
  my $base = basename($path);
  $base =~ s/\.(2bit|fa|fasta)(\.gz)?$//;
  return $base;
}

# sketch($seq, $workDir, $tag, $regenerate, $dbHost) -> path to a .msh file for $seq.
#   If $seq's basename identifies a GenArk accession (e.g.
#   GCA_939628115.1_Tfree1.0.2bit) that has an actual build directory
#   under /hive/data/genomes/asmHubs/, or is a plain UCSC database's own
#   .2bit (e.g. /gbdb/hg38/hg38.2bit, confirmed via an hgcentraltest
#   dbDb lookup on $dbHost), the sketch is written into (or, if already
#   present, reused from) that assembly's own mashSketch/ directory --
#   so it accumulates once per assembly and is shared by every future
#   comparison involving it, cluster run or standalone mashDistance.pl
#   check alike, instead of being rebuilt from scratch every time.
#   Otherwise falls back to a one-off sketch named mashSketch.$tag.msh
#   under $workDir, as before.
sub sketch {
  my ($seq, $workDir, $tag, $regenerate, $dbHost) = @_;
  $dbHost = $defaultDbHost if (! $dbHost);
  my $prefix;
  my $id;
  my $accession = &accessionFromPath($seq);
  if ($accession) {
    my $cacheDir = &mashSketchDir($accession);
    if ($cacheDir) {
      make_path($cacheDir) if (! -d $cacheDir);
      $prefix = "$cacheDir/$accession";
      $id = $accession;
    }
  }
  if (! $prefix) {
    # Not a GenArk accession -- see if it's a plain UCSC database's own
    # sequence file instead.  This dbDb lookup always runs on $dbHost
    # (hgcentraltest is only reachable from hgwdev), never on whatever
    # host happens to be running this Perl process.
    my $db = &seqBaseName($seq);
    if ($db ne '' && &HgAutomate::isUcscDb($dbHost, $db)) {
      my $cacheDir = "$HgAutomate::clusterData/$db/mashSketch";
      make_path($cacheDir) if (! -d $cacheDir);
      $prefix = "$cacheDir/$db";
      $id = $db;
    }
  }
  if (! $prefix) {
    $prefix = "$workDir/mashSketch.$tag";
    $id = basename($seq);
  }
  # Label the sketch with something meaningful (the accession or db name
  # when we have one) instead of letting mash default to the first
  # sequence's own ID (e.g. an arbitrary scaffold accession like
  # NC_007416.3) -- 'mash info' and 'mash dist' output are both far more
  # legible this way.
  my $mshFile = "$prefix.msh";
  if ($regenerate || ! -e $mshFile) {
    if ($seq =~ /\.2bit$/) {
      (system("twoBitToFa $seq stdout | mash sketch -k 21 -s 10000 -I $id -o $prefix - 2> /dev/null") == 0)
        || croak "mashDistance: twoBitToFa/mash sketch failed on $seq\n";
    } else {
      (system("mash sketch -k 21 -s 10000 -I $id -o $prefix $seq 2> /dev/null") == 0)
        || croak "mashDistance: mash sketch failed on $seq\n";
    }
  }
  return $mshFile;
} # sketch

#########################################################################
# choosePipeline($distance) -> ($pipeline, $minimapPreset, $warning)
#   $pipeline:      'minimap2' or 'lastz'
#   $minimapPreset: 'asm5'/'asm10'/'asm20' when $pipeline eq 'minimap2',
#                   else undef
#   $warning:       a string worth relaying to the user, or '' if nothing
#                   notable
sub choosePipeline {
  my ($dist) = @_;
  if ($dist >= $mashLastzMin) {
    return ('lastz', undef,
	"mash distance $dist >= $mashLastzMin: likely too diverged for " .
	"minimap2's asm* presets -- use doBlastzChainNet.pl instead.");
  }
  return ('minimap2', 'asm5', '') if ($dist < $mashAsm5Max);
  return ('minimap2', 'asm10', '') if ($dist < $mashAsm10Max);
  my $warning = ($dist > $mashWarnMax) ?
    "mash distance $dist is on the high end of what minimap2 asm20 " .
    "expects for a same-species pair -- doBlastzChainNet.pl may fit better."
    : '';
  return ('minimap2', 'asm20', $warning);
} # choosePipeline

1;
