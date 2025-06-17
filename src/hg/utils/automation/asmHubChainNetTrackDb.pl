#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

sub usage() {
  printf STDERR "usage: asmHubChainNetTrackDb.pl <buildDir>\n";
  printf STDERR "expecting to find directories: buildDir/trackData/lastz.*/\n";
  printf STDERR "where each /lastz.*/ directory is one completed lastz/chainNet\n";
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
`mkdir -p $buildDir/liftOver`;

open (DL, "ls -d $buildDir/trackData/lastz.*|") or die "can not list $buildDir/trackData/lastz.*";
while (my $lastzDir = <DL>) {
  chomp $lastzDir;
  my $queryDb = basename($lastzDir);
  $queryDb =~ s/lastz.//;
  my $Qdb = ucfirst($queryDb);
#  push @queryList, $queryDb;
  $queryPrio{$queryDb} = 100;
  my $fbTxt = `ls $buildDir/trackData/lastz.${queryDb}/fb.${targetAccession}.chain${Qdb}Link.txt 2> /dev/null`;
  chomp $fbTxt;
  if (-s "${fbTxt}") {
    my $prio = `cut -d' ' -f5 $fbTxt | tr -d '()%'`;
    chomp $prio;
#    $queryPrio{$queryDb} = sprintf("%d", int((100 - $prio)+0.5));
    $queryPrio{$queryDb} = sprintf("%.3f", 100 - $prio);
  }
}
close (DL);

foreach my $qDb ( sort {$queryPrio{$a} <=> $queryPrio{$b}} keys %queryPrio) {
  push @queryList, $qDb;
}

# foreach my $queryDb (@queryList) {
#    printf STDERR "%s vs. %s\n", $queryDb, $targetDb;
# }

##### begin trackDb output ######
printf "track %sChainNet\n", $targetDb;
printf "compositeTrack on
shortLabel Chain/Net
longLabel Chain alignments to target sequence: %s\n", $targetDb;
printf "subGroup1 view Views chain=Chains synten=Syntenic rbest=Reciprocal_best liftover=Lift_over align=Alignment\n";
printf "subGroup2 species Assembly";
my $N = 0;
foreach my $queryDb (@queryList) {
  printf " s%03d=%s", $N++, $queryDb;
}
printf "\n";
printf "subGroup3 chainType chain_type c00=chain c01=syntenic c02=reciprocal_best c03=lift_over c04=chain_align c05=syntenic_align c06=reciprocal_align c07=lift_over_align\n";
printf "dragAndDrop subTracks\n";
printf "visibility hide
group compGeno
color 0,0,0
altColor 255,255,0
type bed 3
chainLinearGap loose
chainMinScore 5000
dimensions dimensionX=chainType dimensionY=species
sortOrder species=+ view=+ chainType=+
configurable on\n";
printf "html html/%s.chainNet\n", $targetDb;

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
  my $overChain="${targetAccession}.${queryDb}.over.chain.gz";
  my $overToChain="${targetAccession}To${QueryDb}.over.chain.gz";
  my $lastzDir="lastz.$queryDb";
  `rm -f $buildDir/bbi/$targetDb.chain${QueryDb}.bb`;
  `rm -f $buildDir/bbi/$targetDb.chain${QueryDb}Link.bb`;
  `rm -f $buildDir/liftOver/${overToChain}`;
  if ( -s "$buildDir/trackData/$lastzDir/axtChain/${overChain}" ) {
     `ln -s ../trackData/$lastzDir/axtChain/${overChain} $buildDir/liftOver/${overToChain}`;
  } else {
     printf STDERR "# NOT FOUND: '%s'\n", "$buildDir/trackData/$lastzDir/axtChain/${overChain}";
  }
  `ln -s ../trackData/lastz.$queryDb/axtChain/chain${QueryDb}.bb  $buildDir/bbi/$targetDb.chain${QueryDb}.bb`;
  `ln -s ../trackData/lastz.$queryDb/axtChain/chain${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chain${QueryDb}Link.bb`;
  $queryDate = "some date";
  if ( $queryDb !~ m/^GC/ ) {
    $queryDate = `hgsql -N -e 'select description from dbDb where name="$queryDb"' hgcentraltest | sed -e 's/ (.*//;'`;
    chomp $queryDate;
  } else {
    ($queryDate, $queryAsmName) = &HgAutomate::hubDateName($queryDb);
  }
  if (0 == $headerOut) {
    printf "
    track %sChainNetViewchain
    shortLabel Chains
    view chain
    visibility dense
    parent %sChainNet
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;

    printf "
        track chain%s
        parent %sChainNetViewchain on", $QueryDb, $targetDb;
  } else {
    printf "
        track chain%s
        parent %sChainNetViewchain off", $QueryDb, $targetDb;
  }
  printf "
        subGroups view=chain species=s%03d chainType=c00
        shortLabel %s Chain
        longLabel %s/%s%s (%s) Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chain%s.bb
        linkDataUrl bbi/%s.chain%sLink.bb
        otherDb %s
        html html/%s.chainNet
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
  if ( -s "$buildDir/trackData/lastz.$queryDb/axtChain/chainSyn${QueryDb}.bb" ) {
    `rm -f $buildDir/bbi/$targetDb.chainSyn${QueryDb}.bb`;
    `rm -f $buildDir/bbi/$targetDb.chainSyn${QueryDb}Link.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainSyn${QueryDb}.bb  $buildDir/bbi/$targetDb.chainSyn${QueryDb}.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainSyn${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chainSyn${QueryDb}Link.bb`;
  if (0 == $headerOut) {
    printf "
    track %sChainNetViewSynTen
    shortLabel Syntenic
    view synten
    visibility hide
    parent %sChainNet
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;
  }
  printf "
        track chainSyn%s
        parent %sChainNetViewSynTen off
        subGroups view=synten species=s%03d chainType=c01
        shortLabel %s synChain
        longLabel %s/%s%s (%s) Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chainSyn%s.bb
        linkDataUrl bbi/%s.chainSyn%sLink.bb
        otherDb %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $queryDb, $targetDb,
     $QueryDb, $targetDb, $QueryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }
  $N++;
}

