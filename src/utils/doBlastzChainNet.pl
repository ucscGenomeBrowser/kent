#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/doBlastzChainNet.pl instead.

# $Id: doBlastzChainNet.pl,v 1.8 2005/03/03 00:25:20 angie Exp $

# to-do items:
# - lots of testing
# - handle .2bit for target
# - better logging: right now it just passes stdout and stderr,
#   leaving redirection to a logfile up to the user
# - reciprocal best?
# - compress more files in the cleanup step
# - more options: -bigClusterHub, -smallClusterHub, -buildDir ...?  
# - make it churn out a download README.txt from template, inserting
#   params from DEF.

use Getopt::Long;
use warnings;
use strict;

# Hardcoded paths/command sequences:
my $getFileServer = '/cluster/bin/scripts/fileServer';
my $blastzRunUcsc = '/cluster/bin/scripts/blastz-run-ucsc';
my $partition = '/cluster/bin/scripts/partitionSequence.pl';
my $gensub2 = '/parasol/bin/gensub2';
my $para = '/parasol/bin/para';
my $bigClusterHub = 'kk';
my $smallClusterHub = 'kki';
my $workhorse = 'kolossus';
my $paraRun = ("$para make jobList\n" .
	       "$para check\n" .
	       "$para time > run.time\n" .
	       'cat run.time');
my $dbHost = 'hgwdev';
my $clusterData = '/cluster/data';
my $trackBuild = 'bed';
my $goldenPath = '/usr/local/apache/htdocs/goldenPath';
my $downloadPath = "$dbHost:$goldenPath";
my $clusterLocal = '/scratch/hg';
my $clusterSortaLocal = '/iscratch/i';
my @clusterNAS = ('/cluster/bluearc', '/panasas');
my $clusterNAS = join('/... or ', @clusterNAS) . '/...';
my @clusterNoNo = ('/cluster/home', '/projects');
my @fileServerNoNo = ('kkhome', 'kks00');
my $splitThreshold = 100;

# Option variable names:
use vars qw/
    $opt_continue
    $opt_stop
    $opt_blastzOutRoot
    $opt_swap
    $opt_workhorse
    $opt_debug
    $opt_verbose
    $opt_help
    /;

# Numeric values of -continue/-stop options for determining precedence:
my %stepVal = ( 'cat' => 1,
		'chainRun' => 2,
		'chainMerge' => 3,
		'net' => 4,
		'load' => 5,
		'download' => 6,
		'cleanup' => 7,
	      );

# Option defaults:
my $defaultVerbose = 1;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  my $stepVals = join(", ",
		      sort { $stepVal{$a} <=> $stepVal{$b} }  keys %stepVal);
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base DEF
options:
    -continue step        Pick up at the step where a previous run left off
                          (some debugging and cleanup may be necessary first).
                          step must be one of the following:
                          $stepVals
    -stop step            Stop after completing the specified step.
                          (same possible values as for -continue above)
    -blastzOutRoot dir    Directory path where outputs of the blastz cluster
                          run will be stored.  By default, they will be
                          stored in the $clusterData build directory , but
                          this option can specify something more cluster-
                          friendly: $clusterNAS .
                          If dir does not already exist it will be created.
                          Blastz outputs are removed in the cleanup step.
    -swap                 DEF has already been used to create chains; swap
                          those chains (target for query), then net etc. in
                          a new directory:
                          $clusterData/\$qDb/$trackBuild/blastz.\$tDb.swap/
    -workhorse machine    Use machine (default: $workhorse) for compute or
                          memory-intensive steps.
    -debug                Don't actually run commands, just display them.
    -verbose num          Set verbose level to num (default $defaultVerbose).
    -help                 Show detailed help and exit.
Automates UCSC's blastz/chain/net pipeline:
    1. Big cluster run of blastz.
    2. Small cluster consolidation of blastz result files.
    3. Small cluster chaining run.
    4. Sorting and netting of chains on the fileserver.
    5. Generation of liftOver-suitable chains from nets+chains on fileserver
       (not done for self-alignments).
    6. Generation of axtNet and mafNet files on the fileserver.
    7. Addition of gap/repeat info to nets on hgwdev.
    8. Loading of chain and net tables on hgwdev.
    9. Setup of download directory on hgwdev.
DEF is a Scott Schwartz-style bash script containing blastz parameters.
This script makes a lot of assumptions about conventional placements of 
certain files, and what will be in the DEF vars.  Stick to the conventions 
described in the -help output, pray to the cluster gods, and all will go 
well.  :)

