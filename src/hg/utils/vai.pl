#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/vai.pl instead.

# Copyright (C) 2016 The Regents of the University of California
# See README in this or parent directory for licensing information.

use Cwd;
use File::Basename;
use Getopt::Long;
use warnings;
use strict;

# Constants
my $doQueryParam = "hgva_startQuery";

# Command line option variables with default values
my $hgVai = '/usr/local/apache/cgi-bin/hgVai';
my $position = '';
my $rsId = 0;
my $udcCache;
my $debug = '';

# GetOpt option configuration for options that don't directly map to hgva_... CGI params,
# and references to the corresponding global variables or handlers:
my %optionConfig = ('hgVai=s' => \$hgVai,
                    'position=s' => \$position,
                    'rsId' => \$rsId,
                    'udcCache=s' => \$udcCache,
                    'dry-run|n|debug' => \$debug,
                    'help|h' => sub { usage(0) },
                   );

# Command line options that map directly to hgva_... CGI params, with their default values
# and descriptions
my %paramOptions = ( geneTrack => ['refGene', '=track',
                                   'Genome Browser track with transcript predictions'],
                     hgvsG => ['on', '=on|off', 'Include HGVS g. (genomic) terms in output (RefSeq transcripts only)'],
                     hgvsCN => ['on', '=on|off', 'Include HGVS c./n. (coding/noncoding) terms in output (RefSeq transcripts only)'],
                     hgvsP => ['on', '=on|off', 'Include HGVS p. (protein) terms in output (RefSeq transcripts only)'],
                     hgvsPAddParens => ['off', '=on|off', 'Add parentheses around HGVS p. predicted changes'],
                     hgvsBreakDelIns => ['off', '=on|off', 'HGVS delins: show "delAGinsTT" instead of "delinsTT"'],
                     include_intergenic => ['on', '=on|off',
                                            'Include intergenic variants in output'],
                     include_upDownstream => ['on', '=on|off',
                                              'Include upstream and downstream variants in output'],
                     include_nmdTranscript => ['on', '=on|off',
                                               'Include variants in NMD transcripts in output'],
                     include_exonLoss => ['on', '=on|off',
                                          'Include exon loss variants in output'],
                     include_utr => ['on', '=on|off',
                                     'Include 3\' and 5\' UTR variants in output'],
                     include_cdsSyn => ['on', '=on|off',
                                        'Include CDS synonymous variants in output'],
                     include_cdsNonSyn => ['on', '=on|off',
                                           'Include CDS non-synonymous variants in output'],
                     include_intron => ['on', '=on|off',
                                        'Include intron variants in output'],
                     include_splice => ['on', '=on|off',
                                        'Include splice site and splice region variants in output'],
                     include_nonCodingExon => ['on', '=on|off',
                                               'Include non-coding exon variants in output'],
                     include_noVariation => ['on', '=on|off',
                                             'Include "variants" with no observed variation in output'],
                     variantLimit => [10000, '=N',
                                      'Maximum number of variants to process'],
                   );


# CGI params and values to be passed to hgVai are collected here:
my %hgVaiParams = ( $doQueryParam => 'go',
                  );

