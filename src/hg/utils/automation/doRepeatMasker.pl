#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doRepeatMasker.pl instead.

# $Id: doRepeatMasker.pl,v 1.14 2009/03/19 16:15:29 hiram Exp $

use Getopt::Long;
use warnings;
use strict;
use Carp;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Hardcoded command path:
my $RepeatMaskerPath = "/hive/data/outside/RepeatMasker/RepeatMasker-4.2.1";
my $RepeatMasker = "$RepeatMaskerPath/RepeatMasker";
# default engine changed from crossmatch to rmblast as of 2022-12
# with RM version 4.1.4
# version 4.2.1 changed the way the libraries are used, now need
#   the -uncurated option to have behavior similar to before
my $RepeatMaskerEngine = "-uncurated -engine rmblast -pa 1";
# per RM doc, rmblast uses 4 CPUs for each job
my $parasolRAM = "-cpu=4 -ram=32g";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_ncbiRmsk
    $opt_dupList
    $opt_liftSpec
    $opt_species
    $opt_unmaskedSeq
    $opt_customLib
    $opt_useHMMER
    $opt_useRMBlastn
    $opt_useCrossMatch
    $opt_splitTables
    $opt_noSplit
    $opt_updateTable
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'cluster', func => \&doCluster },
      { name => 'cat',     func => \&doCat },
      { name => 'mask',    func => \&doMask },
      { name => 'install', func => \&doInstall },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $unmaskedSeq = "\$db.unmasked.2bit";
my $defaultSpecies = 'scientificName from dbDb';
my $ncbiRmsk = "";	# path to NCBI *_rm.out.gz file to use NCBI result
                        # for repeat masking result
                        # in the assembly as calculated by NCBI
my $dupList = "";	# path to download/asmId.remove.dups.list for NCBI
my $liftSpec = "";	# lift file from NCBI coordinates to UCSC coordinates
                        # usually required with ncbiRmsk

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
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/RepeatMasker.\$date
                          (necessary when continuing at a later date).
    -species sp           Use sp (which can be quoted genus and species, or
                          a common name that RepeatMasker recognizes.
                          Default: $defaultSpecies.
    -ncbiRmsk path/file_rm.out.gz - Use the repeat masker result supplied in
                          the assembly as calculated by NCBI
    -dupList .../download/asmId.remove.dups.list - to remove duplicates from
			  NCBI repeat masker file
    -liftSpec path/file.lift - Use this lift file to lift the NCBI coordinates
                          to UCSC coordinates, usually used with ncbiRmsk
    -unmaskedSeq seq.2bit Use seq.2bit as the unmasked input sequence instead
                          of default ($unmaskedSeq).
    -customLib lib.fa     Use custom repeat library instead of RepeatMaskers\'s.
    -useRMBlastn          This is the default as of 2022-12 == NCBI rmblastn
    -useCrossMatch        Use crossmatch instead of NCBI rmblastn
    -useHMMER             Use hmmer instead of crossmatch ( currently for human only )
    -updateTable          load into table name rmskUpdate (default: rmsk)
    -splitTables          split the _rmsk tables (default is not split)
    -noSplit              default behavior, this option no longer required.
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '');
  print STDERR "
