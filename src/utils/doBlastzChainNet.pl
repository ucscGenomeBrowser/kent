#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/doBlastzChainNet.pl instead.

# $Id: doBlastzChainNet.pl,v 1.40 2006/05/23 21:15:41 angie Exp $

# to-do items:
# - lots of testing
# - better logging: right now it just passes stdout and stderr,
#   leaving redirection to a logfile up to the user
# - -swapBlastz, -loadBlastz
# - -tDb, -qDb
# - -tUnmasked, -qUnmasked
# - -axtBlastz
# - another Gill wish list item: save a lav header (involves run-blastz-ucsc)
# - 2bit / multi-sequence support when abridging?
# - reciprocal best?
# - hgLoadSeq of query instead of assuming there's a $qDb database?

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

# Hardcoded paths/command sequences:
my $getFileServer = '/cluster/bin/scripts/fileServer';
my $blastzRunUcsc = '/cluster/bin/scripts/blastz-run-ucsc';
my $partition = '/cluster/bin/scripts/partitionSequence.pl';
my $gensub2 = '/parasol/bin/gensub2';
my $para = '/parasol/bin/para';
my $paraRun = ("$para make jobList\n" .
	       "$para check\n" .
	       "$para time > run.time\n" .
	       'cat run.time');
my $dbHost = 'hgwdev';
my $centralDbSql = "ssh -x $dbHost hgsql -h genome-testdb -A -N hgcentraltest";
my $clusterData = '/cluster/data';
my $trackBuild = 'bed';
my $goldenPath = '/usr/local/apache/htdocs/goldenPath';
my $downloadPath = "$dbHost:$goldenPath";
my $gbdb = '/gbdb';
my $clusterLocal = '/scratch/hg';
my $clusterSortaLocal = '/iscratch/i';
my @clusterNAS = ('/cluster/bluearc', '/panasas/store', '/san/sanvol1');
my $clusterNAS = join('/... or ', @clusterNAS) . '/...';
my @clusterNoNo = ('/cluster/home', '/projects');
my @fileServerNoNo = ('kkhome', 'kks00');
my @fileServerNoLogin = ('kkusr01', '10.1.1.3', '10.1.10.11',
			 'sanhead1', 'sanhead2', 'sanhead3', 'sanhead4',
			 'sanhead5', 'sanhead6', 'sanhead7', 'sanhead8');
my $splitThreshold = 100;

# Option variable names:
use vars qw/
    $opt_continue
    $opt_stop
    $opt_blastzOutRoot
    $opt_swap
    $opt_chainMinScore
    $opt_chainLinearGap
    $opt_workhorse
    $opt_fileServer
    $opt_bigClusterHub
    $opt_smallClusterHub
    $opt_debug
    $opt_verbose
    $opt_readmeOnly
    $opt_help
    /;

# Numeric values of -continue/-stop options for determining precedence:
my %stepVal = ( 'partition' => 0,
                'blastz' => 1,
		'cat' => 2,
		'chainRun' => 3,
		'chainMerge' => 4,
		'net' => 5,
		'load' => 6,
		'download' => 7,
		'cleanup' => 8,
	      );

# Option defaults:
my $bigClusterHub = 'kk';
my $smallClusterHub = 'kki';
my $workhorse = 'kolossus';
my $defaultVerbose = 1;
my $defaultChainLinearGap = "loose";
my $defaultChainMinScore = "1000";	# from axtChain itself

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
    -chainMinScore n      Add -minScore=n (default: $defaultChainMinScore) to the
                                  axtChain command.
    -chainLinearGap type  Add -linearGap=<loose|medium|filename> to the
                                  axtChain command.  (default: loose)
    -workhorse machine    Use machine (default: $workhorse) for compute or
                          memory-intensive steps.
    -fileServer mach      Use mach (default: fileServer of the build directory)
                          for I/O-intensive steps.
    -bigClusterHub mach   Use mach (default: $bigClusterHub) as parasol hub
                          for blastz cluster run.
    -smallClusterHub mach Use mach (default: $smallClusterHub) as parasol hub
                          for cat & chain cluster runs.
    -debug                Don't actually run commands, just display them.
    -verbose num          Set verbose level to num (default $defaultVerbose).
    -help                 Show detailed help and exit.
Automates UCSC's blastz/chain/net pipeline:
    1. Big cluster run of blastz.
    2. Small cluster consolidation of blastz result files.
    3. Small cluster chaining run.
    4. Sorting and netting of chains on the fileserver
       (no nets for self-alignments).
    5. Generation of liftOver-suitable chains from nets+chains on fileserver
       (not done for self-alignments).
    6. Generation of axtNet and mafNet files on the fileserver (not for self).
    7. Addition of gap/repeat info to nets on hgwdev (not for self).
    8. Loading of chain and net tables on hgwdev (no nets for self).
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
   LiftOver chains (not applicable for self-alignments) go in this file:
   $clusterData/\$tDb/$trackBuild/liftOver/\$tDbTo\$QDb.over.chain.gz
   a copy is kept here (in case the liftOver/ copy is overwritten):
   $clusterData/\$tDb/$trackBuild/blastz.\$qDb.\$date/\$tDb.\$qDb.over.chain.gz
   and symbolic links to the liftOver/ file are put here:
   $downloadPath/\$tDb/liftOver/\$tDbTo\$QDb.over.chain.gz
   $dbHost:$gbdb/\$tDb/liftOver/\$tDbTo\$QDb.over.chain.gz
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
   or $clusterNAS
   because it will be a large file and it is read by blastz-run-ucsc 
   (big cluster script).
   SEQ2_LEN: ditto for query.