$N = 0;
$headerOut = 0;
foreach my $queryDb (@queryList) {
  $comName = $queryDb;
  $comName = $commonName{$queryDb} if (defined($commonName{$queryDb}));
  $QueryDb = ucfirst($queryDb);

  if ( -s "$buildDir/trackData/lastz.$queryDb/axtChain/chainRBest${QueryDb}.bb" ) {
    `rm -f $buildDir/bbi/$targetDb.chainRBest${QueryDb}.bb`;
    `rm -f $buildDir/bbi/$targetDb.chainRBest${QueryDb}Link.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainRBest${QueryDb}.bb  $buildDir/bbi/$targetDb.chainRBest${QueryDb}.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainRBest${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chainRBest${QueryDb}Link.bb`;
  if (0 == $headerOut) {
    printf "
    track %sChainNetViewRBest
    shortLabel Reciprocal best
    view rbest
    visibility hide
    parent %sChainNet
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;
  }

  printf "
        track chainRBest%s
        parent %sChainNetViewRBest off
        subGroups view=rbest species=s%03d chainType=c02
        shortLabel %s rbChain
        longLabel %s/%s%s (%s) Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chainRBest%s.bb
        linkDataUrl bbi/%s.chainRBest%sLink.bb
        otherDb %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $queryDb, $targetDb,
     $QueryDb, $targetDb, $QueryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }
  $N++;
}

$N = 0;
$headerOut = 0;
foreach my $queryDb (@queryList) {
  $comName = $queryDb;
  $comName = $commonName{$queryDb} if (defined($commonName{$queryDb}));
  $QueryDb = ucfirst($queryDb);

  if ( -s "$buildDir/trackData/lastz.$queryDb/axtChain/chainLiftOver${QueryDb}.bb" ) {
    `rm -f $buildDir/bbi/$targetDb.chainLiftOver${QueryDb}.bb`;
    `rm -f $buildDir/bbi/$targetDb.chainLiftOver${QueryDb}Link.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainLiftOver${QueryDb}.bb  $buildDir/bbi/$targetDb.chainLiftOver${QueryDb}.bb`;
    `ln -s ../trackData/lastz.$queryDb/axtChain/chainLiftOver${QueryDb}Link.bb  $buildDir/bbi/$targetDb.chainLiftOver${QueryDb}Link.bb`;

  if (0 == $headerOut) {
    printf "
    track %sChainNetViewLiftOver
    shortLabel Lift over
    view liftover
    visibility hide
    parent %sChainNet
    spectrum on
", $targetDb, $targetDb;
    $headerOut = 1;
  }

  printf "
        track chainLiftOver%s
        parent %sChainNetViewLiftOver off
        subGroups view=liftover species=s%03d chainType=c03
        shortLabel %s loChain
        longLabel %s/%s%s (%s) Chained Alignments
        type bigChain %s
        bigDataUrl bbi/%s.chainLiftOver%s.bb
        linkDataUrl bbi/%s.chainLiftOver%sLink.bb
        otherDb %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $queryDb, $targetDb,
     $QueryDb, $targetDb, $QueryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }
  $N++;
}

printf "
    track %sMafNetViewnet
    shortLabel Nets
    view align
    visibility hide
    parent %sChainNet
", $targetDb, $targetDb;