";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $clusterData/\$db/ is the main directory for database/assembly \$db.
   $clusterData/\$tDb/$trackBuild/blastz.\$qDb.\$date/ will be the directory 
   created for this run, where \$tDb is the target/reference db and 
   \$qDb is the query.  (Can be overridden, see #10 below.)  
   $downloadPath/\$tDb/vs\$QDb/ (or vsSelf) 
   is the directory where downloadable files need to go.
   LiftOver chains (not applicable for self-alignments) are to be copied 
   to these directories:
   $clusterData/\$tDb/$trackBuild/bedOver/
   $downloadPath/\$tDb/liftOver/ 
2. DEF's SEQ1* variables describe the target/reference assembly.
   DEF's SEQ2* variables describe the query assembly.
   If those are the same assembly, then we're doing self-alignments and 
   will drop aligned blocks that cross the diagonal.
3. DEF's SEQ1_DIR is either a directory containing one nib file per 
   target sequence (usually chromosome), OR a complete path to a 
   single .2bit file containing all target sequences.  This directory 
   should be in $clusterLocal or $clusterSortaLocal .
   SEQ2_DIR: ditto for query.
4. DEF's SEQ1_LEN is a tab-separated dump of the target database table 
   chromInfo -- or at least a file that contains all sequence names 
   in the first column, and corresponding sizes in the second column.
   Normally this will be $clusterData/\$tDb/chrom.sizes, but for a 
   scaffold-based assembly, it is a good idea to put it in $clusterSortaLocal 
   or $clusterNAS because it will be a large file 
   and it is read by blastz-run-ucsc (big cluster script).
   SEQ2_LEN: ditto for query.
5. DEF's SEQ1_CHUNK and SEQ1_LAP determine the step size and overlap size 
   of chunks into which large target sequences are to be split before 
   alignment.  SEQ2_CHUNK and SEQ2_LAP: ditto for query.
6. DEF's BLASTZ_ABRIDGE_REPEATS should be set to something nonzero if 
   abridging of lineage-specific repeats is to be performed.  If so, the 
   following additional constraints apply:
   a. Both target and query assemblies must be structured as one nib file 
      per sequence in SEQ*_DIR (sorry, this rules out scaffold-based 
      assemblies).
   b. SEQ1_SMSK must be set to a directory containing one file per target 
      sequence, with the name pattern \$seq.out.spec.  This file must be 
      a RepeatMasker .out file (usually filtered by DateRepeats).  The 
      directory should be under $clusterLocal or $clusterSortaLocal .
      SEQ2_SMSK: ditto for query.
7. DEF's BLASTZ_[A-Z] variables will be translated into blastz command line 
   options (e.g. BLASTZ_H=foo --> H=foo, BLASTZ_Q=foo --> Q=foo).  
   For human-mouse evolutionary distance/sensitivity, none of these are 
   necessary (blastz-run-ucsc defaults will be used).  Here's what we have 
   used for human-fugu and other very-distant pairs:
BLASTZ_H=2000
BLASTZ_Y=3400
BLASTZ_L=6000
BLASTZ_K=2200
BLASTZ_Q=$clusterData/blastz/HoxD55.q
   Blastz parameter tuning is somewhat of an art and is beyond the scope 
   here.  Webb Miller and Jim can provide guidance on how to set these for 
   a new pair of organisms.  
8. DEF's PATH variable, if set, must specify a path that contains programs 
   necessary for blastz to run: blastz, and if BLASTZ_ABRIDGE_REPEATS is set, 
   then also fasta_subseq, strip_rpts, restore_rpts, and revcomp.  
   If DEF does not contain a PATH, blastz-run-ucsc will use its own default.
9. DEF's BLASTZ variable can specify an alternate path for blastz.
10. DEF's BASE variable can specify the blastz/chain/net build directory 
    (defaults to $clusterData/\$tDb/$trackBuild/blastz.\$qDb.\$date/).
11. All other variables in DEF will be ignored!

" if ($detailed);
  exit $status;
}


# Globals:
my %defVars = ();
my ($DEF, $tDb, $qDb, $QDb, $isSelf, $buildDir, $startStep, $stopStep);
my ($swapDir, $splitRef);

sub isInDirList {
  # Return TRUE if $dir is under (begins with) something in dirList.
  my ($dir, @dirList) = @_;
  my $pat = '^(' . join('|', @dirList) . ')(/.*)?$';
  return ($dir =~ m@$pat@);
}