5. DEF's SEQ1_CHUNK and SEQ1_LAP determine the step size and overlap size 
   of chunks into which large target sequences are to be split before 
   alignment.  SEQ2_CHUNK and SEQ2_LAP: ditto for query.
6. DEF's SEQ1_LIMIT and SEQ2_LIMIT decide what the maximum number of 
   sequences should be for any partitioned file (the files created in the
   tParts and qParts directories).  This limit only effects SEQ1 or SEQ2
   when they are 2bit files.  Some 2bit files have too many contigs.  This
   reduces the number of blastz hippos (jobs taking forever compared to 
   the other jobs).
7. DEF's BLASTZ_ABRIDGE_REPEATS should be set to something nonzero if 
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
8. DEF's BLASTZ_[A-Z] variables will be translated into blastz command line 
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
9. DEF's PATH variable, if set, must specify a path that contains programs 
   necessary for blastz to run: blastz, and if BLASTZ_ABRIDGE_REPEATS is set, 
   then also fasta_subseq, strip_rpts, restore_rpts, and revcomp.  
   If DEF does not contain a PATH, blastz-run-ucsc will use its own default.
10. DEF's BLASTZ variable can specify an alternate path for blastz.
11. DEF's BASE variable can specify the blastz/chain/net build directory 
    (defaults to $clusterData/\$tDb/$trackBuild/blastz.\$qDb.\$date/).
12. SEQ?_CTGDIR specifies sequence source with the contents of full chrom
    sequences and the contig randoms and chrUn.  This keeps the contigs
    separate during the blastz and chaining so that chains won't go through
    across multiple contigs on the randoms.
13. SEQ?_CTGLEN specifies a length file to be used in conjunction with the
    special SEQ?_CTGDIR file specified above which contains the random contigs.
14. SEQ?_LIFT specifies a lift file to lift sequences in the SEQ?_CTGDIR
    to their random and chrUn positions.  This is useful for a 2bit file that
    has both full chrom sequences and the contigs for the randoms.
15. SEQ2_SELF=1 specifies the SEQ2 is already specially split for self
    alignments and to use SEQ2 sequence for self alignment, not just a
    copy of SEQ1
16. TMPDIR - specifies directory on cluster node to keep temporary files
    Typically TMPDIR=/scratch/tmp
17. All other variables in DEF will be ignored!

" if ($detailed);
  exit $status;
}


# Globals:
my %defVars = ();
my ($DEF, $tDb, $qDb, $QDb, $isSelf, $selfSplit, $buildDir, $startStep, $stopStep);
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
		      "chainMinScore=i",
		      "chainLinearGap=s",
		      "workhorse=s",
		      "fileServer=s",
		      "bigClusterHub=s",
		      "smallClusterHub=s",
		      "verbose=n",
		      "debug",
		      "readmeOnly",
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
  $bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
  $smallClusterHub = $opt_smallClusterHub if ($opt_smallClusterHub);
}

#########################################################################
# The following routines were taken almost verbatim from blastz-run-ucsc,
# so may be good candidates for libification!  unless that would slow down 
# blastz-run-ucsc...
# nfsNoodge() was removed from loadDef() and loadSeqSizes() -- since this 
# script will not be run on the cluster, we should fully expect files to 
# be immediately visible.  

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

# end shared stuff from blastz-run-ucsc
#########################################################################

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
  if ($val =~ m@^/\S+/($oldDbFormat|$newDbFormat)((\.2bit)|(/(\S+)?))?$@) {
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
  $isSelf = ($tDb eq $qDb);
  # special split on SEQ2 for Self alignments
  $selfSplit = $defVars{'SEQ2_SELF'} || 0;
  $QDb = $isSelf ? 'Self' : ucfirst($qDb);
  if ($isSelf && $opt_swap) {
    die "-swap is not supported for self-alignments\n" .
        "($DEF has $tDb as both target and query).\n";
  }
  HgAutomate::verbose(1, "$DEF looks OK!\n" .
	  "\ttDb=$tDb\n\tqDb=$qDb\n\ts1d=$defVars{SEQ1_DIR}\n" .
	  "\tisSelf=$isSelf\n");
  if ($defVars{'SEQ1_SMSK'} || $defVars{'SEQ2_SMSK'} ||
      $defVars{'BLASTZ_ABRIDGE_REPEATS'}) {
    &requireVar('BLASTZ_ABRIDGE_REPEATS');
    foreach my $s ('SEQ1_', 'SEQ2_') {
      my $var = $s. 'SMSK';
      &requireVar($var);
      &requirePath($var);
    }
    HgAutomate::verbose(1, "Abridging repeats!\n");
  }
}