Automates UCSC's RepeatMasker process for genome database \$db.  Steps:
    cluster: Do a cluster run of RepeatMasker on 500kb sequence chunks.
    cat:     Concatenate the cluster run results into \$db.sorted.fa.out.
    mask:    Create a \$db.2bit masked by \$db.sorted.fa.out.
    install: Load \$db.sorted.fa.out into the rmsk table (possibly split) in \$db,
             install \$db.2bit in $HgAutomate::clusterData/\$db/, and if \$db is
             chrom-based, split \$db.sorted.fa.out into per-chrom files.
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/RepeatMasker.\$date unless -buildDir is given.
Run -help to see what files are required for this script.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.unmasked.2bit contains sequence for
   database/assembly \$db.  (This can be overridden with -unmaskedSeq.)
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir, $chromBased, $updateTable, $secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'species=s',
		      'ncbiRmsk=s',
		      'dupList=s',
		      'liftSpec=s',
		      'unmaskedSeq=s',
		      'customLib=s',
                      'useRMBlastn',
                      'useCrossMatch',
                      'useHMMER',
		      'splitTables',
		      'noSplit',
		      'updateTable',
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
# * step: cluster [bigClusterHub]
sub doCluster {
  return if ($ncbiRmsk);
  my $runDir = "$buildDir/run.cluster";
  if ( -d "$runDir" ) {
       if ( -s "$runDir/run.time" ) {
         &HgAutomate::verbose(1,
	  "\ncluster step previously completed\n");
         return;
       } else {
         die "\nERROR: cluster step may not be completed properly\n";
       }
  }
  &HgAutomate::mustMkdir($runDir);

  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
    &HgAutomate::chooseClusterByBandwidth();
  if (! -e $unmaskedSeq) {
    die "Error: required file $unmaskedSeq does not exist.";
  }
  my @okIn = grep !/scratch/,
    &HgAutomate::chooseFilesystemsForCluster($paraHub, "in");
  my @okOut = &HgAutomate::chooseFilesystemsForCluster($paraHub, "out");
  if (scalar(@okOut) > 1) {
    @okOut = grep !/$okIn[0]/, @okOut;
  }
  my $inHive = 0;
  $inHive = 1 if ($okIn[0] =~ m#/hive/data/genomes#);
  my $clusterSeqDir = "$okIn[0]/$db";
  $clusterSeqDir = "$buildDir" if ($opt_unmaskedSeq);
  my $clusterSeq = "$clusterSeqDir/$db.unmasked.2bit";
  $clusterSeq = "$unmaskedSeq" if ($opt_unmaskedSeq);
  my $partDir .= "$okOut[0]/$db/RMPart";
  $partDir = "$buildDir/RMPart" if ($opt_unmaskedSeq);
  my $species = $opt_species ? $opt_species : &HgAutomate::getSpecies($dbHost, $db);
  my $customLib = $opt_customLib;
  my $repeatLib = "";
  if ($opt_customLib && $opt_species) {
     $repeatLib = "-species \'$species\' -lib $customLib";
  }
  elsif ($opt_customLib) {
     $repeatLib = "-lib $customLib";
  }
  else {
     $repeatLib = "-species \'$species\'";
  }

  # updated for ku kluster operation -cpu option instead of ram option
  if ( $opt_useRMBlastn ) {
    $RepeatMaskerEngine = "-uncurated -engine rmblast -pa 1";
    $parasolRAM = "-cpu=4 -ram=32g";
  } elsif ( $opt_useCrossMatch ) {
    $RepeatMaskerEngine = "-uncurated -engine crossmatch -s";
    $parasolRAM = "-cpu=1";
  } elsif ( $opt_useHMMER ) {
    # NOTE: This is only applicable for 8gb one-job-per-node scheduling
    $RepeatMaskerEngine = "-uncurated -engine hmmer -pa 4";
    $parasolRAM = "-cpu=4 -ram=32g";
  }

  # Script to do a dummy run of RepeatMasker, to test our invocation and
  # unpack library files before kicking off a large cluster run.
  #  And now that RM is being run from local /scratch/data/RepeatMasker/
  #  this is also done in the cluster run script so each node will have
  #	its library initialized
  my $fh = &HgAutomate::mustOpen(">$runDir/dummyRun.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

$RepeatMasker $RepeatMaskerEngine $repeatLib /dev/null
_EOF_
  ;
  close($fh);

  my $tmpDir = &HgAutomate::tmpDir();
  # Cluster job script:
  $fh = &HgAutomate::mustOpen(">$runDir/RMRun.csh");
  print $fh <<_EOF_
#!/bin/csh -ef

if ( -d "/data/tmp" ) then
  setenv TMPDIR "/data/tmp"
else if ( -d "/scratch/tmp" ) then
  setenv TMPDIR "/scratch/tmp"
else
  setenv TMPDIR "/tmp"
endif

set finalOut = \$1

set inLst = \$finalOut:r
set inLft = \$inLst:r.lft
set alignOut = \$finalOut:r.align
set catOut = \$finalOut:r.cat

# Use local disk for output, and move the final result to \$outPsl
# when done, to minimize I/O.
set tmpDir = `mktemp -d -p \$TMPDIR doRepeatMasker.cluster.XXXXXX`
pushd \$tmpDir

# Initialize local library
$RepeatMasker $RepeatMaskerEngine $repeatLib /dev/null

foreach spec (`cat \$inLst`)
  # Remove path and .2bit filename to get just the seq:start-end spec:
  set base = `echo \$spec | sed -r -e 's/^[^:]+://'`
  # RM has a limitation of the length of a sequence name
  # in the case of name too long, create a shorter name, then lift
  set nameLength = `echo \$base | wc -c`
  set shortName = `echo \$base  | sed -e 's/>//;' | md5sum | cut -d' ' -f1`
  # If \$spec is the whole sequence, twoBitToFa removes the :start-end part,
  # which causes liftUp to barf later.  So tweak the header back to
  # seq:start-end for liftUp's sake:
  if ( \$nameLength > 45 ) then
    twoBitToFa \$spec stdout | sed -e "s/^>.*/>\$shortName/" > \$base.fa
    set fastaSize = `faSize \$base.fa | grep bases | cut -d' ' -f1`
    /bin/printf "0\\t\$shortName\\t\$fastaSize\\t\$base\\t\$fastaSize" > lift.\$base.txt
  else
    twoBitToFa \$spec stdout | sed -e "s/^>.*/>\$base/" > \$base.fa
  endif
  $RepeatMasker $RepeatMaskerEngine -align $repeatLib \$base.fa
  if ( \$nameLength > 45 ) then
    liftUp -type=.out stdout lift.\$base.txt error \$base.fa.out > lift.\$base.out
    liftUp -type=.align stdout lift.\$base.txt error \$base.fa.align > lift.\$base.align
    mv -f lift.\$base.out \$base.fa.out
    mv -f lift.\$base.align \$base.fa.align
  endif
  if (-e \$base.fa.cat) then
    if ( \$nameLength > 45 ) then
      liftUp -type=.align stdout lift.\$base.txt error \$base.fa.cat > lift.\$base.cat
      mv -f lift.\$base.cat \$base.fa.cat
    endif
    mv \$base.fa.cat \$catOut
  endif
end

# Lift up (leave the RepeatMasker header in place because we'll liftUp
# again later):
liftUp -type=.out stdout \$inLft error *.fa.out > tmpOut__out

set nonomatch
set alignFiles = ( *.align )
if ( \${#alignFiles} && -e \$alignFiles[1] ) then
  liftUp -type=.align stdout \$inLft error *.align \\
    > tmpOut__align
else
  touch tmpOut__align
endif

# Move final result into place:
mv tmpOut__out \$finalOut
mv tmpOut__align \$alignOut

popd
rm -rf \$tmpDir
_EOF_
  ;
  close($fh);

  &HgAutomate::makeGsub($runDir,
      "./RMRun.csh {check out line $partDir/\$(path1).out}");

  my $whatItDoes =
"It computes a logical partition of unmasked 2bit into 500k chunks
and runs it on the cluster with the most available bandwidth.";
  my $bossScript = new HgRemoteScript("$runDir/doCluster.csh", $paraHub,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_

chmod a+x dummyRun.csh
chmod a+x RMRun.csh

# Record RM version used:
printf "The repeat files provided for this assembly were generated using RepeatMasker.\\
  Smit, AFA, Hubley, R & Green, P.,\\
  RepeatMasker version 4.2.1\\
  1996-2025 <http://www.repeatmasker.org>.\\
\\
VERSION:\\n" > ../versionInfo.txt

./dummyRun.csh | grep -v "dev/null" >> ../versionInfo.txt

$RepeatMasker -v >> ../versionInfo.txt
printf "# RMRBMeta.embl library version: %s\\n" "`grep RELEASE $RepeatMaskerPath/Libraries/RMRBMeta.embl`" >> ../versionInfo.txt
printf "# RepeatMasker engine: %s\\n" "${RepeatMaskerEngine}" >> ../versionInfo.txt

ls -ld $RepeatMaskerPath $RepeatMasker
$RepeatMasker -v
echo -n "# RMRBMeta.embl library version: "
grep RELEASE $RepeatMaskerPath/Libraries/RMRBMeta.embl | sed -e 's/  *\\*\$//;'
echo "# RepeatMasker engine: $RepeatMaskerEngine"
_EOF_
  );
  if ($opt_useCrossMatch) {
    $bossScript->add(<<_EOF_
printf "# using engine crossmatch\\n" >> ../versionInfo.txt
echo "# useCrossMatch crossmatch"
_EOF_
    );
  } elsif ($opt_useRMBlastn) {
    $bossScript->add(<<_EOF_
printf "# using engine rmblastn:\\t" >> ../versionInfo.txt
echo "# useRMBlastn: rmblastn:"
grep -w value $RepeatMaskerPath/RepeatMaskerConfig.pm | grep rmblastn | awk '{print \$NF}' >> ../versionInfo.txt
_EOF_
    );
  } elsif ($opt_useHMMER) {
    $bossScript->add(<<_EOF_
printf "# using Dfam library and HMMER3:\\n" >> ../versionInfo.txt
echo "# useHMMER: Dfam library: "
ls -ld $RepeatMaskerPath/Libraries/Dfam.hmm
grep Release: $RepeatMaskerPath/Libraries/Dfam.hmm >> ../versionInfo.txt
echo "# useHMMER: HMMER3: "
grep -m 1 ^HMMER3 $RepeatMaskerPath/Libraries/Dfam.hmm >> ../versionInfo.txt
_EOF_
    );
  }
  if (length($repeatLib) > 0) {
    $bossScript->add(<<_EOF_
printf "# RepeatMasker library options: %s\\n" "${repeatLib}" >> ../versionInfo.txt
echo "# RepeatMasker library options: '$repeatLib'"
_EOF_
    );
  }
  if ( ! $opt_unmaskedSeq && ! $inHive) {
    $bossScript->add(<<_EOF_
mkdir -p $clusterSeqDir
rsync -av $unmaskedSeq $clusterSeq
_EOF_
    );
  }
  if ($opt_unmaskedSeq) {
    $bossScript->add(<<_EOF_
rm -rf $partDir
$Bin/simplePartition.pl $clusterSeq 500000 $partDir
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
rm -rf $partDir
$Bin/simplePartition.pl $clusterSeq 500000 $partDir
rm -f $buildDir/RMPart
ln -s $partDir $buildDir/RMPart
_EOF_
    );
  }
  my $binPara = "/parasol/bin/para";
  if ( ! -s "$binPara" ) {
    # allow PATH to find the para command
    $binPara = "para";
  }
  my $gensub2 = &HgAutomate::gensub2();
  $bossScript->add(<<_EOF_

printf "\\nPARAMETERS:\\
$RepeatMasker $RepeatMaskerEngine -align $repeatLib\\n" >> ../versionInfo.txt

$gensub2 $partDir/partitions.lst single gsub jobList
$binPara $parasolRAM make jobList
$binPara check
$binPara time > run.time
cat run.time

_EOF_
  );
  if (! $opt_unmaskedSeq && ! $inHive) {
    $bossScript->add(<<_EOF_
rm -f $clusterSeq
_EOF_
    );
  }
  `touch "$runDir/para_hub_$paraHub"`;
  $bossScript->execute();
} # doCluster

#########################################################################
# * step: cat [fileServer]
sub doCat {
  my $runDir = "$buildDir";
  if (! -s "${ncbiRmsk}" ) {
      &HgAutomate::checkExistsUnlessDebug('cluster', 'cat',
				      "$buildDir/run.cluster/run.time");
  }
  if ( -s "$runDir/doCat.bash" ) {
     if ( ! (-s "$runDir/$db.sorted.fa.out.gz" || -s "$runDir/$db.sorted.fa.out" ) ) {
         die "\nERROR: cat step may not be completed properly\n";
     } else {
         &HgAutomate::verbose(1,
	  "\ncat step previously completed\n");
         return;
     }
  }
  my $whatItDoes =
"It concatenates .out files from cluster run into a single $db.sorted.fa.out.\n" .
"liftUp (with no lift specs) is used to concatenate .out files because it\n" .
"uniquifies (per input file) the .out IDs which can then be used to join\n" .
"fragmented repeats in the Nested Repeats track.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/doCat.bash", $fileServer,
				      $runDir, $whatItDoes);

  $bossScript->add("# Debug mode -- don't know the real number of levels\n")
    if ($opt_debug);

  if ($ncbiRmsk) {
    my $zippedSource = 0;	# asssume not a gz zipped file
    $zippedSource = 1 if ($ncbiRmsk =~ m/.gz$/);
    my $liftOpts = "";
    if (-s "${liftSpec}" ) {
      $liftOpts = "| liftUp -type=.out stdout $liftSpec warn stdin";
    }
    $bossScript->add(<<_EOF_
export db="${db}"
export ncbiRmOutFile="${ncbiRmsk}"
cat /dev/null > "\${db}.sorted.fa.out"
_EOF_
    );
    if (! -s "${liftSpec}" ) {
    printf STDERR "# WARNING: no liftSpec given with ncbiRmsk file, probably will need one ?\n";
    $bossScript->add(<<_EOF_
#########
# WARNING: no liftSpec given with ncbiRmsk file, probably will need one ?
#########

printf "   SW  perc perc perc  query      position in query           matching       repeat              position in  repeat
score  div. del. ins.  sequence    begin     end    (left)    repeat         class/family         begin  end (left)   ID

" >> "\${db}.sorted.fa.out"
_EOF_
    );
    }
    if ($zippedSource) {
      if ( -s "${dupList}" ) {
        $bossScript->add(<<_EOF_
zcat "\${ncbiRmOutFile}" | tail -n +4 | sort -k5,5 -k6,6n $liftOpts \\
   | grep -v -f ${dupList} >> "\${db}.sorted.fa.out"
_EOF_
        );
      } else {
        $bossScript->add(<<_EOF_
zcat "\${ncbiRmOutFile}" | tail -n +4 | sort -k5,5 -k6,6n $liftOpts >> "\${db}.sorted.fa.out"
_EOF_
        );
      }
    } else {
      if ( -s "${dupList}" ) {
        $bossScript->add(<<_EOF_
tail -n +4 "\${ncbiRmOutFile}" sort -k5,5 -k6,6n $liftOpts \\
    | grep -v -f ${dupList} >> "\${db}.sorted.fa.out"
_EOF_
        );
      } else {
        $bossScript->add(<<_EOF_
tail -n +4 "\${ncbiRmOutFile}" sort -k5,5 -k6,6n $liftOpts >> "\${db}.sorted.fa.out"
_EOF_
        );
      }
    }
  } else {	# not using the NCBI supplied file, use local cluster run result
  my $partDir = "$buildDir/RMPart";
  my $levels = $opt_debug ? 1 : `cat $partDir/levels`;
  chomp $levels;
  # Use symbolic link created in cluster step:
  my $indent = "";
  my $path = "";
  # Make the appropriate number of nested foreach's to match the number
  # of levels from the partitioning.  If $levels is 1, no foreach required.
  for (my $l = $levels - 2;  $l >= 0;  $l--) {
    my $dir = ($l == ($levels - 2)) ? $partDir : "\$d" . ($l + 1);
    $bossScript->add(<<_EOF_
${indent}for d$l in $dir/???
do
_EOF_
    );
    $indent .= '  ';
    $path .= "\$d$l/";
  }
  for (my $l = 0;  $l <= $levels - 2;  $l++) {
    $bossScript->add(<<_EOF_
${indent}  liftUp ${path}cat.out /dev/null carry ${path}???/*.out
${indent}  liftUp ${path}cat.align /dev/null carry ${path}???/*.align
done
_EOF_
    );
    $indent =~ s/  //;
    $path =~ s/\$d\d+\/$//;
  }
  $bossScript->add(<<_EOF_

liftUp $db.fa.out /dev/null carry $partDir/???/*.out
liftUp $db.fa.align /dev/null carry $partDir/???/*.align
head -3 $db.fa.out > $db.sorted.fa.out
tail -n +4 $db.fa.out | sort -k5,5 -k6,6n >> $db.sorted.fa.out
_EOF_
  );
  }
  $bossScript->add(<<_EOF_

# Use the ID column to join up fragments of interrupted repeats for the
# Nested Repeats track.  Try to avoid the Undefined id errors.
($Bin/extractNestedRepeats.pl $db.sorted.fa.out 2> nestRep.err || true) | sort -k1,1 -k2,2n > $db.nestedRepeats.bed
if [ -s "nestRep.err" ]; then
  export lineCount=`(grep "Undefined id, line" nestRep.err || true) | cut -d' ' -f6 | wc -l`
   if [ "\${lineCount}" -gt 0 ]; then
     if [ "\${lineCount}" -gt 10 ]; then
        printf "ERROR: too many Undefined id lines (> 10) reported by extractNestedRepeats.pl" 1>&2
        exit 255
     fi
     export sedExpr=`grep "Undefined id, line" nestRep.err | cut -d' ' -f6 | sed -e 's/\$/d;/;' | xargs echo`
     export sedCmd="sed -i.broken -e '\$sedExpr'"
     eval \$sedCmd $db.sorted.fa.out
     ($Bin/extractNestedRepeats.pl $db.sorted.fa.out 2> nestRep2.err || true) | sort -k1,1 -k2,2n > $db.nestedRepeats.bed
     if [ -s "nestRep2.err" ]; then
        printf "ERROR: the attempt of cleaning nestedRepeats did not work" 1>&2
        exit 255
     fi
   fi
fi
_EOF_
  );
  $bossScript->execute();
} # doCat

#########################################################################
# * step: mask [workhorse]
sub doMask {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('cat', 'mask', "$buildDir/$db.sorted.fa.out");

  my $whatItDoes = "It makes a masked .2bit in this build directory.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doMask.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export db=$db
twoBitMask $unmaskedSeq \$db.sorted.fa.out \$db.rmsk$updateTable.2bit
twoBitToFa \$db.rmsk$updateTable.2bit stdout | faSize stdin > faSize.rmsk$updateTable.txt
_EOF_
  );
  $bossScript->execute();
} # doMask

#########################################################################
# * step: install [dbHost, maybe fileServer]
sub doInstall {
  my $runDir = "$buildDir";
  &HgAutomate::checkExistsUnlessDebug('cat', 'install', "$buildDir/$db.sorted.fa.out");

  my $split = "";
  $split = " (split)" if ($opt_splitTables);
  my $whatItDoes =
"It loads $db.sorted.fa.out into the$split rmsk$updateTable table and $db.nestedRepeats.bed\n" .
"into the nestedRepeats table.  It also installs the masked 2bit.";
  my $bossScript = newBash HgRemoteScript("$runDir/doLoad.bash", $dbHost,
				      $runDir, $whatItDoes);

  $split = "-nosplit";
  $split = "-split" if ($opt_splitTables);
  my $installDir = "$HgAutomate::clusterData/$db";
  $bossScript->add(<<_EOF_
export db=$db

# ensure sort functions properly despite kluster node environment
export LC_COLLATE=C

hgLoadOut -table=rmsk$updateTable $split \$db \$db.sorted.fa.out
hgLoadOut -verbose=2 -tabFile=\$db.rmsk$updateTable.tab -table=rmsk$updateTable -nosplit \$db \$db.sorted.fa.out 2> \$db.bad.records.txt
# construct bbi files for assembly hub
$RepeatMaskerPath/util/rmToTrackHub.pl -out \$db.sorted.fa.out -align \$db.fa.align
# in place same file sort using the -o output option
awk -F\$'\\t' '\$15 > -1 && \$13 > -1' \$db.fa.align.tsv | sort -k1,1 -k2,2n > t.tsv
rm -f \$db.fa.align.tsv
mv t.tsv \$db.fa.align.tsv
# sort -k1,1 -k2,2n -o \$db.fa.align.tsv \$db.fa.align.tsv &
sort -k1,1 -k2,2n -o \$db.sorted.fa.join.tsv \$db.sorted.fa.join.tsv
wait
bedToBigBed -tab -as=\$HOME/kent/src/hg/lib/bigRmskAlignBed.as -type=bed3+14 \\
  \$db.fa.align.tsv ../../chrom.sizes \$db.rmsk.align.bb &
bedToBigBed -tab -as=\$HOME/kent/src/hg/lib/bigRmskBed.as -type=bed9+5 \\
  \$db.sorted.fa.join.tsv ../../chrom.sizes \$db.rmsk.bb
wait
rm -fr classBed classBbi rmskClass
mkdir classBed classBbi rmskClass
sort -k12,12 \$db.rmsk$updateTable.tab \\
  | splitFileByColumn -ending=tab  -col=12 -tab stdin rmskClass
for T in SINE LINE LTR DNA Simple Low_complexity Satellite
do
    fileCount=`(ls rmskClass/\${T}*.tab 2> /dev/null || true) | wc -l`
    if [ "\$fileCount" -gt 0 ]; then
       echo "working: "`ls rmskClass/\${T}*.tab | xargs echo`
       \$HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/\${T}*.tab \\
        | sort -k1,1 -k2,2n > classBed/\$db.rmsk.\${T}.bed
       bedToBigBed -tab -type=bed6+10 -as=\$HOME/kent/src/hg/lib/rmskBed6+10.as \\
         classBed/\$db.rmsk.\${T}.bed ../../chrom.sizes \\
           classBbi/\$db.rmsk.\${T}.bb
    fi
done
fileCount=`(ls rmskClass/*RNA.tab 2> /dev/null || true) | wc -l`
if [ "\$fileCount" -gt 0 ]; then
  echo "working: "`ls rmskClass/*RNA.tab | xargs echo`
  \$HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/*RNA.tab \\
     | sort -k1,1 -k2,2n > classBed/\$db.rmsk.RNA.bed
  bedToBigBed -tab -type=bed6+10 -as=\$HOME/kent/src/hg/lib/rmskBed6+10.as \\
     classBed/\$db.rmsk.RNA.bed ../../chrom.sizes \\
        classBbi/\$db.rmsk.RNA.bb
fi
fileCount=`(ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" 2> /dev/null || true) | wc -l`
if [ "\$fileCount" -gt 0 ]; then
  echo "working: "`ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" | xargs echo`
  \$HOME/kent/src/hg/utils/automation/rmskBed6+10.pl `ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab"` \\
        | sort -k1,1 -k2,2n > classBed/\$db.rmsk.Other.bed
  bedToBigBed -tab -type=bed6+10 -as=\$HOME/kent/src/hg/lib/rmskBed6+10.as \\
    classBed/\$db.rmsk.Other.bed ../../chrom.sizes \\
      classBbi/\$db.rmsk.Other.bb
fi

export bbiCount=`for F in classBbi/*.bb; do bigBedInfo \$F | grep itemCount; done | awk '{print \$NF}' | sed -e 's/,//g' | ave stdin | grep total | awk '{print \$2}' | sed -e 's/.000000//'`

export firstTabCount=`cat \$db.rmsk$updateTable.tab | wc -l`
export splitTabCount=`cat rmskClass/*.tab | wc -l`

if [ "\$firstTabCount" -ne \$bbiCount ]; then
   echo "\$db.rmsk$updateTable.tab count: \$firstTabCount, split class tab file count: \$splitTabCount, bbi class item count:  \$bbiCount"
   echo "ERROR: did not account for all items in rmsk class bbi construction" 1>&2
   exit 255
fi
wc -l classBed/*.bed > \$db.class.profile.txt
wc -l rmskClass/*.tab >> \$db.class.profile.txt
rm -fr rmskClass classBed \$db.rmsk$updateTable.tab
_EOF_
  );

  if ($opt_updateTable) {
  $bossScript->add(<<_EOF_
if [ -s \$db.nestedRepeats.bed ]; then
  sed -e 's/nestedRepeats/nestedRepeatsUpdate/g' \$HOME/kent/src/hg/lib/nestedRepeats.sql > nestedRepeatsUpdate.sql
  hgLoadBed \$db nestedRepeats$updateTable \$db.nestedRepeats.bed \\
    -sqlTable=nestedRepeatsUpdate.sql
fi
_EOF_
  );
  } else {
  $bossScript->add(<<_EOF_
if [ -s \$db.nestedRepeats.bed ]; then
  hgLoadBed \$db nestedRepeats \$db.nestedRepeats.bed \\
    -sqlTable=\$HOME/kent/src/hg/lib/nestedRepeats.sql
fi
_EOF_
  );
  }

  $bossScript->add(<<_EOF_
rm -f $installDir/\$db.rmsk$updateTable.2bit
ln -s $buildDir/\$db.rmsk$updateTable.2bit $installDir/\$db.rmsk$updateTable.2bit
_EOF_
  );
  $bossScript->execute();

  # Make a new script for the fileServer if chrom-based:
  if ($chromBased) {
    my $fileServer = &HgAutomate::chooseFileServer($runDir);
    $whatItDoes =
"It splits $db.sorted.fa.out into per-chromosome files in chromosome directories\n" .
"where makeDownload.pl will expect to find them.\n";
    my $bossScript = newBash HgRemoteScript("$runDir/doSplit.bash", $fileServer,
					$runDir, $whatItDoes);
    $bossScript->add(<<_EOF_
export db=$db
head -3 \$db.sorted.fa.out > /tmp/rmskHead.txt
tail -n +4 \$db.sorted.fa.out \\
| splitFileByColumn -col=5 stdin /cluster/data/\$db -chromDirs \\
    -ending=.fa.out -head=/tmp/rmskHead.txt
_EOF_
    );
    $bossScript->execute();
  }
} # doInstall


#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
if [ -e "$db.fa.align" ]; then
  gzip $db.fa.align
fi
rm -fr RMPart/*
rm -fr RMPart
if [ -d "/hive/data/genomes/$db/RMPart" ]; then
   rmdir /hive/data/genomes/$db/RMPart
fi
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
$secondsStart = `date "+%s"`;
chomp $secondsStart;
($db) = @ARGV;

# Now that we know the $db, figure out our paths:
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/RepeatMasker.$date";
$unmaskedSeq = $opt_unmaskedSeq ? $opt_unmaskedSeq :
  "$HgAutomate::clusterData/$db/$db.unmasked.2bit";
my $seqCount = `twoBitInfo $unmaskedSeq stdout | wc -l`;
$chromBased = ($seqCount <= $HgAutomate::splitThreshold) && $opt_splitTables;
$updateTable = $opt_updateTable ? "Update" : "";
$ncbiRmsk = $opt_ncbiRmsk ? $opt_ncbiRmsk : "";
$dupList = $opt_dupList ? $opt_dupList : "";
$liftSpec = $opt_liftSpec ? $opt_liftSpec : "";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

$secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough - Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s
 *** Steps were performed in $buildDir
_EOF_
);
if ($stepper->stepPrecedes('mask', $stopStep)) {
  &HgAutomate::verbose(1, <<_EOF_
 *** Please check the log file to see if hgLoadOut had warnings that we
     should pass on to Arian and Robert.
 *** Please also spot-check some repeats in the browser, run twoBitToFa on
     a portion of $db.2bit to make sure there's some masking, etc.

_EOF_
  );
}
&HgAutomate::verbose(1, "\n");