sub usage($) {
  # Show usage and exit with the given status.
  my ($status) = @_;

  print STDERR <<EOF
vai.pl - Invokes hgVai (Variant Annotation Integrator) on a set of variant calls to add functional effect predictions and other data relevant to function.

usage:
    vai.pl [options] db input.(vcf|pgsnp|pgSnp|txt)[.gz] > output.tab

Invokes hgVai (Variant Annotation Integrator) on a set of variant calls to
add functional effect predictions (e.g. does the variant fall within a
regulatory region or part of a gene) and other data relevant to function.

input.(...) must be a file or URL containing either variants formatted as VCF
or pgSnp, or a sequence of dbSNP rs# IDs, optionally compressed by gzip.
Output is printed to stdout.

options:
  --hgVai=/path/to/hgVai          Path to hgVai executable
                                  (default: $hgVai)
  --position=chrX:N-M             Sequence name, start and end of range to query
                                  (default: genome-wide query)
  --rsId                          Attempt to match dbSNP rs# ID with variant
                                  position at the expense of performance.
                                  (default: don't attempt to match dbSNP rs# ID)
  --udcCache=/path/to/udcCache    Path to udc cache, overriding hg.conf setting
                                  (default: use value in hg.conf file)
EOF
  ;
  foreach my $param (sort keys %paramOptions) {
    my ($default, $optArg, $desc) = @{$paramOptions{$param}};
    print STDERR sprintf("  --%-29s $desc\n%34s(default: $default)\n", $param . $optArg, '');
  }
  print STDERR <<EOF
  -n, --dry-run                   Display hgVai command, but don't execute it
  -h, --help                      Display this message
EOF
    ;
  exit $status;
} # usage


sub setParam($$) {
  # Handler for Getopt::Long: set the value (first element in array) of item in %paramOptions
  my ($name, $val) = @_;
  $paramOptions{$name}->[0] = $val;
} # setParam


sub checkArgs() {
  # Parse command line options and make sure $db and $inputFile are given; return $db and $inputFile
  foreach my $param (keys %paramOptions) {
    $optionConfig{"$param=s"} = \&setParam;
  }

  my $ok = GetOptions(%optionConfig);
  if (! $ok) {
    usage(-1);
  }

  if ($position) {
    # Strip commas and whitespace from position; abort if it doesn't look like a position.
    my $trimmedPos = $position;
    $trimmedPos =~ s/,//g;
    $trimmedPos =~ s/\s//g;
    if ($trimmedPos =~ /^[^:]+(:\d+-\d+)?$/) {
      $position = $trimmedPos;
    } else {
      print STDERR "position argument should be like chrX:N-M (sequence name, colon,\n" .
        "starting base offset, ending base offset), not '$position'\n";
      usage(-1);
    }
  }

  my $db = shift @ARGV;
  if (! $db) {
    # No args -- just show usage.
    usage(0);
  }
  if ($db !~ /^\w+$/) {
    print STDERR "First argument must be a database identifier.\n";
    usage(-1);
  }

  my $inputFile = shift @ARGV;
  if (! $inputFile) {
    print STDERR "Missing second argument inputFile";
    usage(-1);
  }
  if (@ARGV) {
    print STDERR "Please provide only one input file.\n";
    usage(-1);
  }
  if ($inputFile !~ /^(https?|ftp):\/\// && ! -r $inputFile) {
    die "Can't open input file '$inputFile' for reading.\n";
  }
  return ($db, $inputFile);
} # checkArgs

sub splitIds($) {
  # Split a line by whitespace or comma
  my ($line) = @_;
  return split(/[\s,]/, $line);
} # splitRsIds

sub looksLikeRsIds($) {
  # Return true if first like of input looks like a (sequence of) rs# IDs.
  my ($firstLine) = @_;
  foreach my $col (splitIds($firstLine)) {
    return 0 if ($col && $col !~ /^rs\d+$/);
  }
  return 1;
} # looksLikeRsIds

sub rsIdStringFromInput($$) {
  # Make a string concatenating all rsIds in input
  my ($inFh, $firstLine) = @_;
  my @ids = splitIds($firstLine);
  while (<$inFh>) {
    if (! /^\s*#/) {
      push @ids, splitIds($_);
    }
  }
  return join('%20', @ids);
} # rsIdStringFromInput


sub autodetectRsId($) {
  # If input begins with # header lines, collect those into a header string.
  # Once we get to the first line of data, see if it looks like rsIds.
  # If it does, set rsId input parameters.  If not, complain and abort.
 # or none of the above (in which case, complain and abort).
  my ($inputFile) = @_;
  my $header;
  my $type;
  open(my $inFh, $inputFile) || die "Can't open inputFile '$inputFile': $!\n";
  my $firstLine = <$inFh>;
  chomp $firstLine; chomp $firstLine;
  while (defined $firstLine && ($firstLine =~ /^\s*#/ || $firstLine =~ /^\s*$/)) {
    $header .= "$firstLine\n";
    $firstLine = <$inFh>;
    chomp $firstLine; chomp $firstLine;
  }
  if (looksLikeRsIds($firstLine)) {
    $hgVaiParams{hgva_variantIds} = rsIdStringFromInput($inFh, $firstLine);
    $hgVaiParams{hgva_variantTrack} = 'hgva_useVariantIds';
  } else {
    print STDERR "Unrecognized input format: please provide input file(s) formatted as\n" .
      "VCF, pgSnp or text with one dbSnp rsNNNN ID per line.\n";
    usage(-1);
  }
  close($inFh);
} # autodetectRsId


sub runHgVai(@) {
  my ($command) = @_;
  my $origDir = getcwd;
  my $status = system($command);
  return $status;
} # runHgVai


# CPAN's URI::Escape is better but then GBiB would need cpan and URI::Escape...
sub uriEscape($) {
  my ($raw) = @_;
  my $escaped = "";
  foreach my $c (split(//, $raw)) {
    if ($c =~ /[\w.~-]/) {
      $escaped .= $c;
    } else {
      $escaped .= sprintf("%%%02X", ord($c));
    }
  }
  return $escaped;
} # uriEscape


####################################### MAIN #######################################


my ($db, $inputFile) = checkArgs();
$hgVaiParams{db} = $db;
if ($position) {
  $hgVaiParams{position} = uriEscape($position);
  $hgVaiParams{hgva_regionType} = 'range';
}
$hgVaiParams{hgva_rsId} = $rsId;

# Attempt to determine type of inputFile by suffix.
my $type = '';
if ($inputFile =~ /\.vcf(\.gz)?$/) {
  $type = 'vcf';
} elsif ($inputFile =~ /(\.pgSnp|.pgsnp|.bed)(\.gz)?$/) {
  $type = 'pgSnp';
}
if ($type) {
  $hgVaiParams{hgva_variantFileOrUrl} = $inputFile;
  $hgVaiParams{hgva_variantTrack} = 'hgva_useVariantFileOrUrl';
} else {
  # Make sure it's a list of rs# IDs.
  #*** CHECK RETURN VAL (and set hgVaiParams here not in autodetect)
  autodetectRsId($inputFile);
}

# Add command line options that map directly to hgva_... CGI params
foreach my $param (keys %paramOptions) {
  my $value = $paramOptions{$param}->[0];
  $hgVaiParams{"hgva_$param"} = uriEscape($value);
}

# If variant file has relative path then convert to absolute.
if ($hgVaiParams{hgva_variantTrack} eq 'hgva_useVariantFileOrUrl' &&
    $hgVaiParams{hgva_variantFileOrUrl} !~ /^(https?|ftp):\/\// &&
    $hgVaiParams{hgva_variantFileOrUrl} =~ /^[^\/]/) {
  $hgVaiParams{hgva_variantFileOrUrl} = getcwd() . '/' . $hgVaiParams{hgva_variantFileOrUrl};
}

# If env var ALL_JOINER_FILE is not already set, try to find an all.joiner to set it to.
my $hgVaiDir = dirname $hgVai;
if (! $ENV{ALL_JOINER_FILE}) {
  if (-e "all.joiner") {
    $ENV{ALL_JOINER_FILE} = "all.joiner";
  } else {
    my $joinerFile =  "$hgVaiDir/all.joiner";
    if (-e $joinerFile) {
      $ENV{ALL_JOINER_FILE} = $joinerFile};
  }
}

# If env var HGDB_CONF is not already set, try to find an hg.conf to set it to.
if (! $ENV{HGDB_CONF}) {
  if (! -e "hg.conf" && ! -e $ENV{HOME}."/.hg.conf") {
    my $hgConf = "$hgVaiDir/hg.conf";
    if (-e $hgConf) {
      $ENV{HGDB_CONF} = $hgConf;
    }
  }
}

# If -udcCache arg is given, set env var UDC_CACHEDIR to its value.
if ($udcCache) {
  $ENV{UDC_CACHEDIR} = $udcCache;
}

my @params = map { "$_=" . $hgVaiParams{$_} } keys %hgVaiParams;
my $command = "$hgVai '" . join('&', @params) . "'";
if ($debug) {
  print "$command\n";
} else {
  delete $ENV{HTTP_COOKIE};
  if (! $ENV{JKTRASH}) {
    # If user has not set JKTRASH, but TMPDIR is defined, then use TMPDIR instead of cwd.
    my $tmpDir = $ENV{TMPDIR};
    if ($tmpDir) {
      $ENV{JKTRASH} = $tmpDir;
    }
  }
  exit runHgVai($command);
}
