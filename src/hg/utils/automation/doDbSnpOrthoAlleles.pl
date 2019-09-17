#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doDbSnpOrthoAlleles.pl instead.

use Getopt::Long;
use LWP::UserAgent;
use warnings;
use strict;
use File::Basename;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;


my $stepper = new HgStepManager(
    [ { name => 'prepSnps',   func => \&prepSnps },
      { name => 'liftOver',  func => \&liftOver },
      { name => 'join',  func => \&join },
      { name => 'sort',  func => \&sort },
      { name => 'load',  func => \&load },
      { name => 'cleanup',  func => \&cleanup },
    ]
			       );

# Hardcoded (for now):
my $chimpDb = 'panTro5';
my $orangDb = 'ponAbe2';
my $macDb = 'rheMac8';

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    /;

# Option defaults:
my $dbHost = $HgAutomate::defaultDbHost;

my $basename = basename($0);

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $basename db dbSnpBuildNumber
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/snp<buildNum>Ortho.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
                                                'bigClusterHub' => '');
  print STDERR "
Automates creation of \"orthologous alleles\" (i.e. liftOver'd SNPs) for
humans in chimp, orangutan and rhesus macaque.

";
  exit $status;
}

# Globals:
# Command line args: db, buildNumber
my ($db, $buildNum);
# Other:
my ($buildDir, $track);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#########################################################################
# * step: prepSnps [workhorse]
sub prepSnps {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It filters SNPs and gloms coords, strand & alleles onto the name column.";
  my $bossScript = new HgRemoteScript("$runDir/prepSnps.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
# Filter snp$buildNum to to keep only uniquely mapped biallelic SNVs (class=single, length=1).
# Glom all human info that we need for the final table onto the name, to sneak it through
# liftOver: rsId|chr|start|end|obs|ref|strand

zcat /hive/data/outside/dbSNP/${buildNum}/human_$db/snp${buildNum}.bed.gz \\
| awk 'BEGIN{OFS="\\t";} \\
       {if (\$3-\$2 == 1 && \$11 == "single" && \\
            \$18 !~ /^MultipleAlignments|SingleClassTriAllelic|SingleClassQuadAllelic/) { \\
           print \$1, \$2, \$3, \\
           \$4 "|" \$1 "|" \$2 "|" \$3 "|" \$9 "|" \$8 "|" \$6, \\
           0, \$6; \\
        } \\
       }' \\
  > snp${buildNum}ForLiftOver.bed
_EOF_
  );
  $bossScript->execute();
} # prepSnps

#########################################################################
# * step: liftOver [cluster]
sub liftOver {
# Do a cluster run to use liftOver to get the other species' coords
# and get the species' "allele" (reference assembly base) at that location.
  my $runDir = "$buildDir/run.liftOver";
  &HgAutomate::mustMkdir($runDir);
  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
                                     &HgAutomate::chooseClusterByBandwidth();
  my $whatItDoes = "It lifts over human SNPs to other species' coords and gets their " .
                   "reference assembly base (\"allele\") at that location.";
  my $bossScript = new HgRemoteScript("$runDir/liftOver.csh", $paraHub,
				      $runDir, $whatItDoes);


  # Cluster job script:
  my $fh = &HgAutomate::mustOpen(">$runDir/liftOne.csh");
  print $fh <<EOF
#!/bin/csh -ef
set chunkFile = \$1
set oDb = \$2
set outFile = \$3
set ODb = `echo \$oDb | perl -wpe 's/(\\S+)/\\u\$1/'`
set liftOverFile = /hive/data/genomes/$db/bed/liftOver/${db}To\$ODb.over.chain.gz
set other2bit = /hive/data/genomes/\$oDb/\$oDb.2bit
# Use local disk for output, and move the final result to \$finalOut
# when done, to minimize I/O.
set tmpFile = `mktemp doDbSnpOrthoAlleles.liftOver.XXXXXX`
# End with a lexical sort because we're going to join these files later.
liftOver \$chunkFile \$liftOverFile stdout /dev/null \\
| \$HOME/kent/src/hg/snp/snpLoad/getOrthoSeq.pl \$other2bit \\
| sort > \$tmpFile
cp -p \$tmpFile \$outFile
rm \$tmpFile
EOF
  ;
  close($fh);

  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
chmod a+x liftOne.csh

# Map coords to chimp using liftOver.
mkdir split out
splitFile ../snp${buildNum}ForLiftOver.bed 20000 split/chunk
cp /dev/null jobList
foreach chunkFile (split/chunk*)
  set chunk = \$chunkFile:t:r
  foreach oDb ($chimpDb $orangDb $macDb)
    echo ./liftOne.csh \$chunkFile \$oDb \\{check out exists out/\$oDb.\$chunk.bed\\} \\
      >> jobList
  end
end

$paraRun
_EOF_
  );
  $bossScript->execute();
} # liftOver


