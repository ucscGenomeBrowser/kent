#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

sub usage() {
  printf STDERR "usage: asmHubMiniMap2ChainNetTrackDb.pl <buildDir>\n";
  printf STDERR "expecting to find directories: buildDir/trackData/miniMap2.*/\n";
  printf STDERR "where each /miniMap2.*/ directory is one completed minimap2/chainNet\n";
  printf STDERR "and basename(buildDir) is the 'target' sequence name\n";
  exit 255;
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
}

my $buildDir = shift;
my $targetDb = basename($buildDir);
my @tParts = split('_', $targetDb);
my $targetAccession = "$tParts[0]_$tParts[1]";
my @queryList;
my %queryPrio;	# key is queryDb, value is featureBits on chain file
my %commonName;	# key is queryDb, value is common name

open (my $CN, "-|", "hgsql -N -e 'select gcAccession,commonName from genark;' hgcentraltest") or die "can not hgsql -N -e 'select gcAccession,commonName from genark;'";
while (my $line = <$CN>) {
  chomp $line;
  my ($gcX, $comName) = split('\t', $line);
  $comName =~ s/\s\(.*//;
  $commonName{$gcX} = $comName;
}
close ($CN);

open ($CN, "-|", "hgsql -N -e 'select name,organism from dbDb;' hgcentraltest") or die "can not hgsql -N -e 'select name,organism from dbDb;'";
while (my $line = <$CN>) {
  chomp $line;
  my ($gcX, $comName) = split('\t', $line);
  $comName =~ s/\s\(.*//;
  $commonName{$gcX} = "$comName/${gcX}";
}
close ($CN);

`mkdir -p $buildDir/bbi`;

# doMiniMap2.pl names buildDir/trackData/miniMap2.$QDb where $QDb is
# ucfirst() of the actual query db/accession.  Reconstruct the actual
# db name/accession (otherDb) by undoing that ucfirst, except for a
# GenArk accession (already starts with the uppercase 'GC', ucfirst is
# a no-op there).
sub queryDbFromQDb($) {
  my ($qDb) = @_;
  return $qDb if ($qDb =~ m/^GC/);
  return lcfirst($qDb);
}

open (DL, "ls -d $buildDir/trackData/miniMap2.*|") or die "can not list $buildDir/trackData/miniMap2.*";
while (my $mm2Dir = <DL>) {
  chomp $mm2Dir;
  # a -swap run's default swapDir is itself named 'miniMap2.<otherDb>.swap',
  # which also matches this glob alongside its own dateless symlink
  # 'miniMap2.<otherDb>' -- skip that real *.swap work directory so this
  # query genome is only processed once, via its dateless symlink.
  next if ($mm2Dir =~ m/\.swap$/);
  my $Qdb = basename($mm2Dir);
  $Qdb =~ s/miniMap2.//;
  my $queryDb = &queryDbFromQDb($Qdb);
  $queryPrio{$queryDb} = 100;
  my $fbTxt = `ls $buildDir/trackData/miniMap2.${Qdb}/fb.*.chain${Qdb}Link.txt 2> /dev/null`;
  chomp $fbTxt;
  if (-s "${fbTxt}") {
    my $prio = `cut -d' ' -f5 $fbTxt | tr -d '()%'`;
    chomp $prio;
    $queryPrio{$queryDb} = sprintf("%.3f", 100 - $prio);
  }
}
close (DL);

foreach my $qDb ( sort {$queryPrio{$a} <=> $queryPrio{$b}} keys %queryPrio) {
  push @queryList, $qDb;
}

##### begin trackDb output ######
printf "track %sChainNetMiniMap2\n", $targetDb;
printf "compositeTrack on
shortLabel minimap2 Chain/Net
longLabel minimap2 chain alignments to target sequence: %s\n", $targetDb;
printf "subGroup1 view Views chain=Chains liftover=Lift_over\n";
printf "subGroup2 species Assembly";
my $N = 0;
foreach my $queryDb (@queryList) {
  printf " s%03d=%s", $N++, $queryDb;
}
printf "\n";
printf "subGroup3 chainType chain_type c00=chain c01=lift_over\n";
printf "dragAndDrop subTracks\n";
printf "visibility hide
group compGeno
color 0,0,0
altColor 255,255,0
type bed 3
chainLinearGap medium
dimensions dimensionX=chainType dimensionY=species
sortOrder species=+ view=+ chainType=+
configurable on\n";
printf "html html/%s.miniMap2ChainNet\n", $targetDb;

my $QueryDb = "QDb";
my $queryDate = "some date";
my $comName = "some date";
my $queryAsmName = "qAsmName";

$N = 0;
my $headerOut = 0;
foreach my $queryDb (@queryList) {
  $comName = $queryDb;
  $comName = $commonName{$queryDb} if (defined($commonName{$queryDb}));
  $QueryDb = ucfirst($queryDb);
  my $mm2Dir="miniMap2.$QueryDb";
  `rm -f $buildDir/bbi/$targetDb.chainMiniMap2${QueryDb}.bb`;
  `rm -f $buildDir/bbi/$targetDb.chainMiniMap2${QueryDb}Link.bb`;
  `ln -s ../trackData/$mm2Dir/axtChain/chain${QueryDb}.bb  $buildDir/bbi/$targetDb.chainMiniMap2${QueryDb}.bb`;
  `ln -s ../trackData/$mm2Dir/axtChain/chain${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chainMiniMap2${QueryDb}Link.bb`;
  $queryDate = "some date";
  $queryAsmName = "";
  if ( $queryDb !~ m/^GC/ ) {
    $queryDate = `hgsql -N -e 'select description from dbDb where name="$queryDb"' hgcentraltest | sed -e 's/ (.*//;'`;
    chomp $queryDate;
  } else {
    ($queryDate, $queryAsmName) = &HgAutomate::hubDateName($queryDb);
  }
  if (0 == $headerOut) {
    printf "
    track %sChainNetMiniMap2Viewchain
    shortLabel Chains
    view chain
    visibility dense
    parent %sChainNetMiniMap2
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;

    printf "
        track chainMiniMap2%s
        parent %sChainNetMiniMap2Viewchain on", $QueryDb, $targetDb;
  } else {
    printf "
        track chainMiniMap2%s
        parent %sChainNetMiniMap2Viewchain off", $QueryDb, $targetDb;
  }
  printf "
        subGroups view=chain species=s%03d chainType=c00
        shortLabel %s mm2 Chain
        longLabel %s/%s%s (%s) minimap2 Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chainMiniMap2%s.bb
        linkDataUrl bbi/%s.chainMiniMap2%sLink.bb
        otherDb %s
        html html/%s.miniMap2ChainNet
        priority %s
", $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $queryDb, $targetDb,
     $QueryDb, $targetDb, $QueryDb, $queryDb, $targetDb, $queryPrio{$queryDb};
  $N++;
}

$N = 0;
$headerOut = 0;
foreach my $queryDb (@queryList) {
  $comName = $queryDb;
  $comName = $commonName{$queryDb} if (defined($commonName{$queryDb}));
  $QueryDb = ucfirst($queryDb);
  my $mm2Dir="miniMap2.$QueryDb";

  if ( -s "$buildDir/trackData/$mm2Dir/axtChain/chainLiftOver${QueryDb}.bb" ) {
    `rm -f $buildDir/bbi/$targetDb.chainLiftOverMiniMap2${QueryDb}.bb`;
    `rm -f $buildDir/bbi/$targetDb.chainLiftOverMiniMap2${QueryDb}Link.bb`;
    `ln -s ../trackData/$mm2Dir/axtChain/chainLiftOver${QueryDb}.bb  $buildDir/bbi/$targetDb.chainLiftOverMiniMap2${QueryDb}.bb`;
    `ln -s ../trackData/$mm2Dir/axtChain/chainLiftOver${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chainLiftOverMiniMap2${QueryDb}Link.bb`;

    if ( $queryDb !~ m/^GC/ ) {
      $queryDate = `hgsql -N -e 'select description from dbDb where name="$queryDb"' hgcentraltest | sed -e 's/ (.*//;'`;
      chomp $queryDate;
    } else {
      ($queryDate, $queryAsmName) = &HgAutomate::hubDateName($queryDb);
    }

  if (0 == $headerOut) {
    printf "
    track %sChainNetMiniMap2ViewLiftOver
    shortLabel Lift over
    view liftover
    visibility hide
    parent %sChainNetMiniMap2
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;
  }

  printf "
        track chainLiftOverMiniMap2%s
        parent %sChainNetMiniMap2ViewLiftOver off
        subGroups view=liftover species=s%03d chainType=c01
        shortLabel %s mm2 loChain
        longLabel %s/%s%s (%s) minimap2 Lift Over Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chainLiftOverMiniMap2%s.bb
        linkDataUrl bbi/%s.chainLiftOverMiniMap2%sLink.bb
        otherDb %s
        html html/%s.miniMap2ChainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $queryDb, $targetDb,
     $QueryDb, $targetDb, $QueryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }
  $N++;
}

printf "\n" if ($N > 0);