sub enforceClusterNoNo {
  # Die right away if user is trying to put cluster output somewhere 
  # off-limits.
  my ($dir, $desc) = @_;
  if (&isInDirList($dir, @clusterNoNo)) {
    die "\ncluster outputs are forbidden to go to " .
      join (' or ', @clusterNoNo) . " so please choose a different " .
      "$desc instead of $dir .\n\n";
  }
  my $fileServer = `$getFileServer $dir/`;
  if (scalar(grep /^$fileServer$/, @fileServerNoNo)) {
    die "\ncluster outputs are forbidden to go to fileservers " .
      join (' or ', @fileServerNoNo) . " so please choose a different " .
      "$desc instead of $dir (which is hosted on $fileServer).\n\n";
  }
}

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions("continue=s",
		      "stop=s",
		      "blastzOutRoot=s",
		      "swap",
		      "workhorse=s",
		      "verbose=n",
		      "debug",
		      "help");
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  $opt_verbose = $defaultVerbose if (! defined $opt_verbose);
  if ($opt_continue) {
    if (! defined $stepVal{$opt_continue}) {
      warn "\nUnsupported -continue value \"$opt_continue\".\n";
      &usage(1);
    }
    $startStep = $stepVal{$opt_continue};
    if ($opt_swap && $startStep < $stepVal{'net'}) {
      warn "\nIf -swap is specified, then -continue must specify a step ". 
	"of \"net\" or later.\n";
      &usage(1);
    }
  } else {
    $startStep = $opt_swap ? $stepVal{'chainMerge'} : 0;
  }
  if ($opt_stop) {
    if (! defined $stepVal{$opt_stop}) {
      warn "\nUnsupported -stop value \"$opt_stop\".\n";
      &usage(1);
    }
    $stopStep = $stepVal{$opt_stop};
    if ($opt_swap && $stopStep < $stepVal{'chainMerge'}) {
      warn "\nIf -swap is specified, then -stop must specify a step ". 
	"of \"chainMerge\" or later.\n";
      &usage(1);
    }
  } else {
    $stopStep = scalar(keys %stepVal);
  }
  if ($stopStep < $startStep) {
    warn "\n-stop step ($opt_stop) must not precede -continue step " .
      "($opt_continue).\n";
    &usage(1);
  }
  if ($opt_blastzOutRoot) {
    if ($opt_blastzOutRoot !~ m@^/\S+/\S+@) {
      warn "\n-blastzOutRoot must specify a full path.\n";
      &usage(1);
    }
    &enforceClusterNoNo($opt_blastzOutRoot, '-blastzOutRoot');
    if (! &isInDirList($opt_blastzOutRoot, @clusterNAS)) {
      warn "\n-blastzOutRoot is intended to specify something on " .
	"$clusterNAS, but I'll trust your judgment " .
	"and use $opt_blastzOutRoot\n\n";
    }
  }
  $workhorse = $opt_workhorse if ($opt_workhorse);
}

#########################################################################
# The following routines were taken almost verbatim from blastz-run-ucsc,
# so may be good candidates for libification!  unless that would slow down 
# blastz-run-ucsc...
# run() had $opt_debug added to it which would be a good idea for b-r-u too.
# run had path stuff removed -- user's path should be good enough already.
# nfsNoodge() was removed from loadDef() and loadSeqSizes() -- since this 
# script will not be run on the cluster, we should fully expect files to 
# be immediately visible.  

sub run {
  # Run a command in sh (unless -debug).
  my ($cmd) = @_;
  if ($opt_debug) {
    print "# $cmd\n";
  } else {
    verbose(1, "# $cmd\n");
    system($cmd) == 0 || die "Command failed:\n$cmd\n";
  }
}