$N = 0;
foreach my $queryDb (@queryList) {
  $comName = $queryDb;
  $comName = $commonName{$queryDb} if (defined($commonName{$queryDb}));
  $QueryDb = ucfirst($queryDb);
  my @targetAccession = split('_', $targetDb);
  my $targetAcc = sprintf("%s_%s", $targetAccession[0], $targetAccession[1]);
  my $queryDate = "some date";
  my $queryAsmName = "";
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.net.bb`;
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.net.summary.bb`;
  if ( -s "$buildDir/trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.net.bb" ) {
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.net.bb  $buildDir/bbi/$targetDb.${queryDb}.net.bb`;
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.net.summary.bb  $buildDir/bbi/$targetDb.${queryDb}.net.summary.bb`;
  if ( $queryDb !~ m/^GC/ ) {
    $queryDate = `hgsql -N -e 'select description from dbDb where name="$queryDb"' hgcentraltest | sed -e 's/ (.*//;'`;
    chomp $queryDate;
  } else {
    ($queryDate, $queryAsmName) = &HgAutomate::hubDateName($queryDb);
  }
    printf "
        track net%s
        parent %sMafNetViewnet off
        subGroups view=align species=s%03d chainType=c04
        shortLabel %s mafNet
        longLabel %s/%s%s (%s) Chained Alignments
        type bigMaf
        bigDataUrl bbi/%s.%s.net.bb
        summary bbi/%s.%s.net.summary.bb
        speciesOrder %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $targetDb, $queryDb, $targetDb, $queryDb, $queryDb, $targetDb, $queryPrio{$queryDb};
  }
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.synNet.bb`;
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.synNet.summary.bb`;
  if ( -s "$buildDir/trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.synNet.bb" ) {
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.synNet.bb  $buildDir/bbi/$targetDb.${queryDb}.synNet.bb`;
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.synNet.summary.bb  $buildDir/bbi/$targetDb.${queryDb}.synNet.summary.bb`;
    printf "
        track synNet%s
        parent %sMafNetViewnet off
        subGroups view=align species=s%03d chainType=c05
        shortLabel %s synNet
        longLabel %s/%s%s (%s) Chained Alignments
        type bigMaf
        bigDataUrl bbi/%s.%s.synNet.bb
        summary bbi/%s.%s.synNet.summary.bb
        speciesOrder %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $targetDb, $queryDb, $targetDb, $queryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }

  `rm -f $buildDir/bbi/$targetDb.${queryDb}.rbestNet.bb`;
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.rbestNet.summary.bb`;
  if ( -s "$buildDir/trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.rbestNet.bb" ) {
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.rbestNet.bb  $buildDir/bbi/$targetDb.${queryDb}.rbestNet.bb`;
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.rbestNet.summary.bb  $buildDir/bbi/$targetDb.${queryDb}.rbestNet.summary.bb`;
    printf "
        track rbestNet%s
        parent %sMafNetViewnet off
        subGroups view=align species=s%03d chainType=c06
        shortLabel %s rbestNet
        longLabel %s/%s%s (%s) Chained Alignments
        type bigMaf
        bigDataUrl bbi/%s.%s.rbestNet.bb
        summary bbi/%s.%s.rbestNet.summary.bb
        speciesOrder %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $targetDb, $queryDb, $targetDb, $queryDb, $queryDb, $targetDb, $queryPrio{$queryDb};

  }

  `rm -f $buildDir/bbi/$targetDb.${queryDb}.liftOverNet.bb`;
  `rm -f $buildDir/bbi/$targetDb.${queryDb}.liftOverNet.summary.bb`;
  if ( -s "$buildDir/trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.liftOverNet.bb" ) {
printf STDERR "constructing liftOverNet links $targetDb $queryDb\n";
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.liftOverNet.bb  $buildDir/bbi/$targetDb.${queryDb}.liftOverNet.bb`;
  `ln -s ../trackData/lastz.$queryDb/bigMaf/${targetAcc}.${queryDb}.liftOverNet.summary.bb  $buildDir/bbi/$targetDb.${queryDb}.liftOverNet.summary.bb`;
    printf "
        track liftOverNet%s
        parent %sMafNetViewnet off
        subGroups view=align species=s%03d chainType=c07
        shortLabel %s liftOverNet
        longLabel %s/%s%s (%s) Chained Alignments
        type bigMaf
        bigDataUrl bbi/%s.%s.liftOverNet.bb
        summary bbi/%s.%s.liftOverNet.summary.bb
        speciesOrder %s
        html html/%s.chainNet
        priority %s
", $QueryDb, $targetDb, $N, $comName, $comName, $queryDb, $queryAsmName, $queryDate, $targetDb, $queryDb, $targetDb, $queryDb, $queryDb, $targetDb, $queryPrio{$queryDb}

  }
  $N++;
}

printf "\n" if ($N > 0);
