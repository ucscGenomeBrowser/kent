#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/mashDistance.pl instead.

# mashDistance.pl -- standalone divergence check between two assemblies.
# Thin CLI wrapper around AssemblyDivergence.pm, for a human (or another
# script, via backticks) to decide which pairwise pipeline fits a given
# pair before committing to one:
#   doMiniMap2.pl        (minimap2/chain/net) for closely related pairs
#   doBlastzChainNet.pl  (lastz/chain/net)     for everything else

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use AssemblyDivergence;

use vars qw/
    $opt_workDir
    $opt_regenerate
    $opt_help
    /;

sub usage {
  my ($status) = @_;
  print STDERR "
usage: mashDistance.pl aSeq bSeq
Measures mash distance between two sequences and recommends which UCSC
pairwise pipeline fits:
    doMiniMap2.pl        (minimap2/chain/net) for closely related pairs
    doBlastzChainNet.pl  (lastz/chain/net)     for everything else
aSeq and bSeq can each be, in any combination:
  - a plain path to a .2bit, .fa/.fasta, or .fa.gz/.fasta.gz file
  - a GenArk accession, e.g. GCA_060551615.1
  - a UCSC database name, e.g. hg38

A GenArk accession or UCSC db name is resolved entirely under
/hive/data/genomes/ (reachable from every cluster node, unlike /gbdb or
hgcentraltest, which this never touches): its mash sketch is cached
permanently in that assembly's own mashSketch/ directory and reused on
every future call for it.
  - A GenArk accession with no cache yet still works: the source .2bit
    is found automatically in its own build tree (also under
    /hive/data/genomes/), no path needed.
  - A bare UCSC db name with no cache yet does NOT get looked up
    anywhere -- that call just fails.  Sketch it at least once by its
    real path (e.g. /gbdb/hg38/hg38.2bit) first; every call after that
    can use the bare db name and will hit the cache.
Anything else is a one-off sketch, written to -workDir.

Prints, to stdout, lines suitable for parsing by another script:
    mashDistance=<float>
    pipeline=minimap2|lastz
    minimapPreset=asm5|asm10|asm20   (only when pipeline=minimap2)
Any recommendation caveat goes to stderr, not stdout.

options:
    -workDir dir          Fallback scratch dir for mash sketch files, used
                          only for a sequence file that isn't a cacheable
                          GenArk accession (see above).  Default: a fresh
                          directory under /tmp, removed on exit.  Give an
                          existing pipeline's build dir here to reuse/leave
                          behind its mashSketch.{a,b}.msh files.
    -regenerate           Re-sketch and overwrite even if a .msh already
                          exists, cached or fallback.  Use after changing
                          sketch parameters, after a re-built assembly, or
                          some other change to the assembly.
    -help                 This help.
";
  exit $status;
}

# Accept an existing path as-is (unchanged, original behavior).  A bare
# GenArk accession or a bare word (a likely UCSC db name) is passed
# straight through untouched -- AssemblyDivergence::sketch() resolves
# those itself against its /hive/data/genomes/-based cache, never /gbdb
# or hgcentraltest (neither of which a cluster node can reach).  Only
# reject here what's clearly a broken path (contains a '/' but doesn't
# exist), so a typo'd path fails fast with a clear message instead of a
# confusing croak two calls deep.
sub resolveSeqArg {
  my ($arg) = @_;
  return $arg if (-e $arg);
  return $arg if ($arg !~ m{/});
  die "mashDistance.pl: can't find path '$arg'\n";
} # resolveSeqArg

my $ok = GetOptions('workDir=s', 'regenerate', 'help');
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 2);
my ($aSeq, $bSeq) = @ARGV;

$aSeq = &resolveSeqArg($aSeq);
$bSeq = &resolveSeqArg($bSeq);

my $workDir = $opt_workDir;
my $cleanupWorkDir = 0;
if ($workDir) {
  &HgAutomate::mustMkdir($workDir) if (! -d $workDir);
} else {
  $workDir = `mktemp -d -t mashDistance.XXXXXX`;
  chomp $workDir;
  $cleanupWorkDir = 1;
}

my $dist = &AssemblyDivergence::mashDistance($aSeq, $bSeq, $workDir, $opt_regenerate);
my ($pipeline, $preset, $warning) = &AssemblyDivergence::choosePipeline($dist);

print "mashDistance=$dist\n";
print "pipeline=$pipeline\n";
print "minimapPreset=$preset\n" if ($preset);
warn "$warning\n" if ($warning);

system("rm -rf $workDir") if ($cleanupWorkDir);