sub doPartition{ 
  # Partition the sequence up before blastz.
  my ($paraHub, $fileServer) = @_;
  my $runDir = "$buildDir/run.blastz";
  my $targetList = "$tDb.lst";
  my $queryList = $isSelf ? $targetList : "$qDb.lst";
  if ($selfSplit) {
    $queryList = "$qDb.selfSplit.lst"
  }
  my $tPartDir = '-lstDir tParts';
  my $qPartDir = '-lstDir qParts';
  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : '../psl';
  my $mkOutRoot = $opt_blastzOutRoot ? "mkdir -p $opt_blastzOutRoot" : "";

  my $seq1Dir = $defVars{'SEQ1_CTGDIR'} || $defVars{'SEQ1_DIR'};
  my $seq2Dir = $defVars{'SEQ2_CTGDIR'} || $defVars{'SEQ2_DIR'};
  my $seq1Len = $defVars{'SEQ1_CTGLEN'} || $defVars{'SEQ1_LEN'};
  my $seq2Len = $defVars{'SEQ2_CTGLEN'} || $defVars{'SEQ2_LEN'};
  my $seq1Limit = $defVars{'SEQ1_LIMIT'} || 0;
  my $seq2Limit = $defVars{'SEQ2_LIMIT'} || 0;

  my $partitionTargetCmd = 
    ("$partition $defVars{SEQ1_CHUNK} $defVars{SEQ1_LAP} " .
     "$seq1Dir $seq1Len -xdir xdir.sh -rawDir $outRoot $seq1Limit " .
     "$tPartDir > $targetList");
  my $partitionQueryCmd = 
    (($isSelf && (! $selfSplit)) ?
     '# Self-alignment ==> use target partition for both.' :
     "$partition $defVars{SEQ2_CHUNK} $defVars{SEQ2_LAP} " .
     "$seq2Dir $seq2Len $seq2Limit " .
     "$qPartDir > $queryList");
  my $bossScript = "$runDir/doPartition.csh";
  &HgAutomate::mustMkdir($runDir);
  open (SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It partitions target and query sequences into chunks of a sensible size 
# for cluster runs.  
# This script will fail if any of its commands fail.

cd $runDir
$partitionTargetCmd
$mkOutRoot
$partitionQueryCmd
_EOF_
    ;
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $paraHub $bossScript");
  &HgAutomate::nfsNoodge("$outRoot");
  &HgAutomate::run("ssh -x $fileServer '(cd $runDir; csh -ef xdir.sh)'");
}

sub doBlastzClusterRun {
  # Set up and perform the big-cluster blastz run.
  my ($paraHub) = @_;
  my $runDir = "$buildDir/run.blastz";
  my $targetList = "$tDb.lst";
  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : '../psl';
  my $queryList = $isSelf ? $targetList : "$qDb.lst";
  if ($selfSplit) {
    $queryList = "$qDb.selfSplit.lst"
  }
  # First, make sure we're starting clean.
  if (-e "$runDir/run.time") {
    die "doBlastzClusterRun: looks like this was run successfully already " .
      "(run.time exists).  Either run with -continue cat or some later " .
	"stage, or move aside/remove $runDir/ and run again.\n";
  } elsif ((-e "$runDir/gsub" || -e "$runDir/jobList") && ! $opt_debug) {
    die "doBlastzClusterRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  # Second, make sure we got through the partitioning already
  if (! -e "$runDir/$targetList" && ! $opt_debug) {
    die "doBlastzClusterRun: there's no target list file " .
        "so start over without the -continue align.\n";
  }
  if (! -e "$runDir/$queryList" && ! $opt_debug) {
    die "doBlastzClusterRun: there's no query list file" .
        "so start over without the -continue align.\n";
  }
  my $templateCmd = ("$blastzRunUcsc -outFormat psl " .
		     ($isSelf ? '-dropSelf ' : '') .
		     '$(path1) $(path2) ../DEF ' .
		     '{check out exists ' .
		     $outRoot . '/$(file1)/$(file1)_$(file2).psl }');
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
$gensub2 $targetList $queryList gsub jobList
$paraRun
_EOF_
    ;
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $paraHub $bossScript");
}	#	sub doBlastzClusterRun {}

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
  &HgAutomate::mustMkdir($runDir);

  open (GSUB, ">$runDir/gsub")
    || die "Couldn't open $runDir/gsub for writing: $!\n";
  print GSUB  <<_EOF_
#LOOP
./cat.csh \$(path1) {check out exists ../pslParts/\$(file1).psl.gz }
#ENDLOOP
_EOF_
  ;
  close(GSUB);

  my $outRoot = $opt_blastzOutRoot ? "$opt_blastzOutRoot/psl" : '../psl';

  open (CAT, ">$runDir/cat.csh")
    || die "Couldn't open $runDir/cat.csh for writing: $!\n";
  print CAT <<_EOF_
#!/bin/csh -ef
find $outRoot/\$1/ -name "*.psl" | xargs cat | gzip -c > \$2
_EOF_
  ;
  close(CAT);

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
(cd $outRoot; find . -type d -maxdepth 1 | grep '^./') \\
        | sed -e 's#/\$##; s#^./##' > tParts.lst
chmod a+x cat.csh
$gensub2 tParts.lst single gsub jobList
mkdir ../pslParts
$paraRun
_EOF_
    ;
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $paraHub $bossScript");
}	#	sub doCatRun {}


sub makePslPartsLst {
  # Create a pslParts.lst file the subdirectories of pslParts; if some
  # are for subsequences of the same sequence, make a single .lst line
  # for the sequence (single chaining job with subseqs' alignments
  # catted together).  Otherwise (i.e. subdirs that contain small
  # target seqs glommed together by partitionSequences) make one .lst
  # line per partition.
  return if ($opt_debug);
  opendir(P, "$buildDir/pslParts")
    || die "Couldn't open directory $buildDir/pslParts for reading: $!\n";
  my @parts = readdir(P);
  closedir(P);
  my $partsLst = "$buildDir/axtChain/run/pslParts.lst";
  open(L, ">$partsLst") || die "Couldn't open $partsLst for writing: $!\n";
  my %seqs = ();
  my $count = 0;
  foreach my $p (@parts) {
    $p =~ s@^/.*/@@;  $p =~ s@/$@@;
    $p =~ s/\.psl\.gz//;
    next if ($p eq '.' || $p eq '..');
    if ($p =~ m@^(\S+:\S+):\d+-\d+$@) {
      # Collapse subsequences (subranges of a sequence) down to one entry
      # per sequence:
      $seqs{$1} = 1;
    } else {
      print L "$p\n";
      $count++;
    }
  }
  foreach my $p (keys %seqs) {
    print L "$p:\n";
    $count++;
  }
  close(L);
  if ($count < 1) {
    die "makePslPartsLst: didn't find any pslParts/ items.";
  }
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
  &HgAutomate::mustMkdir($runDir);

  open (GSUB, ">$runDir/gsub")
    || die "Couldn't open $runDir/gsub for writing: $!\n";
  print GSUB  <<_EOF_
#LOOP
chain.csh \$(file1) {check out line+ chain/\$(file1).chain}
#ENDLOOP
_EOF_
  ;
  close(GSUB);

  my $seq1Dir = $defVars{'SEQ1_CTGDIR'} || $defVars{'SEQ1_DIR'};
  my $seq2Dir = $defVars{'SEQ2_CTGDIR'} || $defVars{'SEQ2_DIR'};
  my $matrix = $defVars{'BLASTZ_Q'} ? "-scoreScheme=$defVars{BLASTZ_Q} " : "";
  my $minScore = $opt_chainMinScore ? "-minScore=$opt_chainMinScore" : "";
  my $linearGap = $opt_chainLinearGap ? "-linearGap=$opt_chainLinearGap" :
	"-linearGap=$defaultChainLinearGap";
  open (CHAIN, ">$runDir/chain.csh")
    || die "Couldn't open $runDir/chain.csh for writing: $!\n";
  print CHAIN  <<_EOF_
#!/bin/csh -ef
zcat ../../pslParts/\$1*.psl.gz \\
| axtChain -psl -verbose=0 $matrix $minScore $linearGap stdin \\
    $seq1Dir \\
    $seq2Dir \\
    stdout \\
| chainAntiRepeat $seq1Dir \\
    $seq2Dir \\
    stdin \$2
_EOF_
    ;
  if (exists($defVars{'SEQ1_LIFT'})) {
  print CHAIN <<_EOF_
set c=\$2:t:r
echo "lifting \$2 to \${c}.lifted.chain"
liftUp liftedChain/\${c}.lifted.chain \\
    $defVars{'SEQ1_LIFT'} carry \$2
rm \$2
mv liftedChain/\${c}.lifted.chain \$2
_EOF_
    ;
  }
  if (exists($defVars{'SEQ2_LIFT'})) {
  print CHAIN <<_EOF_
set c=\$2:t:r
echo "lifting \$2 to \${c}.lifted.chain"
liftUp -chainQ liftedChain/\${c}.lifted.chain \\
    $defVars{'SEQ2_LIFT'} carry \$2
rm \$2
mv liftedChain/\${c}.lifted.chain \$2
_EOF_
    ;
  }
  close(CHAIN);

  &makePslPartsLst();

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
chmod a+x chain.csh
$gensub2 pslParts.lst single gsub jobList
mkdir chain liftedChain
$paraRun
rmdir liftedChain
_EOF_
  ;
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $paraHub $bossScript");
}	#	sub doChainRun {}


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
  &HgAutomate::run("ssh -x $workhorse nice " .
       "'chainMergeSort $runDir/run/chain/*.chain " .
       "| nice gzip -c > $runDir/$chain'");
  if ($splitRef) {
    &HgAutomate::run("ssh -x $fileServer nice " .
	 "chainSplit $runDir/chain $runDir/$chain");
  }
  &HgAutomate::nfsNoodge("$runDir/$chain");
}	#	sub postProcessChains {}


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
  } elsif ($opt_debug) {
    $chain = "$runDir/$tDb.$qDb.all.chain.gz";
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
  &HgAutomate::run("ssh -x $workhorse nice " .
       "'chainSwap $buildDir/axtChain/$inChain stdout " .
       "| nice chainSort stdin stdout " .
       "| nice gzip -c > $runDir/$swappedChain'");
  &HgAutomate::nfsNoodge("$runDir/$swappedChain");
  if ($splitRef) {
    &HgAutomate::run("ssh -x $fileServer nice " .
	 "chainSplit $runDir/chain $runDir/$swappedChain");
  }
}	#	sub swapChains {}


