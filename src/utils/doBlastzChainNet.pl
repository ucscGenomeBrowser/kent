#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/doBlastzChainNet.pl instead.

# $Id: doBlastzChainNet.pl,v 1.1 2005/02/17 01:19:30 angie Exp $

use Getopt::Long;
use warnings;
use strict;

# Hardcoded paths/command sequences:
my $blastzRunUcsc = '/cluster/bin/scripts/blastz-run-ucsc';
my $partition = '/cluster/bin/scripts/partitionSequence.pl';
my $gensub2 = '/parasol/bin/gensub2';
my $para = '/parasol/bin/para';
my $bigClusterHub = 'kk';
my $smallClusterHub = 'kki';
my $paraRun = ("$para make jobList\n" .
	       "$para check\n" .
	       "$para time > run.time\n");
my $dbHost = 'hgwdev';
my $clusterData = '/cluster/data';
my $trackBuild = 'bed';

# Option variable names:
use vars qw/
    $opt_continue
    $opt_debug
    $opt_verbose
    $opt_help
    /;

# Numeric values of -continue options for determining precedence:
my %continueVal = ( 'cat' => 1,
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
  my $continueVals = join(", ",
			  sort { $continueVal{$a} <=> $continueVal{$b} }
			  keys %continueVal);
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base DEF
options:
    -continue step        Pick up at the step where a previous run left off
                          (i.e. ran into trouble which has been fixed).
                          step must be one of the following:
                          $continueVals
    -debug                Don't actually run commands, just display them.
    -verbose num          Set verbose level to num (default $defaultVerbose).
    -help                 Show detailed help and exit.
Automates UCSC's blastz/chain/net pipeline:
    1. Big cluster run of blastz.
    2. Small cluster consolidation of files.
    3. Small cluster chaining run.
    4. Sorting and netting of chains on the fileserver.
    5. Generation of liftOver-suitable chains from nets+chains on fileserver.
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
   \$qDb is the query.  
   hgwdev:/usr/local/apache/htdocs/goldenPath/\$tDb/vs\$QDb/ is the 
   directory where downloadable files need to go.
   -- need to add liftover files too --
2. DEF's SEQ1* variables describe the target/reference assembly.
   DEF's SEQ2* variables describe the query assembly.
   If those are the same assembly, then we're doing self-alignments and 
   will drop aligned blocks that cross the diagonal.
3. DEF's SEQ1_DIR is either a directory containing one nib file per 
   target sequence (usually chromosome), OR a complete path to a 
   single .2bit file containing all target sequences.  This directory 
   should be in /scratch/hg/ or /iscratch/i
   SEQ2_DIR: ditto for query.
4. DEF's SEQ1_LEN is a tab-separated dump of the target database table 
   chromInfo -- or at least a file that contains all sequence names 
   in the first column, and corresponding sizes in the second column.
   Normally this will be $clusterData/\$tDb/chrom.sizes, but for a 
   scaffold-based assembly, it is a good idea to put it in /iscratch 
   or /cluster/bluearc, /panasas etc. because it will be a large file 
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
      directory should be under /scratch/hg/ or /iscratch/i/.  
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
10. All other variables in DEF will be ignored!

" if ($detailed);
  exit $status;
}


# Globals:
my %defVars = ();
my ($DEF, $tDb, $qDb, $QDb, $isSelf, $buildDir, $startStep);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions("continue=s",
		      "verbose=n",
		      "debug",
		      "help");
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  $opt_verbose = $defaultVerbose if (! defined $opt_verbose);
  if ($opt_continue) {
    if (! defined $continueVal{$opt_continue}) {
      warn "\nUnsupported -continue value \"$opt_continue\".\n";
      &usage(1);
    }
    $startStep = $continueVal{$opt_continue};
  } else {
    $startStep = 0;
  }
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
  # Run a command in sh, with PATH specified in %defVars.
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

