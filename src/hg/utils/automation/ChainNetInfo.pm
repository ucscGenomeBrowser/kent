# ChainNetInfo: common routines to read the results of a lastz
#   (doBlastzChainNet.pl) or minimap2 (doMiniMap2.pl) chain/net build
#   directory, for the chain/net track description page scripts.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/ChainNetInfo.pm instead.

package ChainNetInfo;

use warnings;
use strict;
use Carp;
use Cwd qw(abs_path);
use File::Basename;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

# This is a listing of the public methods exported by this module:
@EXPORT_OK = (
    qw( htmlEscape fbPercent findFbPercent readVersionTxt readLastzParams
        accessionOrDb findAlignments readCladeFile
      ),
);

# accession only for GenArk assemblies (GCA_123456789.1_asmName ->
# GCA_123456789.1), that is what the build directory names and the
# fb.<target>.*.txt file names use.  UCSC database names unchanged.
sub accessionOrDb($) {
  my ($asm) = @_;
  return $1 if ($asm =~ m/^(GC[AF]_[0-9]{9}\.[0-9]+)/);
  return $asm;
}

# escape the HTML special characters in a string
sub htmlEscape($) {
  my ($str) = @_;
  $str =~ s/&/&amp;/g;
  $str =~ s/</&lt;/g;
  $str =~ s/>/&gt;/g;
  return $str;
}

# given an fb.*.txt file, featureBits style output:
#   2943421005 bases of 3099734149 (94.957%) in intersection
# return the percent number: 94.957
# or the empty string if the file is not there or not recognized
sub fbPercent($) {
  my ($fbFile) = @_;
  return "" if (! -s "$fbFile");
  open (my $fh, "<", $fbFile) or return "";
  my $line = <$fh>;
  close ($fh);
  return "" if (! defined($line));
  return $1 if ($line =~ m/\(\s*([0-9.]+)\s*%\s*\)/);
  return "";
}

# given a build directory, the fb file prefix (fb.<targetDb>.) and a
# list of possible file name suffixes, return the percent from the first
# one found, or the empty string when none of them are there.  Some
# measurements have been named more than one way over the years, e.g.
# chainLiftOver<Query>Link.txt (lastz) vs. chainLiftOver<Query>.txt
# (doMiniMap2.pl).
sub findFbPercent($$@) {
  my ($dir, $fbPrefix, @suffixes) = @_;
  foreach my $suffix (@suffixes) {
    my $fbFile = "$dir/$fbPrefix$suffix";
    return &fbPercent($fbFile) if (-s "$fbFile");
  }
  return "";
}

# doMiniMap2.pl records the mash distance, chosen preset, minimap2 version
# and the minimap2 command line in version.txt in its work directory, e.g.:
#   mashDistance=0.00159554 minimapPreset=asm5
#   minimap2 2.31-r1302
#   minimap2 -cx asm5 -N 5 -p 0.8 -t 8 target.fa query.fa > out.paf
# returns a hash reference with keys: version command mashDistance
#   minimapPreset (any of them may be the empty string), or undef when
#   there is no version.txt
sub readVersionTxt($) {
  my ($mm2Dir) = @_;
  return undef if (! -s "$mm2Dir/version.txt");
  my %info = ( 'version' => "", 'command' => "", 'mashDistance' => "",
               'minimapPreset' => "" );
  open (my $vfh, "<", "$mm2Dir/version.txt") or return undef;
  while (my $line = <$vfh>) {
    chomp $line;
    $line =~ s/^\s+//;
    $line =~ s/\s+$//;
    next if (length($line) < 1);
    if ($line =~ m/^minimap2\s+-/) {
      $info{'command'} = $line;
    } elsif ($line =~ m/^minimap2\s+(\S+)$/) {
      $info{'version'} = $1;
    } else {
      foreach my $keyVal (split('\s+', $line)) {
        my ($key, $val) = split('=', $keyVal, 2);
        next if (!defined($val));
        $info{'mashDistance'} = $val if ($key eq "mashDistance");
        $info{'minimapPreset'} = $val if ($key eq "minimapPreset");
      }
    }
  }
  close ($vfh);
  return \%info;
}