sub swapGlobals {
  # Swap our global variables ($buildDir, $tDb, $qDb and %defVars SEQ1/SEQ2)
  # so that the remaining steps need no tweaks for -swap.
  $buildDir = $swapDir;
  my $tmp = $qDb;
  $qDb = $tDb;
  $tDb = $tmp;
  $QDb = $isSelf ? 'Self' : ucfirst($qDb);
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
  # Don't do this for self alignments.
  if ($isSelf) {
    die "program error: netChains should not be called when $isSelf.";
  }
  my $over = $tDb . "To$QDb.over.chain.gz";
  my $altOver = "$tDb.$qDb.over.chain.gz";
  my $liftOverDir = "$clusterData/$tDb/$trackBuild/liftOver";
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

# Make liftOver chains:
netChainSubset -verbose=0 noClass.net $chain stdout \\
| chainSort stdin stdout | gzip -c > $altOver
mkdir -p $liftOverDir
cp -p $altOver $liftOverDir/$over

_EOF_
    ;
  my $seq1Dir = $defVars{'SEQ1_DIR'};
  my $seq2Dir = $defVars{'SEQ2_DIR'};
  if ($splitRef) {
    print SCRIPT <<_EOF_
# Make axtNet for download: one .axt per $tDb seq.
netSplit noClass.net net
cd ..
mkdir axtNet
foreach f (axtChain/net/*.net)
netToAxt \$f axtChain/chain/\$f:t:r.chain \\
  $seq1Dir $seq2Dir stdout \\
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
  $seq1Dir $seq2Dir stdout \\
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
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $machine nice $bossScript");
}	#	sub netChains {}


sub loadUp {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-e "$runDir/$tDb.$qDb.net" || -e "$runDir/$tDb.$qDb.net.gz") {
    die "loadUp: looks like this was run successfully already " .
      "($tDb.$qDb.net[.gz] exists).  Either run with -continue download, " .
	"or move aside/remove $runDir/$tDb.$qDb.net[.gz] and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successDir = $isSelf ? "$runDir/$tDb.$qDb.all.chain.gz" :
                             "$buildDir/mafNet/";
  if (! -d $successDir && ! $opt_debug) {
    die "loadUp: looks like previous stage was not successful " .
      "(can't find $successDir).\n";
  }
  my $bossScript = "$buildDir/axtChain/loadUp.csh";
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
foreach c (`awk '{print \$1;}' $defVars{SEQ1_LEN}`)
    set f = \$c.chain
    if (! -e \$f) then
      echo no chains for \$c
      set f = /dev/null
    endif
    hgLoadChain $tDb \${c}_chain$QDb \$f
end
_EOF_
      ;
  } else {
    print SCRIPT <<_EOF_
cd $runDir
hgLoadChain -tIndex $tDb chain$QDb $tDb.$qDb.all.chain.gz
_EOF_
      ;
  }
  if (! $isSelf) {
    print SCRIPT <<_EOF_

# Add gap/repeat stats to the net file using database tables:
cd $runDir
netClass -verbose=0 -noAr noClass.net $tDb $qDb $tDb.$qDb.net

# Load nets:
netFilter -minGap=10 $tDb.$qDb.net \\
| hgLoadNet -verbose=0 $tDb net$QDb stdin
_EOF_
      ;
  }
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $dbHost nice $bossScript");
# maybe also peek in trackDb and see if entries need to be added for chain/net
}	#	sub loadUp {}


sub makeDownloads {
  # Compress the netClassed .net for download (other files should have been
  # compressed already).
  my ($machine) = @_;
  my $runDir = "$buildDir/axtChain";
  if (-e "$runDir/$tDb.$qDb.net") {
    &HgAutomate::run("ssh -x $machine nice " .
	 "gzip $runDir/$tDb.$qDb.net");
  }
  # Make an md5sum.txt file.
  my $bossScript = "$buildDir/axtChain/makeMd5sum.csh";
  my $net = $isSelf ? "" : "$tDb.$qDb.net.gz";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $machine in $runDir .
# It makes an md5sum.txt file for downloadable files, with relative paths
# matching what the user will see on the download server.
# This script will fail if any of its commands fail.

cd $runDir
md5sum $tDb.$qDb.all.chain.gz $net > md5sum.txt
_EOF_
;
  if (! $isSelf) {
    print SCRIPT <<_EOF_
cd ..
md5sum axtNet/*.gz >> axtChain/md5sum.txt
_EOF_
      ;
  }
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $machine nice $bossScript");
}


sub getAssemblyInfo {
  # Do a quick dbDb lookup to get assembly descriptive info for README.txt.
  my ($db) = @_;
  my $query = "select genome,description,sourceName from dbDb " .
              "where name = \"$db\";";
  my $line = `echo '$query' | $centralDbSql`;
  chomp $line;
  my ($genome, $date, $source) = split("\t", $line);
  return ($genome, $date, $source);
}


sub getBlastzParams {
  # Return parameters in BLASTZ_Q file, or defaults, for README.txt.
  my $matrix = 
"           A    C    G    T
      A   91 -114  -31 -123
      C -114  100 -125  -31
      G  -31 -125  100 -114
      T -123  -31 -114   91";
  if ($defVars{'BLASTZ_Q'}) {
    open(Q, $defVars{'BLASTZ_Q'})
      || die "Couldn't open BLASTZ_Q file $defVars{BLASTZ_Q}: $!\n";
    my $line = <Q>;
    if ($line !~ /^\s*A\s+C\s+G\s+T\s*$/) {
      die "Can't parse first line of $defVars{BLASTZ_Q}";
    }
    $matrix = '        ' . $line;
    foreach my $base ('A', 'C', 'G', 'T') {
      $line = <Q>;
      die "Too few lines of $defVars{BLASTZ_Q}" if (! $line);
      if ($line !~ /^\s*-?\d+\s+-?\d+\s+-?\d+\s+-?\d+\s*$/) {
	die "Can't parse this line of $defVars{BLASTZ_Q}:\n$line";
      }
      $matrix .= "      $base " . $line;
    }
    chomp $matrix;
    $line = <Q>;
    if ($line && $line =~ /\S/) {
      warn "\nWarning: BLASTZ_Q matrix file $defVars{BLASTZ_Q} has " .
           "additional contents after the matrix -- those are ignored " .
	   "by blastz.\n\n";
    }
    close(Q);
  }
  my $o = $defVars{'BLASTZ_O'} || 400;
  my $e = $defVars{'BLASTZ_E'} || 30;
  my $k = $defVars{'BLASTZ_K'} || 3000;
  my $l = $defVars{'BLASTZ_L'} || 2200;
  my $h = $defVars{'BLASTZ_H'} || 2000;
  my $blastzOther = '';
  foreach my $var (sort keys %defVars) {
    if ($var =~ /^BLASTZ_(\w)$/) {
      my $p = $1;
      if ($p ne 'K' && $p ne 'L' && $p ne 'H' && $p ne 'Q') {
	if ($blastzOther eq '') {
	  $blastzOther = 'Other blastz
parameters specifically set for this species pair:';
	}
	$blastzOther .= "\n    $p=$defVars{$var}";
      }
    }
  }
  return ($matrix, $o, $e, $k, $l, $h, $blastzOther);
}


sub commafy {
  # Assuming $num is a number, add commas where appropriate.
  my ($num) = @_;
  $num =~ s/(\d)(\d\d\d)$/$1,$2/;
  $num =~ s/(\d)(\d\d\d),/$1,$2,/g;
  return($num);
}

sub describeOverlapping {
  # Return some text describing how large sequences were split.
  my $lap;
  my $chunkPlusLap1 = $defVars{'SEQ1_CHUNK'} + $defVars{'SEQ1_LAP'};
  my $chunkPlusLap2 = $defVars{'SEQ2_CHUNK'} + $defVars{'SEQ2_LAP'};
  if ($chunkPlusLap1 == $chunkPlusLap2) {
    $lap .= "Any sequences larger\n" .
"than " . &commafy($chunkPlusLap1) . " bases were split into chunks of " .
&commafy($chunkPlusLap1) . " bases
overlapping by " . &commafy($defVars{SEQ1_LAP}) . " bases for alignment.";
  } else {
    $lap .= "Any $tDb sequences larger\n" .
"than " . &commafy($chunkPlusLap1) . " bases were split into chunks of " .
&commafy($chunkPlusLap1) . " bases overlapping
by " . &commafy($defVars{SEQ1_LAP}) . " bases for alignment.  " .
"A similar process was followed for $qDb,
with chunks of " . &commafy($chunkPlusLap2) . " overlapping by " .
&commafy($defVars{SEQ2_LAP}) . ".)";
  }
  $lap .= "  Following alignment, the
coordinates of the chunk alignments were corrected by the
blastz-normalizeLav script written by Scott Schwartz of Penn State.";
  return $lap;
}


sub dumpDownloadReadme {
  # Write a file (README.txt) describing the download files.
  my ($file) = @_;
  open (R, ">$file") || die "Couldn't open $file for writing: $!\n";
  my ($tGenome, $tDate, $tSource) = &getAssemblyInfo($tDb);
  my ($qGenome, $qDate, $qSource) = &getAssemblyInfo($qDb);
  my $dir = $splitRef ? 'axtNet/*.' : '';
  my ($matrix, $o, $e, $k, $l, $h, $blastzOther) = &getBlastzParams();
  my $defaultMatrix = $defVars{'BLASTZ_Q'} ? '' : ' the default matrix';
  my $lap = &describeOverlapping();
  my $abridging = "";
  if ($defVars{'BLASTZ_ABRIDGE_REPEATS'}) {
    if ($isSelf) {
      $abridging = "
All repetitive sequences identified by RepeatMasker were removed from the
assembly before alignment using the fasta-subseq and strip_rpts programs
from Penn State.  The abbreviated genome was aligned with blastz, and the
transposons were then added back in (i.e. the alignment coordinates were
adjusted) using the restore_rpts program from Penn State.";
    } else {
      $abridging = "
Transposons that have been inserted since the $qGenome/$tGenome split were
removed from the assemblies before alignment using the fasta-subseq and
strip_rpts programs from Penn State.  The abbreviated genomes were aligned
with blastz, and the transposons were then added back in (i.e. the
alignment coordinates were adjusted) using the restore_rpts program from
Penn State.";
    }
  }
  my $desc = $isSelf ? 
"This directory contains alignments of $tGenome ($tDb, $tDate,
$tSource) to itself." :
"This directory contains alignments of the following assemblies:

  - target/reference: $tGenome ($tDb, $tDate, $tSource)

  - query: $qGenome ($qDb, $qDate, $qSource)";

  print R "$desc

Files included in this directory:

  - md5sum.txt: md5sum checksums for the files in this directory

  - $tDb.$qDb.all.chain.gz: chained blastz alignments. The chain format is
    described in http://genome.ucsc.edu/goldenPath/help/chain.html .

";
  if (! $isSelf) {
    print R
"  - $tDb.$qDb.net.gz: \"net\" file that describes rearrangements between 
    the species and the best $qGenome match to any part of the
    $tGenome genome.  The net format is described in
    http://genome.ucsc.edu/goldenPath/help/net.html .

  - $dir$tDb.$qDb.net.axt.gz: chained and netted alignments,
    i.e. the best chains in the $tGenome genome, with gaps in the best
    chains filled in by next-best chains where possible.  The axt format is
    described in http://genome.ucsc.edu/goldenPath/help/axt.html .

";
  }
  if ($opt_swap) {
    my $TDb = ucfirst($tDb);
    print R
"The chainSwap program was used to translate $qDb-referenced chained blastz
alignments to $tDb into $tDb-referenced chains aligned to $qDb.  See
the download directory goldenPath/$qDb/vs$TDb/README.txt for more
information about the $qDb-referenced blastz and chaining process.
";
  } else {
    print R ($isSelf ?
"The $tDb assembly was aligned to itself" :
"The $tDb and $qDb assemblies were aligned");
  my $chainMinScore = $opt_chainMinScore ? "$opt_chainMinScore" :
	$defaultChainMinScore;
  my $chainLinearGap = $opt_chainLinearGap ? "$opt_chainLinearGap" :
	$defaultChainLinearGap;
    print R " by the blastz alignment
program, which is available from Webb Miller's lab at Penn State
University (http://www.bx.psu.edu/miller_lab/).  $lap $abridging

The blastz scoring matrix (Q parameter) used was$defaultMatrix:

$matrix

with a gap open penalty of O=$o and a gap extension penalty of E=$e.
The minimum score for an alignment to be kept was K=$k for the first pass
and L=$l for the second pass, which restricted the search space to the
regions between two alignments found in the first pass.  The minimum
score for alignments to be interpolated between was H=$h.  $blastzOther

The .lav format blastz output was translated to the .psl format with
lavToPsl, then chained by the axtChain program.\n
Chain minimum score: $chainMinScore, and linearGap matrix of ";
    if ($chainLinearGap =~ m/loose/) {
	print R "(loose):
tablesize   11
smallSize   111   
position  1   2   3   11  111 2111  12111 32111 72111 152111  252111
qGap    325 360 400  450  600 1100   3600  7600 15600  31600   56600
tGap    325 360 400  450  600 1100   3600  7600 15600  31600   56600
bothGap 625 660 700  750  900 1400   4000  8000 16000  32000   57000
";
    } elsif ($chainLinearGap =~ m/medium/) {
	print R "(medium):
tableSize   11
smallSize  111
position  1   2   3   11  111 2111  12111 32111  72111 152111  252111
qGap    350 425 450  600  900 2900  22900 57900 117900 217900  317900
tGap    350 425 450  600  900 2900  22900 57900 117900 217900  317900
bothGap 750 825 850 1000 1300 3300  23300 58300 118300 218300  318300
";
    } else {
	print R "(specified):\n", `cat $chainLinearGap`, "\n";
    }
  }
  if (! $isSelf) {
    print R "
Chained alignments were processed into nets by the chainNet, netSyntenic,
and netClass programs.
Best-chain alignments in axt format were extracted by the netToAxt program.";
  }
  print R "
All programs run after blastz were written by Jim Kent at UCSC.

----------------------------------------------------------------
If you plan to download a large file or multiple files from this directory,
we recommend you use ftp rather than downloading the files via our website.
To do so, ftp to hgdownload.cse.ucsc.edu, then go to the directory
goldenPath/$tDb/vs$QDb/. To download multiple files, use the \"mget\"
command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all files in the current directory)

All files in this directory are freely available for public use.

--------------------------------------------------------------------
References

Chiaromonte, F., Yap, V.B., and Miller, W.  Scoring pairwise genomic
sequence alignments.  Pac Symp Biocomput 2002, 115-26 (2002).

Kent, W.J., Baertsch, R., Hinrichs, A., Miller, W., and Haussler, D.
Evolution's cauldron: Duplication, deletion, and rearrangement in the
mouse and human genomes. Proc Natl Acad Sci USA 100(20), 11484-11489
(2003).

Schwartz, S., Kent, W.J., Smit, A., Zhang, Z., Baertsch, R., Hardison, R.,
Haussler, D., and Miller, W.  Human-mouse alignments with BLASTZ</A>.
Genome Res. 13(1), 103-7 (2003).

";
  close(R);
}


sub installDownloads {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  my $bossScript = "$buildDir/axtChain/installDownloads.csh";
  # Make sure previous stage was successful.
  my $successFile = $isSelf ? "$runDir/$tDb.$qDb.all.chain.gz" :
                              "$runDir/$tDb.$qDb.net.gz";
  if (! -e $successFile && ! $opt_debug) {
    die "installDownloads: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  &dumpDownloadReadme("$runDir/README.txt");
  my $over = $tDb . "To$QDb.over.chain.gz";
  my $liftOverDir = "$clusterData/$tDb/$trackBuild/liftOver";
  my $gpLiftOverDir = "$goldenPath/$tDb/liftOver";
  my $gbdbLiftOverDir = "$gbdb/$tDb/liftOver";
  my $andNets = $isSelf ? "." :
    ", nets and axtNet,\n" .
    "# and copies the liftOver chains to the liftOver download dir.";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $dbHost in $runDir .
# It creates the download directory for chains$andNets
# This script will fail if any of its commands fail.

mkdir -p $goldenPath/$tDb
mkdir $goldenPath/$tDb/vs$QDb
cd $goldenPath/$tDb/vs$QDb
ln -s $runDir/$tDb.$qDb.all.chain.gz .
cp -p $runDir/README.txt .
ln -s $runDir/md5sum.txt .

_EOF_
    ;
  if (! $isSelf) {
    my $axt = ($splitRef ?
	       "mkdir axtNet\n" . "ln -s $buildDir/axtNet/*.axt.gz axtNet/" :
	       "ln -s $buildDir/axtNet/$tDb.$qDb.net.axt.gz .");
    print SCRIPT <<_EOF_
ln -s $runDir/$tDb.$qDb.net.gz .
$axt

mkdir -p $gpLiftOverDir
rm -f $gpLiftOverDir/$over
ln -s $liftOverDir/$over $gpLiftOverDir/$over
mkdir -p $gbdbLiftOverDir
rm -f $gbdbLiftOverDir/$over
ln -s $liftOverDir/$over $gbdbLiftOverDir/$over
hgAddLiftOverChain -minMatch=0.1 -multiple -path=$gbdbLiftOverDir/$over \\
  $tDb $qDb

_EOF_
      ;
  }
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $dbHost nice $bossScript");
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
  }
  close(SCRIPT);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::run("ssh -x $machine nice $bossScript");
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

# Undocumented option for quickly generating a README from DEF:
if ($opt_readmeOnly) {
  $splitRef = $opt_swap ? (`wc -l < $defVars{SEQ2_LEN}` < $splitThreshold) :
    (`wc -l < $defVars{SEQ1_LEN}` < $splitThreshold);
  &swapGlobals() if $opt_swap;
  &dumpDownloadReadme("/tmp/README.txt");
  exit 0;
}

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
  &HgAutomate::mustMkdir("$swapDir/axtChain");
  $splitRef = (`wc -l < $defVars{SEQ2_LEN}` < $splitThreshold);
  &HgAutomate::verbose(1, "Swapping from $buildDir/axtChain/$inChain\n" .
	      "to $swapDir/axtChain/$qDb.$tDb.all.chain.gz .\n");
} else {
  if (! -d $buildDir) {
    &HgAutomate::mustMkdir($buildDir);
  }
  if (! $opt_blastzOutRoot && $startStep < $stepVal{'chainRun'}) {
    &enforceClusterNoNo($buildDir,
	    'blastz/chain/net build directory (or use -blastzOutRoot)');
  }
  $splitRef = (`wc -l < $defVars{SEQ1_LEN}` < $splitThreshold);
  &HgAutomate::verbose(1, "Building in $buildDir\n");
}

if (! -e "$buildDir/DEF") {
  &HgAutomate::run("cp $DEF $buildDir/DEF");
}

my $fileServer = $opt_fileServer ? $opt_fileServer :
                 $opt_swap ? `$getFileServer $swapDir/` :
                             `$getFileServer $buildDir/`;
chomp $fileServer;
if (scalar(grep /^$fileServer$/, @fileServerNoLogin)) {
  print STDERR "Logins are not allowed on fileServer $fileServer; will use " .
    "workhorse $workhorse instead of fileServer for I/O-intensive steps.\n";
  $fileServer = $workhorse;
}

my $step = 0;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step initial partition to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &doPartition($bigClusterHub, $fileServer);
}

#	-continue blastz
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step initial blastz to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &doBlastzClusterRun($bigClusterHub);
}

#	-continue cat
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step cat to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &doCatRun($smallClusterHub);
}

#	-continue chainRun
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step chainRun to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &doChainRun($smallClusterHub);
}

#	-continue chainMerge
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step chainMerge to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  if ($opt_swap) {
    &swapChains($workhorse, $fileServer);
  } else {
    &postProcessChains($workhorse, $fileServer);
  }
}

&swapGlobals() if ($opt_swap);

#	-continue net
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step net to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &netChains($workhorse) if (! $isSelf);
}

#	-continue load
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step load to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &loadUp();
}

#	-continue download
$step++;
HgAutomate::verbose(2,
	"startStep: $startStep, at step $step download to stopStep $stopStep\n");
if ($startStep <= $step && $step <= $stopStep) {
  &makeDownloads($workhorse);
  &installDownloads();
}

#	-continue cleanup
$step++;
if ($startStep <= $step && $step <= $stopStep) {
  &cleanup($fileServer);
}

HgAutomate::verbose(1,
	"\n *** All done!\n");
HgAutomate::verbose(1,
	" *** Make sure that goldenPath/$tDb/vs$QDb/README.txt is accurate.\n")
  if ($stopStep >= $stepVal{'download'});
HgAutomate::verbose(1,
	" *** Add {chain,net}$QDb tracks to trackDb.ra if necessary.\n")
  if ($stopStep >= $stepVal{'load'});
HgAutomate::verbose(1,
	"\n\n");

