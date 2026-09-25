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
# mashDistance($seqA, $seqB, $workDir, $regenerate) -> distance (float, ~0.0 .. 1.0)
#   $seqA, $seqB: paths to .2bit or fasta(.gz)/fastq(.gz) files -- anything
#                 mash itself or 'twoBitToFa ... stdout | mash sketch -'
#                 can consume.  See sketch() below for what else these
#                 can be (a GenArk accession, a UCSC db name).
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
# Dies if mash/twoBitToFa can't be run or their output can't be parsed.
sub mashDistance {
  my ($seqA, $seqB, $workDir, $regenerate) = @_;
  my $aMsh = &sketch($seqA, $workDir, 'a', $regenerate);
  my $bMsh = &sketch($seqB, $workDir, 'b', $regenerate);
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

# sketch($seq, $workDir, $tag, $regenerate) -> path to a .msh file for $seq.
#   $seq can be an existing sequence file path, a bare GenArk accession
#   (e.g. GCA_939628115.1), or a bare UCSC database name (e.g. hg38).
#
#   The persistent mashSketch/ cache -- and everything needed to find
#   it -- lives entirely under $HgAutomate::clusterData
#   (/hive/data/genomes/), which is reachable from every cluster node.
#   This deliberately never touches /gbdb/ or hgcentraltest: cluster
#   jobs can't reach hgwdev (where hgcentraltest lives) and shouldn't
#   try, and /gbdb isn't mounted on cluster nodes at all.  So:
#     - a GenArk accession or UCSC db name with an already-cached .msh
#       is a pure clusterData filesystem check -- always works, anywhere.
#     - on a cache MISS, this only proceeds if $seq already IS a real,
#       existing sequence file (i.e. the caller resolved it themselves,
#       e.g. to a GenArk build-tree .2bit under clusterData, or handed a
#       literal /gbdb/... path from somewhere that does have it mounted)
#       -- a bare name with no cache and no real file in hand just fails
#       (see the final croak below), it never goes looking for one.
#
#   Cached sketches accumulate once per assembly and are shared by
#   every future comparison involving it, cluster run or standalone
#   mashDistance.pl check alike.  Anything that isn't a GenArk accession
#   or a recognized UCSC db falls back to a one-off sketch named
#   mashSketch.$tag.msh under $workDir, as before.
sub sketch {
  my ($seq, $workDir, $tag, $regenerate) = @_;
  my $prefix;
  my $id;
  my $srcSeq = $seq;   # actual file to sketch from, only needed on a cache miss

  my $accession = &accessionFromPath($seq);
  if ($accession) {
    my $cacheDir = &mashSketchDir($accession);   # pure clusterData 'ls', no /gbdb
    if ($cacheDir) {
      if (! $regenerate && -e "$cacheDir/$accession.msh") {
        return "$cacheDir/$accession.msh";   # cache hit -- done
      }
      if (-e $srcSeq) {
        make_path($cacheDir) if (! -d $cacheDir);
        $prefix = "$cacheDir/$accession";
        $id = $accession;
      } else {
        # Bare accession, no cache yet, no path in hand -- mashSketchDir()
        # only returns a cacheDir when it already found a real build
        # directory, so the real .2bit is right there next to
        # mashSketch/ in that same GenArk build tree (clusterData, not
        # /gbdb): .../<asmId>/mashSketch -> .../<asmId>/<asmId>.2bit
        my $buildDir = $cacheDir;
        $buildDir =~ s#/mashSketch$##;
        my $builtSeq = "$buildDir/" . basename($buildDir) . ".2bit";
        if (-e $builtSeq) {
          make_path($cacheDir) if (! -d $cacheDir);
          $prefix = "$cacheDir/$accession";
          $id = $accession;
          $srcSeq = $builtSeq;
        }
      }
    }
  }

  if (! $prefix) {
    my $db = &seqBaseName($seq);
    if ($db ne '') {
      my $cacheDir = "$HgAutomate::clusterData/$db/mashSketch";
      if (! $regenerate && -e "$cacheDir/$db.msh") {
        return "$cacheDir/$db.msh";   # cache hit -- pure filesystem, done
      }
      # Not cached yet -- only proceed with a source we already have in
      # hand (see the sub's header comment above); never search for one.
      if (-e $srcSeq) {
        make_path($cacheDir) if (! -d $cacheDir);
        $prefix = "$cacheDir/$db";
        $id = $db;
      }
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
    croak "mashDistance: no sequence file to sketch for '$seq' -- not an " .
	  "existing path, a cached/buildable GenArk accession, or a " .
	  "cached/known UCSC db\n"
      if (! -e $srcSeq);
    if ($srcSeq =~ /\.2bit$/) {
      (system("twoBitToFa $srcSeq stdout | mash sketch -k 21 -s 10000 -I $id -o $prefix - 2> /dev/null") == 0)
        || croak "mashDistance: twoBitToFa/mash sketch failed on $srcSeq\n";
    } else {
      (system("mash sketch -k 21 -s 10000 -I $id -o $prefix $srcSeq 2> /dev/null") == 0)
        || croak "mashDistance: mash sketch failed on $srcSeq\n";
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
