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
use AsmHub qw(asmIdToPath);

use vars qw/
    $opt_workDir
    $opt_regenerate
    $opt_dbHost
    $opt_help
    /;

my $dbHost = 'hgwdev';

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
  - a GenArk accession, e.g. GCA_060551615.1, translated to its standard
    location: /gbdb/genark/GCA/060/551/615/GCA_060551615.1/GCA_060551615.1.2bit
  - a UCSC database name, e.g. hg38, translated to /gbdb/hg38/hg38.2bit

For a GenArk assembly (basename starting with GCA_/GCF_ and built under
/hive/data/genomes/asmHubs/), or for a plain UCSC database's own sequence
file (e.g. /gbdb/hg38/hg38.2bit, recognized via an hgcentraltest dbDb
lookup), the mash sketch is cached permanently in that assembly's own
mashSketch/ directory and reused on every future call for it -- no need
to re-sketch the same genome for every pairwise check.  For anything
else, sketches are one-off, written to -workDir.

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
                          to pick up sketches made before -I <accession>
                          labeling was added.
    -dbHost host          Host to run the hgcentraltest dbDb lookup on
                          (see above), default: $dbHost.  hgcentraltest
                          is only reachable from hgwdev -- change this
                          only if you know what you're doing.
    -help                 This help.
";
  exit $status;
}

# Accept an existing path as-is (unchanged, original behavior).  Also
# accept two "just tell me the name" shorthands and translate them to
# the standard, already-built location for that assembly -- everything
# under /gbdb is a fixed, predictable layout, so this is plain path
# construction, no database lookup needed:
#   GCA_060551615.1 -> /gbdb/genark/GCA/060/551/615/GCA_060551615.1/GCA_060551615.1.2bit
#   hg38            -> /gbdb/hg38/hg38.2bit
sub resolveSeqArg {
  my ($arg) = @_;
  return $arg if (-e $arg);
  if ($arg =~ m/^(GC[AF]_\d{9}\.\d+)$/) {
    my $accession = $1;
    my $path = "/gbdb/genark/" . &asmIdToPath($accession) . "/$accession/$accession.2bit";
    return $path if (-e $path);
    die "mashDistance.pl: '$arg' looks like a GenArk accession, but " .
        "$path doesn't exist\n";
  }
  my $path = "/gbdb/$arg/$arg.2bit";
  return $path if (-e $path);
  die "mashDistance.pl: can't find '$arg' as a path, a GenArk accession " .
      "under /gbdb/genark/, or a UCSC database's .2bit under /gbdb/\n";
} # resolveSeqArg

my $ok = GetOptions('workDir=s', 'regenerate', 'dbHost=s', 'help');
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 2);
my ($aSeq, $bSeq) = @ARGV;
$dbHost = $opt_dbHost if ($opt_dbHost);

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

my $dist = &AssemblyDivergence::mashDistance($aSeq, $bSeq, $workDir, $opt_regenerate, $dbHost);
my ($pipeline, $preset, $warning) = &AssemblyDivergence::choosePipeline($dist);

print "mashDistance=$dist\n";
print "pipeline=$pipeline\n";
print "minimapPreset=$preset\n" if ($preset);
warn "$warning\n" if ($warning);

system("rm -rf $workDir") if ($cleanupWorkDir);