sub loadDef {
  # Read parameters from a bash script with Scott's param variable names:
  my ($def) = @_;
  open(DEF, "$def") || die "Can't open def file $def: $!\n";
  while (<DEF>) {
    s/^\s*export\s+//;
    next if (/^\s*#/ || /^\s*$/);
    if (/(\w+)\s*=\s*(.*)/) {
      my ($var, $val) = ($1, $2);
      while ($val =~ /\$(\w+)/) {
	my $subst = $defVars{$1};
	if (defined $subst) {
	  $val =~ s/\$$1/$subst/;
	} else {
	  die "Can't find value to substitute for \$$1 in $DEF var $var.\n";
	}
      }
      $defVars{$var} = $val;
    }
  }
  close(DEF);
}

sub loadSeqSizes {
  # Load up sequence -> size mapping from $sizeFile into $hashRef.
  my ($sizeFile, $hashRef) = @_;
  open(SIZE, "$sizeFile") || die "Can't open size file $sizeFile: $!\n";
  while (<SIZE>) {
    chomp;
    my ($seq, $size) = split;
    $hashRef->{$seq} = $size;
  }
  close(SIZE);
}

sub fileBase {
  # Get rid of leading path and .suffix (and .gz too if it's there).
  my ($path) = @_;
  $path =~ s/^(.*\/)?(\S+)\.\w+(\.gz)?/$2/;
  return $path;
}

# end shared stuff from blastz-run-ucsc
#########################################################################

sub verbose {
  my ($level, $message) = @_;
  print STDERR $message if ($opt_verbose >= $level);
}

sub requireVar {
  my ($var) = @_;
  die "Error: $DEF is missing variable $var\n" if (! defined $defVars{$var});
}

sub requirePath {
  my ($var) = @_;
  my $val = $defVars{$var};
  die "Error: $DEF $var=$val must specify a complete path\n"
    if ($val !~ m@^/\S+/\S+@);
}

sub requireNum {
  my ($var) = @_;
  my $val = $defVars{$var};
  die "Error: $DEF variable $var=$val must specify a number.\n"
    if ($val !~ /^\d+$/);
}

my $oldDbFormat = '[a-z][a-z](\d+)?';
my $newDbFormat = '[a-z][a-z][a-z][A-Z][a-z][a-z](\d+)?';
sub getDbFromPath {
  # Require that $val is a full path that contains a recognizable db as 
  # one of its elements (possibly the last one).
  my ($var) = @_;
  my $val = $defVars{$var};
  my $db;
  if ($val =~ m@^/\S+/($oldDbFormat|$newDbFormat)(/(\S+)?)?$@) {
    $db = $1;
  } else {
    die "Error: $DEF variable $var=$val must be a full path with " .
      "a recognizable database as one of its elements.\n"
  }
return $db;
}

sub checkDef {
  # Make sure %defVars contains what we need and looks consistent with 
  # our assumptions.  
  foreach my $s ('SEQ1_', 'SEQ2_') {
    foreach my $req ('DIR', 'LEN', 'CHUNK', 'LAP') {
      &requireVar("$s$req");
    }
    &requirePath($s . 'DIR');
    &requirePath($s . 'LEN');
    &requireNum($s . 'CHUNK');
    &requireNum($s . 'LAP');
  }
  $tDb = &getDbFromPath('SEQ1_DIR');
  $qDb = &getDbFromPath('SEQ2_DIR');
  $QDb = ucfirst($qDb);
  $isSelf = ($tDb eq $qDb);
  if ($isSelf && $opt_swap) {
    die "-swap is not supported for self-alignments\n" .
        "($DEF has $tDb as both target and query).\n";
  }
  verbose(1, "$DEF looks OK!\n" .
	  "\ttDb=$tDb\n\tqDb=$qDb\n\ts1d=$defVars{SEQ1_DIR}\n" .
	  "\tisSelf=$isSelf\n");
  if ($defVars{'BLASTZ_ABRIDGE_REPEATS'}) {
    foreach my $s ('SEQ1_', 'SEQ2_') {
      my $var = $s. 'SMSK';
      &requireVar($var);
      &requirePath($var);
    }
    verbose(1, "Abridging repeats!\n");
  }
}


sub mustMkdir {
  # mkdir || die.  Immune to -debug -- we need to create the dir structure 
  # and dump out the scripts even if we don't actually execute the scripts.
  my ($dir) = @_;
  system("mkdir -p $dir") == 0 || die "Couldn't mkdir $dir\n";
}


sub doBlastzClusterRun {
  # Set up and perform the big-cluster blastz run.
  my ($paraHub) = @_;
  my $runDir = "$buildDir/run.blastz";
  # First, make sure we're starting clean.
  if (-e "$runDir/run.time") {
    die "doBlastzClusterRun: looks like this was run successfully already " .
      "(run.time exists).  Either run with -continue cat or some later " .
	"stage, or move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doBlastzClusterRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  my $targetList = "$tDb.lst";
  my $queryList = $isSelf ? $targetList : "$qDb.lst";
  my $tPartDir = ($defVars{'SEQ1_DIR'} =~ /\.2bit$/) ? "-lstDir tParts" : '';
  my $qPartDir = ($defVars{'SEQ2_DIR'} =~ /\.2bit$/) ? "-lstDir qParts" : '';
  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : '../psl';
  my $mkOutRoot = $opt_blastzOutRoot ? "mkdir -p $opt_blastzOutRoot" : "";
  my $partitionTargetCmd = 
    ("$partition $defVars{SEQ1_CHUNK} $defVars{SEQ1_LAP} " .

     "$defVars{SEQ1_DIR} $defVars{SEQ1_LEN} -xdir xdir.sh -rawDir $outRoot " .
     "$tPartDir > $targetList");
  my $partitionQueryCmd = 
    ($isSelf ?
     '# Self-alignment ==> use target partition for both.' :
     "$partition $defVars{SEQ2_CHUNK} $defVars{SEQ2_LAP} " .
     "$defVars{SEQ2_DIR} $defVars{SEQ2_LEN} " .
     "$qPartDir > $queryList");
  my $templateCmd = ("$blastzRunUcsc -outFormat psl " .
		     ($isSelf ? '-dropSelf ' : '') .
		     '$(path1) $(path2) ../DEF ' .
		     '{check out exists ' .
		     $outRoot . '/$(file1)/$(file1)_$(file2).psl }');
  &mustMkdir($runDir);
  open (GSUB, ">$runDir/gsub")
    || die "Couldn't open $runDir/gsub for writing: $!\n";
  print GSUB  <<_EOF_
#LOOP
$templateCmd
#ENDLOOP
_EOF_
    ;
  close(GSUB);
  my $bossScript = "$runDir/doClusterRun.csh";
  open (SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It partitions target and query sequences into chunks of a sensible size 
# for cluster runs;  creates a jobList for parasol;  performs the cluster 
# run;  and checks results.  
# This script will fail if any of its commands fail.

cd $runDir
$partitionTargetCmd
$mkOutRoot
csh -ef xdir.sh
$partitionQueryCmd
$gensub2 $targetList $queryList gsub jobList
$paraRun
_EOF_
    ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $paraHub $bossScript");
}

sub doCatRun {
  # Do a small cluster run to concatenate the lowest level of chunk result 
  # files from the big cluster blastz run.  This brings results up to the 
  # next level: per-target-chunk results, which may still need to be 
  # concatenated into per-target-sequence in the next step after this one -- 
  # chaining.
  my ($paraHub) = @_;
  my $runDir = "$buildDir/run.cat";
  # First, make sure we're starting clean.
  if (-e "$runDir/run.time") {
    die "doCatRun: looks like this was run successfully already " .
      "(run.time exists).  Either run with -continue chainRun or some later " .
	"stage, or move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doCatRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/run.blastz/run.time";
  if (! -e $successFile && ! $opt_debug) {
    die "doCatRun: looks like previous stage was not successful (can't find " .
      "$successFile).\n";
  }
  &mustMkdir($runDir);

  open (GSUB, ">$runDir/gsub")
    || die "Couldn't open $runDir/gsub for writing: $!\n";
  print GSUB  <<_EOF_
#LOOP
./cat.csh \$(path1) {check out exists ../pslParts/\$(file1).psl.gz }
#ENDLOOP
_EOF_
  ;
  close(GSUB);

  open (CAT, ">$runDir/cat.csh")
    || die "Couldn't open $runDir/cat.csh for writing: $!\n";
  print CAT <<_EOF_
#!/bin/csh -ef
cat \$1/*.psl | gzip -c > \$2
_EOF_
  ;
  close(CAT);

  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : '../psl';
  my $bossScript = "$runDir/doCatRun.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It sets up and performs a small cluster run to concatenate all files in 
# each subdirectory of $outRoot into a per-target-chunk file.
# This script will fail if any of its commands fail.

cd $runDir
ls -1d $outRoot/* | sed -e 's@/\$@\@' > tParts.lst
chmod a+x cat.csh
$gensub2 tParts.lst single gsub jobList
mkdir ../pslParts
$paraRun
_EOF_
    ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $paraHub $bossScript");
}

sub doChainRun {
  # Do a small cluster run to chain alignments to each target sequence.
  my ($paraHub) = @_;
  my $runDir = "$buildDir/axtChain/run";
  # First, make sure we're starting clean.
  if (-e "$runDir/run.time") {
    die "doChainRun: looks like this was run successfully already " .
      "(run.time exists).  Either run with -continue chainMerge or some " .
	"later stage, or move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doChainRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/run.cat/run.time";
  if (! -e $successFile && ! $opt_debug) {
    die "doChainRun: looks like previous stage was not successful (can't " .
      "find $successFile).\n";
  }
  &mustMkdir($runDir);

  open (GSUB, ">$runDir/gsub")
    || die "Couldn't open $runDir/gsub for writing: $!\n";
  print GSUB  <<_EOF_
#LOOP
chain.csh \$(root1) {check out line+ chain/\$(root1).chain}
#ENDLOOP
_EOF_
  ;
  close(GSUB);

  my $matrix = $defVars{'BLASTZ_Q'} ? "-scoreScheme=$defVars{BLASTZ_Q} " : "";
  open (CHAIN, ">$runDir/chain.csh")
    || die "Couldn't open $runDir/chain.csh for writing: $!\n";
  print CHAIN  <<_EOF_
#!/bin/csh -ef
zcat ../../pslParts/\$1.*.psl.gz \\
| axtChain -psl -verbose=0 $matrix stdin \\
    $defVars{SEQ1_DIR} \\
    $defVars{SEQ2_DIR} \\
    stdout \\
| chainAntiRepeat $defVars{SEQ1_DIR} \\
    $defVars{SEQ2_DIR} \\
    stdin \$2
_EOF_
    ;
  close(CHAIN);

  my $bossScript = "$runDir/doChainRun.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It sets up and performs a small cluster run to chain all alignments 
# to each target sequence.
# This script will fail if any of its commands fail.

cd $runDir
awk '{print \$1;}' $defVars{'SEQ1_LEN'} > chrom.lst
chmod a+x chain.csh
gensub2 chrom.lst single gsub jobList
mkdir chain
$paraRun
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $paraHub $bossScript");
}


sub postProcessChains {
  # chainMergeSort etc.
  my ($workhorse, $fileServer) = @_;
  my $runDir = "$buildDir/axtChain";
  my $chain = "$tDb.$qDb.all.chain.gz";
  # First, make sure we're starting clean.
  if (-e "$runDir/$chain") {
    die "postProcessChains: looks like this was run successfully already " .
      "($chain exists).  Either run with -continue net or some later " .
      "stage, or move aside/remove $runDir/$chain and run again.\n";
  } elsif (-e "$runDir/all.chain" || -e "$runDir/all.chain.gz") {
    die "postProcessChains: looks like this was run successfully already " .
      "(all.chain[.gz] exists).  Either run with -continue net or some later " .
      "stage, or move aside/remove $runDir/all.chain[.gz] and run again.\n";
  } elsif (-e "$runDir/chain" && ! $opt_debug) {
    die "postProcessChains: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/chain and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/axtChain/run/run.time";
  if (! -e $successFile && ! $opt_debug) {
    die "postProcessChains: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  &run("ssh -x $workhorse nice " .
       "'chainMergeSort $runDir/run/chain/*.chain " .
       "| gzip -c > $runDir/$chain'");
  if ($splitRef) {
    &run("ssh -x $fileServer nice " .
	 "chainSplit $runDir/chain $runDir/$chain");
  }
}


sub getAllChain {
  # Find the most likely candidate for all.chain from a previous run/step.
  my ($runDir) = @_;
  my $chain;
  if (-e "$runDir/$tDb.$qDb.all.chain.gz") {
    $chain = "$tDb.$qDb.all.chain.gz";
  } elsif (-e "$runDir/$tDb.$qDb.all.chain") {
    $chain = "$tDb.$qDb.all.chain";
  } elsif (-e "$runDir/all.chain.gz") {
    $chain = "all.chain.gz";
  } elsif (-e "$runDir/all.chain") {
    $chain = "all.chain";
  }
  return $chain;
}


sub swapChains {
  # chainMerge step for -swap: chainSwap | chainSort.
  my ($workhorse, $fileServer) = @_;
  my $runDir = "$swapDir/axtChain";
  my $inChain = &getAllChain("$buildDir/axtChain");
  my $swappedChain = "$qDb.$tDb.all.chain.gz";
  # First, make sure we're starting clean.
  if (-e "$runDir/$swappedChain") {
    die "swapChains: looks like this was run successfully already " .
     "($runDir/$swappedChain exists).  Either run with -continue net or some " .
     "later stage, or move aside/remove $runDir/$swappedChain and run again.\n";
  } elsif (-e "$runDir/all.chain" || -e "$runDir/all.chain.gz") {
    die "swapChains: looks like this was run successfully already " .
     "($runDir/all.chain[.gz] exists).  Either run with -continue net or some " .
     "later stage, or move aside/remove $runDir/all.chain[.gz] and run again.\n";
  }
  # Main routine already made sure that $buildDir/axtChain/all.chain is there.
  &run("ssh -x $workhorse nice " .
       "'chainSwap $buildDir/axtChain/$inChain stdout " .
       "| nice chainSort stdin stdout " .
       "| nice gzip -c > $runDir/$swappedChain'");
  if ($splitRef) {
    &run("ssh -x $fileServer nice " .
	 "chainSplit $runDir/chain $runDir/$swappedChain");
  }
}


sub swapGlobals {
  # Swap our global variables ($buildDir, $tDb, $qDb and %defVars SEQ1/SEQ2)
  # so that the remaining steps need no tweaks for -swap.
  $buildDir = $swapDir;
  my $tmp = $qDb;
  $qDb = $tDb;
  $tDb = $tmp;
  $QDb = ucfirst($qDb);
  foreach my $var ('DIR', 'LEN', 'CHUNK', 'LAP', 'SMSK') {
    $tmp = $defVars{"SEQ1_$var"};
    $defVars{"SEQ1_$var"} = $defVars{"SEQ2_$var"};
    $defVars{"SEQ2_$var"} = $tmp;
  }
  $defVars{'BASE'} = $swapDir;
}


sub netChains {
  # Turn chains into nets (,axt,maf,.over.chain).
  my ($machine) = @_;
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-d "$buildDir/mafNet") {
    die "netChains: looks like this was run successfully already " .
      "(mafNet exists).  Either run with -continue load or some later " .
	"stage, or move aside/remove $buildDir/mafNet " . 
	  "and $runDir/noClass.net and run again.\n";
  } elsif (-e "$runDir/noClass.net") {
    die "netChains: looks like we are not starting with a " .
      "clean slate.  Please move aside or remove $runDir/noClass.net " .
	"and run again.\n";
  }
  # Make sure previous stage was successful.
  my $chain = &getAllChain($runDir);
  if (! defined $chain && ! $opt_debug) {
    die "netChains: looks like previous stage was not successful " .
      "(can't find [$tDb.$qDb.]all.chain[.gz]).\n";
  }
  my $over = "$tDb.$qDb.over.chain.gz";
  my $bedOverDir = "$clusterData/$tDb/$trackBuild/bedOver";
  my $liftOver = ($isSelf ? "# No liftOver chains for self-alignments." :
		  "# Make liftOver chains:\n" .
		  "netChainSubset -verbose=0 noClass.net $chain stdout " .
		  "| chainSort stdin stdout | gzip -c > $over\n" .
		  "mkdir -p $bedOverDir\n" .
		  "cp -p $over $bedOverDir/$over");
  my $bossScript = "$buildDir/axtChain/netChains.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $machine in $runDir .
# It generates nets (without repeat/gap stats -- those are added later on 
# $dbHost) from chains, and generates axt, maf and .over.chain from the nets.
# This script will fail if any of its commands fail.

cd $runDir

# Make nets ("noClass", i.e. without rmsk/class stats which are added later):
chainPreNet $chain $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} stdout \\
| chainNet stdin -minSpace=1 $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} stdout /dev/null \\
| netSyntenic stdin noClass.net

$liftOver

_EOF_
    ;
  if ($splitRef) {
    print SCRIPT <<_EOF_
# Make axtNet for download: one .axt per $tDb seq.
netSplit noClass.net net
cd ..
mkdir axtNet
foreach f (axtChain/net/*.net)
netToAxt \$f axtChain/chain/\$f:t:r.chain \\
  $defVars{SEQ1_DIR} $defVars{SEQ2_DIR} stdout \\
  | axtSort stdin stdout \\
  | gzip -c > axtNet/\$f:t:r.$tDb.$qDb.net.axt.gz
end

# Make mafNet for multiz: one .maf per $tDb seq.
mkdir mafNet
foreach f (axtNet/*.$tDb.$qDb.net.axt.gz)
  axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. \$f \\
        $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} \\
        stdout \\
  | gzip -c > mafNet/\$f:t:r:r:r:r:r.maf.gz
end
_EOF_
      ;
  } else {
    print SCRIPT <<_EOF_
# Make axtNet for download: one .axt for all of $tDb.
mkdir ../axtNet
netToAxt -verbose=0 noClass.net $chain \\
  $defVars{SEQ1_DIR} $defVars{SEQ2_DIR} stdout \\
| axtSort stdin stdout \\
| gzip -c > ../axtNet/$tDb.$qDb.net.axt.gz

# Make mafNet for multiz: one .maf for all of $tDb.
mkdir ../mafNet
axtToMaf -tPrefix=$tDb. -qPrefix=$qDb. ../axtNet/$tDb.$qDb.net.axt.gz \\
  $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} \\
  stdout \\
| gzip -c > ../mafNet/$tDb.$qDb.net.maf.gz
_EOF_
      ;
  }
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $machine nice $bossScript");
}


sub loadUp {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-e "$runDir/$tDb.$qDb.net" || -e "$runDir/$tDb.$qDb.net.gz") {
    die "netChains: looks like this was run successfully already " .
      "($tDb.$qDb.net[.gz] exists).  Either run with -continue download, " .
	"or move aside/remove $runDir/$tDb.$qDb.net[.gz] and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successDir = "$buildDir/mafNet/";
  if (! -d $successDir && ! $opt_debug) {
    die "netChains: looks like previous stage was not successful " .
      "(can't find $successDir).\n";
  }
  my $bossScript = "$buildDir/axtChain/loadUp.csh";
  my $chain = $isSelf ? 'chainSelf' : "chain$QDb";
  my $net = $isSelf ? 'netSelf' : "net$QDb";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $dbHost in $runDir .
# It loads the chain tables into $tDb, adds gap/repeat stats to the .net file,
# and loads the net table.
# This script will fail if any of its commands fail.

# Load chains:
_EOF_
    ;
  if ($splitRef) {
    print SCRIPT <<_EOF_
cd $runDir/chain
foreach f (*.chain)
    set c = \$f:r
    hgLoadChain $tDb \${c}_$chain \$f
end
_EOF_
      ;
  } else {
    print SCRIPT <<_EOF_
cd $runDir
hgLoadChain -tIndex $tDb $chain $tDb.$qDb.all.chain.gz
_EOF_
      ;
  }
  print SCRIPT <<_EOF_

# Add gap/repeat stats to the net file using database tables:
cd $runDir
netClass -verbose=0 -noAr noClass.net $tDb $qDb $tDb.$qDb.net

# Load nets:
netFilter -minGap=10 $tDb.$qDb.net \\
| hgLoadNet -verbose=0 $tDb $net stdin
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $dbHost nice $bossScript");
# maybe also peek in trackDb and see if entries need to be added for chain/net
}


sub makeDownloads {
  # Compress the netClassed .net for download (other files should have been
  # compressed already).
  my ($machine) = @_;
  my $runDir = "$buildDir/axtChain";
  if (-e "$runDir/$tDb.$qDb.net") {
    &run("ssh -x $machine nice " .
	 "gzip $runDir/$tDb.$qDb.net");
  }
}


sub installDownloads {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  my $bossScript = "$buildDir/axtChain/installDownloads.csh";
  my $vs = $isSelf ? 'vsSelf' : "vs$QDb";
  # Make sure previous stage was successful.
  my $successFile = "$runDir/$tDb.$qDb.net.gz";
  if (! -e $successFile && ! $opt_debug) {
    die "installDownloads: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  my $axt = ($splitRef ?
	     "mkdir axtNet\n" . "ln -s $buildDir/axtNet/*.axt.gz axtNet/" :
	     "ln -s $buildDir/axtNet/$tDb.$qDb.net.axt.gz .");
  my $over = "$tDb.$qDb.over.chain.gz";
  my $liftOver = ($isSelf ? '# No liftOver chains for self-alignments.' :
		  "mkdir -p $goldenPath/$tDb/liftOver\n" .
		  "cp -p $buildDir/axtChain/$over " .
		  "$goldenPath/$tDb/liftOver/$over\n" .
		  "ln -s $runDir/$tDb.$qDb.over.chain.gz .");
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $dbHost in $runDir .
# It creates the download directory for chains, nets and axtNet,
# and copies the liftOver chains (if applicable) to the liftOver download dir.
# This script will fail if any of its commands fail.

mkdir $goldenPath/$tDb/$vs
cd $goldenPath/$tDb/$vs
ln -s $runDir/$tDb.$qDb.all.chain.gz .
ln -s $runDir/$tDb.$qDb.net.gz .

$axt

$liftOver

md5sum *.gz */*.gz > md5sum.txt
_EOF_
      ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $dbHost nice $bossScript");
  print STDERR "Reminder: make $downloadPath/README.txt.\n"
# maybe also peek in trackDb and see if entries need to be added for chain/net
}


sub cleanup {
  # Remove intermediate files.
  my ($machine) = @_;
  my $runDir = $buildDir;
  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : "$buildDir/psl";
  my $rootCanal = ($opt_blastzOutRoot ?
		   "rmdir --ignore-fail-on-non-empty $opt_blastzOutRoot" :
		   '');
  my $bossScript = "$buildDir/cleanUp.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $machine in $runDir .
# It cleans up files after a successful blastz/chain/net/install series.
# This script will fail if any of its commands fail, but uses rm -f so
# individual failures should be ignored (e.g. if a partial cleanup has 
# already been performed).

cd $runDir

rm -fr $outRoot/
$rootCanal
rm -fr $buildDir/axtChain/run/chain/
rm -f  $buildDir/axtChain/noClass.net
_EOF_
    ;
  if ($splitRef) {
    print SCRIPT <<_EOF_
rm -fr $buildDir/axtChain/net/
rm -fr $buildDir/axtChain/chain/
_EOF_
      ;
  } else {
    print SCRIPT <<_EOF_
rm -f  $buildDir/axtChain/*.tab
_EOF_
      ;
  }
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $machine nice $bossScript");
}


#########################################################################
#
# -- main --

# Sometimes it gets hung on tty input at the very end of run.cat/doCatRun.csh
# (after completing the para run), sometimes it doesn't...?  To get around
# that, just redirect stdin to be from /dev/null (just closing it causes the
# stdin filehandle to be reused for some files that we open for writing later,
# which prompts warnings).
close(STDIN);
open(STDIN, '/dev/null');

&checkOptions();

&usage(1) if (scalar(@ARGV) != 1);
($DEF) = @ARGV;

&loadDef($DEF);
&checkDef();

my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $defVars{'BASE'} ||
  "$clusterData/$tDb/$trackBuild/blastz.$qDb.$date";

if ($opt_swap) {
  my $inChain = &getAllChain("$buildDir/axtChain");
  if (! defined $inChain) {
    die "-swap: Can't find $buildDir/axtChain/[$tDb.$qDb.]all.chain[.gz]\n" .
        "which is required for -swap.\n";
  }
  $swapDir = "$clusterData/$qDb/$trackBuild/blastz.$tDb.swap";
  &mustMkdir("$swapDir/axtChain");
  $splitRef = (`wc -l < $defVars{SEQ2_LEN}` < $splitThreshold);
  &verbose(1, "Swapping from $buildDir/axtChain/$inChain\n" .
	      "to $swapDir/axtChain/$qDb.$tDb.all.chain.gz .\n");
} else {
  if (! -d $buildDir) {
    &mustMkdir($buildDir);
  }
  if (! $opt_blastzOutRoot && $startStep < $stepVal{'chainRun'}) {
    &enforceClusterNoNo($buildDir,
	    'blastz/chain/net build directory (or use -blastzOutRoot)');
  }
  $splitRef = (`wc -l < $defVars{SEQ1_LEN}` < $splitThreshold);
  &verbose(1, "Building in $buildDir\n");
}

if (! -e "$buildDir/DEF") {
  &run("cp $DEF $buildDir/DEF");
}

my $fileServer = $opt_swap ? `$getFileServer $swapDir/` :
                             `$getFileServer $buildDir/`;
chomp $fileServer;

my $step = 0;
if ($startStep <= $step && $step <= $stopStep) {
  &doBlastzClusterRun($bigClusterHub);
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &doCatRun($smallClusterHub);
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &doChainRun($smallClusterHub);
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  if ($opt_swap) {
    &swapChains($workhorse, $fileServer);
  } else {
    &postProcessChains($workhorse, $fileServer);
  }
}

&swapGlobals() if ($opt_swap);

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &netChains($workhorse);
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &loadUp();
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &makeDownloads($workhorse);
  &installDownloads();
}

$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &cleanup($fileServer);
}

verbose(1, "
 *** All done!
 *** Remember to add the new chain and net tracks to trackDb.ra
 *** if necessary, and make a download README.txt.

\n");