my $oldDbFormat = '[a-z][a-z]\d+';
my $newDbFormat = '[a-z][a-z][a-z][A-Z][a-z][a-z]\d+';
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
  } elsif (-e "$runDir/gsub" || -e "$runDir/jobList") {
    die "doBlastzClusterRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  my $targetList = "$tDb.lst";
  my $queryList = $isSelf ? $targetList : "$qDb.lst";
  my $tPartDir = ($defVars{'SEQ1_DIR'} =~ /\.2bit$/) ? "-lstDir tParts" : '';
  my $qPartDir = ($defVars{'SEQ2_DIR'} =~ /\.2bit$/) ? "-lstDir qParts" : '';
  my $partitionTargetCmd = 
    ("$partition $defVars{SEQ1_CHUNK} $defVars{SEQ1_LAP} " .

     "$defVars{SEQ1_DIR} $defVars{SEQ1_LEN} -xdir xdir.sh -rawDir ../psl " .
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
		     '{check out exists ../psl/$(file1)/$(file1)_$(file2).psl }');
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
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It partitions target and query sequences into chunks of a sensible size 
# for cluster runs;  creates a jobList for parasol;  performs the cluster 
# run;  and checks results.  
# This script will fail if any of its steps fail.

cd $runDir
$partitionTargetCmd
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
  } elsif (-e "$runDir/gsub" || -e "$runDir/jobList") {
    die "doCatRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/run.blastz/run.time";
  if (! -e $successFile) {
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

  my $bossScript = "$runDir/doCatRun.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It sets up and performs a small cluster run to concatenate all files in 
# each subdirectory of ../psl into a per-target-chunk file.
# This script will fail if any of its steps fail.

cd $runDir
ls -1d ../psl/* | sed -e 's@/\$@\@' > tParts.lst
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
  } elsif (-e "$runDir/gsub" || -e "$runDir/jobList") {
    die "doChainRun: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/ and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/run.cat/run.time";
  if (! -e $successFile) {
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
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $paraHub in $runDir .
# It sets up and performs a small cluster run to chain all alignments 
# to each target sequence.
# This script will fail if any of its steps fail.

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
  # chainMergeSort etc. on the fileServer.
  my ($fileServer) = @_;
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-e "$runDir/all.chain") {
    die "postProcessChains: looks like this was run successfully already " .
      "(all.chain exists).  Either run with -continue net or some later " .
	"stage, or move aside/remove $runDir/chain and run again.\n";
  } elsif (-e "$runDir/chain") {
    die "postProcessChains: looks like we are not starting with a clean " .
      "slate.  Please move aside or remove $runDir/chain and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/axtChain/run/run.time";
  if (! -e $successFile) {
    die "postProcessChains: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  my $bossScript = "$buildDir/axtChain/postProcessChains.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $fileServer in $runDir .
# It postprocesses the results of the chaining cluster run into a 
# merged & sorted genome-wide set of chains with stable IDs.
# This script will fail if any of its steps fail.

cd $runDir
chainMergeSort run/chain/*.chain > all.chain
chainSplit chain all.chain
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $fileServer $bossScript");
}


sub netChains {
  # Turn chains into nets (,axt,maf,.over.chain) on the fileServer.
  my ($fileServer) = @_;
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
  my $successFile = "$buildDir/axtChain/all.chain";
  if (! -e $successFile) {
    die "netChains: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  my $over = $tDb . "To" . $QDb . ".over.chain";
  my $bossScript = "$buildDir/axtChain/netChains.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $fileServer in $runDir .
# It generates nets (without repeat/gap stats -- those are added later on 
# $dbHost) from chains, and generates axt, maf and .over.chain from the nets.
# This script will fail if any of its steps fail.

cd $runDir

# Make nets ("noClass", i.e. without rmsk/class stats which are added later):
chainPreNet all.chain $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} stdout \\
| chainNet stdin -minSpace=1 $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} stdout /dev/null \\
| netSyntenic stdin noClass.net

# Make liftOver chains:
netChainSubset noClass.net all.chain stdout \\
| chainSort stdin $over

# Make axtNet for download
netSplit noClass.net net
cd ..
mkdir axtNet
foreach f (axtChain/net/chr*.net)
netToAxt \$f axtChain/chain/\$f:t:r.chain \\
  $defVars{SEQ1_DIR} $defVars{SEQ2_DIR} stdout \\
  | axtSort stdin axtNet/\$f:t:r.axt
end

# Make mafNet for multiz
mkdir mafNet
foreach f (axtNet/chr*.axt)
  set maf = mafNet/\$f:t:r.maf
  axtToMaf \$f \\
        $defVars{SEQ1_LEN} $defVars{SEQ2_LEN} \\
        \$maf -tPrefix=$tDb. -qPrefix=$qDb.
end
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $fileServer $bossScript");
}


sub loadUp {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-d "$runDir/$qDb.net") {
    die "netChains: looks like this was run successfully already " .
      "($qDb.net exists).  Either run with -continue download, " .
	"or move aside/remove $runDir/$qDb.net and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successDir = "$buildDir/mafNet/";
  if (! -d $successDir) {
    die "netChains: looks like previous stage was not successful " .
      "(can't find $successDir).\n";
  }
  my $bossScript = "$buildDir/axtChain/loadUp.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $dbHost in $runDir .
# It loads the chain tables into $tDb, adds gap/repeat stats to the .net file,
# and loads the net table.
# This script will fail if any of its steps fail.

# Load chains:
cd $runDir/chain
foreach f (*.chain)
    set c = \$f:r
    hgLoadChain $tDb \${c}_chain$QDb \$f
end

# Add gap/repeat stats to the net file using database tables:
cd $runDir
netClass -noAr noClass.net $tDb $qDb $qDb.net

# Load nets:
netFilter -minGap=10 $qDb.net \\
| hgLoadNet $tDb net$QDb stdin
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $dbHost $bossScript");
# maybe also peek in trackDb and see if entries need to be added for chain/net
}


sub makeDownloads {
  # Make compressed chain, net, axtNet files for download.
  my ($fileServer) = @_;
  my $runDir = "$buildDir/axtChain";
  # First, make sure we're starting clean.
  if (-d "$clusterData/$tDb/zips/axtNet") {
    die "netChains: looks like this was run successfully already " .
      "(zips/axtNet exists).  Either run with -continue cleanup, " .
	"or move aside/remove $clusterData/$tDb/zips/axtNet/ " . 
	  "and zips/$qDb.*.gz and run again.\n";
  } elsif (-e "$clusterData/$tDb/zips/$qDb.chain.gz" ||
	   -e "$clusterData/$tDb/zips/$qDb.net.gz") {
    die "netChains: looks like we are not starting with a " .
      "clean slate.  Please move aside or remove " .
	"zips/$qDb.*.gz and run again.\n";
  }
  # Make sure previous stage was successful.
  my $successFile = "$buildDir/axtChain/$qDb.net";
  if (! -e $successFile) {
    die "netChains: looks like previous stage was not successful " .
      "(can't find $successFile).\n";
  }
  my $bossScript = "$buildDir/axtChain/makeDownloads.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $fileServer in $runDir .
# It compresses chain, net and axtNet files for download.
# This script will fail if any of its steps fail.

cd $runDir

mkdir -p $clusterData/$tDb/zips
gzip -c all.chain \\
  > $clusterData/$tDb/zips/$qDb.chain.gz
gzip -c $qDb.net \\
  > $clusterData/$tDb/zips/$qDb.net.gz
mkdir $clusterData/$tDb/zips/axtNet
foreach f (../axtNet/chr*axt)
  gzip -c \$f > $clusterData/$tDb/zips/axtNet/\$f:t.gz
end
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $fileServer $bossScript");
}


sub installDownloads {
  # Load chains; add repeat/gap stats to net; load nets.
  my $runDir = "$buildDir/axtChain";
  my $bossScript = "$buildDir/axtChain/installDownloads.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $dbHost in $runDir .
# It creates the download directory for chains, nets and axtNet.
# This script will fail if any of its steps fail.

mkdir /usr/local/apache/htdocs/goldenPath/$tDb/vs$QDb
cd /usr/local/apache/htdocs/goldenPath/$tDb/vs$QDb
mv $clusterData/$tDb/zips/$qDb.*gz .
mv $clusterData/$tDb/zips/axtNet .
md5sum *.gz */*.gz > md5sum.txt
# Make a README.txt which explains the files & formats.
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $dbHost $bossScript");
# copy liftover chain to the right place on hgwdev too...
# maybe also peek in trackDb and see if entries need to be added for chain/net
}


sub cleanup {
  # Make compressed chain, net, axtNet files for download.
  my ($fileServer) = @_;
  my $runDir = $buildDir;
  my $bossScript = "$buildDir/cleanUp.csh";
  open(SCRIPT, ">$bossScript")
    || die "Couldn't open $bossScript for writing: $!\n";
  print SCRIPT <<_EOF_
#!/bin/csh -ef
# This script was automatically generated by $0 
# from $DEF.
# It is to be executed on $fileServer in $runDir .
# It cleans up files after a successful blastz/chain/net/install series.
# This script will fail if any of its steps fail, but uses rm -f so 
# individual failures should be ignored (e.g. if a partial cleanup has 
# already been performed).

cd $runDir

rm -fr psl/
rm -fr axtChain/run/chain/
rm -f axtChain/noClass.net
rm -fr axtChain/net/
rm -fr axtChain/chain/
_EOF_
  ;
  close(SCRIPT);
  &run("chmod a+x $bossScript");
  &run("ssh -x $fileServer $bossScript");
}


#########################################################################
#
# -- main --

&checkOptions();

&usage(1) if (scalar(@ARGV) != 1);
($DEF) = @ARGV;

&loadDef($DEF);
&checkDef();

my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = "$clusterData/$tDb/$trackBuild/blastz.$qDb.$date";
&verbose(1, "Building in $buildDir\n");

if (! -d $buildDir) {
  &mustMkdir($buildDir);
  &run("cp $DEF $buildDir/DEF");
}

my $fileServer = `fileServer $buildDir/`;
chomp $fileServer;

my $step = 0;
if ($startStep <= $step) {
  &doBlastzClusterRun($bigClusterHub);
}

$step++;
if ($startStep <= $step) {
  &doCatRun($smallClusterHub);
}

$step++;
if ($startStep <= $step) {
  &doChainRun($smallClusterHub);
}

$step++;
if ($startStep <= $step) {
  &postProcessChains($fileServer);
}

$step++;
if ($startStep <= $step) {
  &netChains($fileServer);
}

$step++;
if ($startStep <= $step) {
  &loadUp();
}

$step++;
if ($startStep <= $step) {
  &makeDownloads($fileServer);
  &installDownloads();
}

$step++;
if ($startStep <= $step) {
  &cleanup($fileServer);
}