# a doBlastzChainNet.pl build directory has its DEF file, and the
# axtChain/run/chain.csh that shows the axtChain options that were used.
# A -swap directory only has a DEF symlink back to the primary build
# directory, chain.csh is found there.
# returns a hash reference with keys:
#   lastz - the lastz binary from BLASTZ=, or "" when DEF did not say
#   lastzVersion - version number from that path, if it has one
#   scoring - array reference of [key, value] for the BLASTZ_* settings
#   minScore linearGap - from the axtChain command in chain.csh, or ""
# or undef when there is no DEF file
sub readLastzParams($) {
  my ($dir) = @_;
  my $def = "$dir/DEF";
  return undef if (! -s "$def");
  my $realDef = abs_path($def);
  my $primaryDir = defined($realDef) ? dirname($realDef) : $dir;
  my %info = ( 'lastz' => "", 'lastzVersion' => "", 'scoring' => [],
               'minScore' => "", 'linearGap' => "" );
  open (my $dfh, "<", $def) or return undef;
  while (my $line = <$dfh>) {
    chomp $line;
    next if ($line =~ m/^\s*#/);
    next if ($line !~ m/^\s*([A-Za-z0-9_]+)\s*=\s*(\S.*?)\s*$/);
    my ($key, $val) = ($1, $2);
    if ($key eq "BLASTZ") {
      $info{'lastz'} = $val;
      if ($val =~ m/lastz-distrib-([0-9][0-9.]*[0-9])/ ||
          $val =~ m/lastz-([0-9][0-9.]*[0-9])/) {
        $info{'lastzVersion'} = $1;
      }
    } elsif ($key =~ m/^BLASTZ_(.+)$/) {
      push @{$info{'scoring'}}, [$1, $val];
    }
  }
  close ($dfh);
  my $chainCsh = "$primaryDir/axtChain/run/chain.csh";
  if (-s "$chainCsh") {
    open (my $cfh, "<", $chainCsh) or return \%info;
    while (my $line = <$cfh>) {
      next if ($line !~ m/axtChain/);
      $info{'minScore'} = $1 if ($line =~ m/-minScore=(\S+)/);
      $info{'linearGap'} = $1 if ($line =~ m/-linearGap=(\S+)/);
    }
    close ($cfh);
  }
  return \%info;
}

# findAlignments($tRoot, $tId, $query, $methodOpt)
#   find the lastz and minimap2 chain/net builds of target $tId to one
#   query, in $tRoot (/hive/data/genomes/<db>/bed or GenArk trackData).
#   Only these exact names are checked, the directory is never listed:
#     lastz.<q>  blastz.<q>.swap  miniMap2.<q>  miniMap2.<Q>  miniMap2.<q>.swap
#   The dateless symlinks are named with the actual query db/accession, an
#   older miniMap2 build may still have the ucfirst() name.  A -swap work
#   directory is checked on its own since both directions may have been
#   run as a primary build.  When none of those leads to a primary build,
#   the newest finished dated build directory (lastzQuery.yyyy-mm-dd,
#   miniMap2Query.yyyy-mm-dd) is used.
#   $methodOpt is "all", "lastz" or "miniMap2".
# returns a list of hash references, one per build directory found, with
#   keys: qId Q method isSwap dir
#         chain syntenic rBest liftOver quickLift (percent, or "")
#         params (readLastzParams()/readVersionTxt() result, or undef)
sub findAlignments($$$$) {
  my ($tRoot, $tId, $query, $methodOpt) = @_;
  my $qId = &accessionOrDb($query);
  my $Q = ucfirst($qId);
  my @candidates = (
      [ "lastz", "$tRoot/lastz.$qId" ],
      [ "lastz", "$tRoot/blastz.$qId.swap" ],
      [ "miniMap2", "$tRoot/miniMap2.$qId" ],
      [ "miniMap2", "$tRoot/miniMap2.$Q" ],
      [ "miniMap2", "$tRoot/miniMap2.$qId.swap" ],
  );
  @candidates = grep { $_->[0] eq $methodOpt } @candidates
     if ($methodOpt ne "all");
  foreach my $method ("lastz", "miniMap2") {
    next if ($methodOpt ne "all" && $method ne $methodOpt);
    my $havePrimary = 0;
    foreach my $candidate (@candidates) {
      my ($cMethod, $path) = @$candidate;
      next if ($cMethod ne $method || ! -d "$path");
      my $realDir = abs_path($path);
      $havePrimary = 1 if (defined($realDir) && $realDir !~ m/\.swap$/);
    }
    next if ($havePrimary);
    my @dated = sort glob("$tRoot/${method}${Q}.[0-9]*");
    foreach my $datedDir (reverse @dated) {
      next if ($datedDir =~ m/\.swap$/);
      next if (! -s "$datedDir/fb.$tId.chain${Q}Link.txt");
      push @candidates, [ $method, $datedDir ];
      last;
    }
  }
  my @rows;
  my %seenDir;
  my $fbPrefix = "fb.$tId.";
  foreach my $candidate (@candidates) {
    my ($method, $path) = @$candidate;
    next if (! -d "$path");
    my $realDir = abs_path($path);
    next if (! defined($realDir) || defined($seenDir{$realDir}));
    $seenDir{$realDir} = 1;
    my %row = (
      'qId' => $qId,
      'Q' => $Q,
      'method' => $method,
      'isSwap' => ($realDir =~ m/\.swap$/) ? 1 : 0,
      'dir' => $realDir,
      'chain' => &findFbPercent($realDir, $fbPrefix, "chain${Q}Link.txt"),
      'syntenic' => &findFbPercent($realDir, $fbPrefix, "chainSyn${Q}Link.txt"),
      'rBest' => &findFbPercent($realDir, $fbPrefix, "chainRBest.${Q}.txt"),
      'liftOver' => &findFbPercent($realDir, $fbPrefix,
                       "chainLiftOver${Q}Link.txt", "chainLiftOver${Q}.txt"),
      'quickLift' => &findFbPercent($realDir, $fbPrefix, "quick${Q}Link.txt"),
    );
    if ($method eq "miniMap2") {
      $row{'params'} = &readVersionTxt($realDir);
    } else {
      $row{'params'} = &readLastzParams($realDir);
    }
    push @rows, \%row;
  }
  return @rows;
}

# readCladeFile($file) - the clade/order file made by chainNetCladeOrder.pl,
#   the same format as the older hand made files for chainNetComposite.pl:
#     query  clade  chainOn  netOn  [any more columns are ignored]
#   whitespace separated, # comment lines and blank lines are ignored,
#   chainOn and netOn are 'on' or 'off', the default visibility of that
#   query's chain and net subtracks.
# returns a list of hash references, in file order, with keys:
#   query clade chainOn netOn  (chainOn and netOn are 1 or 0)
#   and from a tab separated chainNetCladeOrder.pl file, when they are
#   there: sharedDepth mashDistance chainCoverage scientificName
#   ('-' in the file is returned as "")
sub readCladeFile($) {
  my ($file) = @_;
  open (my $fh, "<", $file) or croak "can not read clade file '$file'\n";
  my @entries;
  my %seen;
  while (my $line = <$fh>) {
    chomp $line;
    next if ($line =~ m/^\s*#/ || $line =~ m/^\s*$/);
    $line =~ s/^\s+//;
    my ($query, $clade, $chainOn, $netOn) = split('\s+', $line);
    if (! defined($netOn)) {
      croak "clade file '$file' needs four columns: query clade chainOn netOn, found: '$line'\n";
    }
    if ($chainOn !~ m/^(on|off)$/ || $netOn !~ m/^(on|off)$/) {
      croak "clade file '$file' chainOn and netOn must be 'on' or 'off', found: '$line'\n";
    }
    my $qId = &accessionOrDb($query);
    next if (defined($seen{$qId}));
    $seen{$qId} = 1;
    my %entry = ( 'query' => $qId, 'clade' => $clade,
       'chainOn' => ($chainOn eq "on") ? 1 : 0,
       'netOn' => ($netOn eq "on") ? 1 : 0 );
    # the information columns, the scientific name has spaces, only
    # in a tab separated file
    if ($line =~ m/\t/) {
      my @fields = split('\t', $line);
      my @infoKeys = ('sharedDepth', 'mashDistance', 'chainCoverage', 'scientificName');
      for (my $i = 0; $i < scalar(@infoKeys); ++$i) {
        my $value = $fields[$i + 4];
        next if (! defined($value));
        $value = "" if ($value eq "-");
        $entry{$infoKeys[$i]} = $value;
      }
    }
    push @entries, \%entry;
  }
  close ($fh);
  croak "clade file '$file' has no entries\n" if (! scalar(@entries));
  return @entries;
}

1; # for require
