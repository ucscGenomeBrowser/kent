#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doCpgIslands.pl instead.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_maskedSeq
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'compile', func => \&doCompile },
      { name => 'hardMask',   func => \&doHardMask },
      { name => 'cpg', func => \&doCpg },
      { name => 'makeBed', func => \&doMakeBed },
      { name => 'load', func => \&doLoadCpg },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $defaultWorkhorse = 'kolossus';
my $maskedSeq = "$HgAutomate::hiveDataGenomes/\$db/\$db.2bit";

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::hiveDataGenomes/\$db/$HgAutomate::trackBuild/cpgIslands
                          (necessary when continuing at a later date).
    -maskedSeq seq.2bit   Use seq.2bit as the masked input sequence instead
                          of default ($maskedSeq).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $defaultWorkhorse);
  print STDERR "
Automates UCSC's CpG Island finder for genome database \$db.  Steps:
    compile:   Check-out and build the CpG Island program from Asif Chinwalla.
    hardMask:  Creates hard-masked fastas needed for the CpG Island program.
    cpg:       Run cpglh.exe on the hard-masked fastas
    makeBed:   Transform output from cpglh.exe into cpgIsland.bed
    load:      Load cpgIsland.bed into \$db.
    cleanup:   Removes hard-masked fastas and output from cpglh.exe.
All operations are performed in the build directory which is
$HgAutomate::hiveDataGenomes/\$db/$HgAutomate::trackBuild/cpgIslands unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::hiveDataGenomes/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'maskedSeq=s',
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
# * step: check out [dbHost]
sub doCompile {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Checks out the cpglh source and compiles the executable.";
  my $bossScript = new HgRemoteScript("$runDir/doCompile.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
cvs -d /projects/compbio/cvsroot checkout -P hg3rdParty/cpgIslands
cd hg3rdParty/cpgIslands
# comment out the following two lines if it compiles cleanly
# some day
sed 's/\\(extern char\\* malloc\\)/\\/\\/ \\1/' cpg_lh.c > tmp.c
mv tmp.c cpg_lh.c
make
cd ../../
ln -s hg3rdParty/cpgIslands/cpglh.exe
_EOF_
  );
  $bossScript->execute();
} # doCompile

#########################################################################
# * step: hard mask [workhorse]
sub doHardMask {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Make hard-masked fastas for each chrom.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doHardMask.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
mkdir -p hardMaskedFa
foreach chrom ( `twoBitInfo $maskedSeq stdout | cut -f1` )
   twoBitToFa ${maskedSeq}:\$chrom stdout \\
      | maskOutFa stdin hard \\
      hardMaskedFa\/\$\{chrom\}.fa
end
_EOF_
  );
  $bossScript->execute();
} # doHardMask

#########################################################################
# * step: cpg [workhorse] (will change to a cluster at some point)
sub doCpg {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Run cpglh.exe on masked sequence.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doCpg.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
mkdir -p results
foreach chrom ( `twoBitInfo $maskedSeq stdout | cut -f1` )
   ./cpglh.exe hardMaskedFa\/\$\{chrom\}.fa > results\/\$\{chrom\}.cpg
end
_EOF_
  );
  $bossScript->execute();
} # doCpg

#########################################################################
# * step: make bed [workhorse]
sub doMakeBed {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Makes bed from cpglh.exe output.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = new HgRemoteScript("$runDir/doMakeBed.csh", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
catDir results \\
     | awk \'\{\$2 = \$2 - 1; width = \$3 - \$2;  printf\(\"\%s\\t\%d\\t\%s\\t\%s \%s\\t\%s\\t\%s\\t\%0.0f\\t\%0.1f\\t\%s\\t\%s\\n\", \$1, \$2, \$3, \$5, \$6, width, \$6, width\*\$7\*0.01, 100.0\*2\*\$6\/width, \$7, \$9\);}\' \\
     | sort -k1,1 -k2,2n > cpgIsland.bed
_EOF_
  );
  $bossScript->execute();
} # doMakeBed

#########################################################################
# * step: load [dbHost]
sub doLoadCpg {
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "Loads cpgIsland.bed.";
  my $bossScript = new HgRemoteScript("$runDir/doLoadCpg.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
cvs -d /projects/compbio/cvsroot checkout -P kent/src/hg/lib
ln -s kent/src/hg/lib/cpgIslandExt.sql
hgLoadBed -sqlTable=cpgIslandExt.sql -tab $db cpgIslandExt cpgIsland.bed 
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = $buildDir;
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf hg3rdParty/ kent/
rm -rf hardMaskedFa/
rm -rf results/
rm -f bed.tab cpgIslandExt.sql cpglh.exe
gzip cpgIsland.bed
_EOF_
  );
  $bossScript->execute();
} # doCleanup


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);
($db) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
#$opt_debug = 1;
#$opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::hiveDataGenomes/$db/$HgAutomate::trackBuild/cpgIslands";
$maskedSeq = $opt_maskedSeq ? $opt_maskedSeq :
  "$HgAutomate::hiveDataGenomes/$db/$db.2bit";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