#########################################################################
# * step: join [cluster]
sub join {
  my $runDir = "$buildDir/run.join";
  &HgAutomate::mustMkdir($runDir);
  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
                                     &HgAutomate::chooseClusterByBandwidth();
  my $whatItDoes = "It uses glommed names to join liftOver results for each chunk " .
                   "from the first cluster run.";
  my $bossScript = new HgRemoteScript("$runDir/join.csh", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
mkdir out
ln -s ../run.liftOver/split .
cp /dev/null jobList
foreach f (split/chunk*)
  set chunk = \$f:t
  echo $Bin/joinOrthoAlleles.csh \\
       ../run.liftOver/out/{$chimpDb,$orangDb,$macDb}.\$chunk.bed \\
        \\{check out exists out/\$chunk.bed\\} \\
        >> jobList
    end
$paraRun
_EOF_
  );
  $bossScript->execute();
} # join


#########################################################################
# * step: sort [workhorse]
sub sort {
  my $runDir = "$buildDir";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It combines and sorts all results of the join step to produce the final .bed.";
  my $bossScript = new HgRemoteScript("$runDir/sort.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
sort -k1,1 -k2n,2n run.join/out/chunk*.bed > $track.bed
_EOF_
  );
  $bossScript->execute();
} # sort


#########################################################################
# * step: load [dbHost]
sub load {
  my $runDir = "$buildDir";
  my $whatItDoes = "It loads $track.bed into the $db.$track table.";
  my $bossScript = new HgRemoteScript("$runDir/load.csh", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
hgLoadBed -tab -onServer -renameSqlTable \\
  -sqlTable=\$HOME/kent/src/hg/lib/snpOrthoPanPonRhe.sql \\
  $db $track $track.bed
_EOF_
  );
  $bossScript->execute();
} # load


#########################################################################
# * step: cleanup [workhorse]
sub cleanup {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It cleans up intermediate files.";
  my $bossScript = new HgRemoteScript("$runDir/cleanup.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -r run*/out run*/split bed.tab
gzip snp${buildNum}ForLiftOver.bed $track.bed
_EOF_
  );
  $bossScript->execute();
} # cleanup


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 2 arguments:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 2);
($db, $buildNum) = @ARGV;

if ($db !~ /^hg\d+/) {
  print STDERR "Expecting a human db, got '$db'\n";
  &usage(1);
}

if ($buildNum !~ /^\d{3}$/) {
  print STDERR "Expecting a 3-digit dbSNP build number (like 144), got '$buildNum'\n";
  &usage(1);
}

# Now that we know $db and $buildNum, figure out our paths:
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/snp${buildNum}Ortho.$date";

# Figure out our output file/table name:
my ($chimpAbbrev, $orangAbbrev, $macAbbrev) =
  map { $a = $_; $a =~ s/^([a-z])[a-z][a-z]([A-Z])[a-z][a-z]([\d+])/\u$1\l$2$3/ || die; $a; }
      ($chimpDb, $orangDb, $macDb);
$track = "snp${buildNum}Ortho$chimpAbbrev$orangAbbrev$macAbbrev";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough
 *** Steps were performed in $buildDir
_EOF_
);
