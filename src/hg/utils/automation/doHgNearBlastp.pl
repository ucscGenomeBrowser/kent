#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/automation/doHgNearBlastp.pl instead.

# $Id: doHgNearBlastp.pl,v 1.11 2009/10/02 05:14:15 kent Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;

# Hardcoded params:
my $tSplitCount = 1000;
my $selfE = 0.01;
my $selfMaxPer = 1000;
my $pairwiseMaxPer = 1;

# Option variable names:
use vars qw/
    $opt_clusterHub
    $opt_distrHost
    $opt_dbHost
    $opt_workhorse
    $opt_blastPath
    $opt_noSelf
    $opt_targetOnly
    $opt_queryOnly
    $opt_noLoad
    $opt_debug
    $opt_verbose
    $opt_help
    /;

# Option defaults:
# my $clusterHub = 'swarm';
my $clusterHub = 'ku';
# my $distrHost = 'swarm';
my $distrHost = 'ku';
my $dbHost = 'hgwdev';
my $workhorse = 'least loaded';
my $blastPath = '/cluster/bin/blast/x86_64/blast-2.2.20';
my $defaultVerbose = 1;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base config.ra
options:
    -clusterHub mach	Parasol hub for blastp cluster runs.
			Default: $clusterHub
    -distrHost mach	Host which distributes files to the cluster-attached
			fast storage for the blastp cluster runs.
			Default: $distrHost
    -dbHost mach	Mysql server into which we load *BlastTab tables.
			Default: $dbHost
    -workhorse mach     Use machine (default: $workhorse) for compute or
                        memory-intensive steps.
    -blastPath dir	Directory in which the latest blat has been installed
			(should also be compiled for the same architecture as
			cluster hosts).
			Default: $blastPath
    -noSelf		Don't do self alignments (update pairwise only).
    -targetOnly		Perform target vs. all queries, not vice versa.
    -queryOnly		Perform all queries vs. target, not vice versa.
    -noLoad		Perform alignments but don't load database tables.
    -debug		Don't actually run commands, just display them.
    -verbose num	Set verbose level to num.  Default $defaultVerbose
    -help		Show detailed help (config.ra variables) and exit.
Automates the protein blast runs for hgNear and loads the *BlastTab tables.
config.ra specifies one target db and multiple query dbs.  Self-blastp for
the target and pairwise blastp for the target vs. all queries, and all
queries vs. the target, are performed by default but can be selectively
disabled with -noSelf, -targetOnly or -queryOnly.
To see detailed information about what should appear in config.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  print STDERR "
config.ra must define the following settings:

targetGenesetPrefix xxx
  - xxx is the prefix used to name the self *BlastTab table.

targetDb xxx
  - xxx is the target database (the one that we just got new genes for)

queryDbs xxx yyy zzz ...
  - space-separated list of query databases (the other hgNear dbs)

xxxFa fff
yyyFa ggg
...
  - for each db in queryDbs, define a *Fa variable with the complete path
    of a single fasta file containing protein sequence for genes.

buildDir xxx
  - Working directory (usually /cluster/data/targetDb/bed/hgNearBlastp)
    in which scriptlets and results will be stored.

scratchDir xxx
  - Cluster-attached fast storage (usually under /san/sanvol1/scratch/...)
    where fasta will be split/formatted and used during
    cluster blastp runs.


Optional settings:

recipBest xxx yyy zzz ...
  - space-separated list of query databases for which we are to find
    reciprocal-best blast hits.  Even if -targetOnly or -queryOnly is
    specified, we will still do alignments in both directions if recipBest
    is specified.

targetAbbrev yz
  - yz is a two-letter abbreviation for the target organism, e.g. mm for
    mouse.  It is used to name the yzBlastTab tables in each queryDb.
    Normally the abbreviation is distilled from targetDb, but if targetDb
    is a temporary database with a name that bears no resemblance to the
    real genome database name, then this overrides.
" if ($detailed);
  print STDERR "\n";
  exit $status;
} # usage

# Globals:
my $CONFIG;

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions("clusterHub=s",
		      "distrHost=s",
		      "dbHost=s",
		      "workhorse=s",
		      "blastPath=s",
		      "verbose=n",
		      "noSelf",
		      "targetOnly",
		      "queryOnly",
		      "noLoad",
		      "debug",
		      "help");
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  $opt_verbose = $defaultVerbose if (! defined $opt_verbose);
  $clusterHub = $opt_clusterHub if (defined $opt_clusterHub);
  $distrHost = $opt_distrHost if (defined $opt_distrHost);
  $dbHost = $opt_dbHost if (defined $opt_dbHost);
  $blastPath = $opt_blastPath if (defined $opt_blastPath);
  if ($opt_targetOnly && $opt_queryOnly) {
    warn "\nBoth -targetOnly and -queryOnly were specified -- if you want " .
      "both target and query, then omit both of those options.\n";
    usage(1);
  }
} # checkOptions

my ($buildDir, $scratchDir, $tGenesetPrefix, $tDb, @qDbs, %dbToFasta,
    %recipBest, $targetAbbrev);
sub parseConfig {
  # Parse config.ra file, make sure it contains the required variables.
  my ($config) = @_;
  my $fh = &HgAutomate::mustOpen($config);
  my %vars;
  while (<$fh>) {
    next if (/^\s*#/ || /^\s*$/);
    if (/^\s*(\w+)\s*(.*)$/) {
      $vars{$1} = $2;
      $vars{$1} = " " if (! $2 && $1 eq 'queryDbs');
    } else {
      die "Parse error line $. of $config:\n$_\n--";
    }
  }
  close($fh);
  # Required variables.
  $buildDir = $vars{'buildDir'}
    || die "Error: $config is missing required variable buildDir.\n";
  delete $vars{'buildDir'};
  $scratchDir = $vars{'scratchDir'}
    || die "Error: $config is missing required variable scratchDir.\n";
  delete $vars{'scratchDir'};
  $tDb = $vars{'targetDb'}
    || die "Error: $config is missing required variable targetDb.\n";
  delete $vars{'targetDb'};
  $tGenesetPrefix = $vars{'targetGenesetPrefix'}
    || die "Error: $config is missing required variable targetGenesetPrefix.\n";
  delete $vars{'targetGenesetPrefix'};
  my $queryDbs = $vars{'queryDbs'}
    || die "Error: $config is missing required variable queryDbs.\n";
  delete $vars{'queryDbs'};
  @qDbs = split(/\s+/, $queryDbs);
  # Variables we expect to be there given the contents of $queryDbs:
  foreach my $db ($tDb, @qDbs) {
    my $faVar = $db . "Fa";
    my $fa = $vars{$faVar}
      || die "Error: $config is missing required variable $faVar.\n";
    $dbToFasta{$db} = $fa;
    delete $vars{$faVar};
  }
  # Optional variables.
  my $recipBests = $vars{'recipBest'};
  if ($recipBests) {
    foreach my $r (split(/\s+/, $recipBests)) {
      die "Error: $config recipBest item \"$r\" is not in queryDbs.\n"
	if (scalar(grep(/^$r$/, @qDbs)) == 0);
      $recipBest{$r} = 1;
    }
    delete $vars{'recipBest'};
  }
  $targetAbbrev = $vars{'targetAbbrev'};
  if ($targetAbbrev) {
    die "Error: $config targetAbbrev must consist of two lower-case letters " .
      "(Genus species --> gs)"
      if ($targetAbbrev !~ /^[a-z]{2}$/);
    delete $vars{'targetAbbrev'};
  }
  # Bogus/misspelled variables.
  if (scalar(keys %vars) > 0) {
    die "Error: $config contains extra variable(s) that I don't know " .
      "what to do with: " . join(", ", sort keys %vars) . "\n";
  }

  die "Sorry, this script can't rsync to $scratchDir -- please use san or " .
    "bluearc for scratchDir in $config.\n"
      if ($scratchDir =~ /^\/i?scratch\//);
} # parseConfig

sub splitSequence {
  # Split the target gene fasta file into small pieces for cluster run.
  my ($tDb, $tFasta) = @_;

  my $runDir = $buildDir;
  my $bossScript = "$runDir/doSplit.csh";
  my $fh = &HgAutomate::mustOpen(">$bossScript");
  print $fh <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0
# from $CONFIG.
# It is to be executed on $distrHost.
# It runs faSplit on a monolithic protein fasta file.
# This script will fail if any of its commands fail.

mkdir $scratchDir/$tDb.split
faSplit sequence $tFasta $tSplitCount $scratchDir/$tDb.split/${tDb}_
_EOF_
    ;
  close($fh);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::nfsNoodge("$bossScript");
  &HgAutomate::run("$HgAutomate::runSSH $distrHost nice $bossScript");
}

sub formatSequence {
  # Run formatdb on the specified query ($scratchDir should be cluster-mounted)
  my ($qDb, $qFasta) = @_;

  my $runDir = $buildDir;
  &HgAutomate::mustMkdir($runDir);
  my $bossScript = "$runDir/doFormat_$qDb.csh";
  my $fh = &HgAutomate::mustOpen(">$bossScript");
  print $fh <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0
# from $CONFIG.
# It is to be executed on $distrHost.
# It runs formatdb to preprocess query protein fasta.
# This script will fail if any of its commands fail.

mkdir $scratchDir/$qDb.formatdb
cd $scratchDir/$qDb.formatdb
$blastPath/bin/formatdb -i $qFasta -t $qDb -n $qDb
_EOF_
    ;
  close($fh);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::nfsNoodge("$bossScript");
  &HgAutomate::run("$HgAutomate::runSSH $distrHost nice $bossScript");
} # formatSequence

# Make a matrix of clade-pair -e (max E-value) threshold:
my ($mammal, $fish, $fly, $worm, $yeast) = (0..5);
my %dbToClade = ( hg => $mammal, mm => $mammal, rn => $mammal, tmpFoo => $mammal,
		  danRer => $fish,
		  dm => $fly,
		  ce => $worm,
		  sacCer => $yeast, );
my @cladeEs;
# Different species within clade (not self alignments -- see $selfE):
$cladeEs[$mammal][$mammal] = $cladeEs[$fish][$fish] = $cladeEs[$fly][$fly] =
    $cladeEs[$worm][$worm] = $cladeEs[$yeast][$yeast] = 0.001;
# Cross-clade:
$cladeEs[$mammal][$fish]  = $cladeEs[$fish][$mammal]  = 0.005;
$cladeEs[$mammal][$fly]   = $cladeEs[$fly][$mammal]   = 0.01;
$cladeEs[$mammal][$worm]  = $cladeEs[$worm][$mammal]  = 0.01;
$cladeEs[$mammal][$yeast] = $cladeEs[$yeast][$mammal] = 0.01;
$cladeEs[$fish][$fly]   = $cladeEs[$fly][$fish]   = 0.01;
$cladeEs[$fish][$worm]  = $cladeEs[$worm][$fish]  = 0.01;
$cladeEs[$fish][$yeast] = $cladeEs[$yeast][$fish] = 0.01;
$cladeEs[$fly][$worm]  = $cladeEs[$worm][$fly]  = 0.01;
$cladeEs[$fly][$yeast] = $cladeEs[$yeast][$fly] = 0.01;
$cladeEs[$worm][$yeast] = $cladeEs[$yeast][$worm] = 0.01;

sub calcE {
  # Look up the blastp e parameter (max E-value) by clade distances
  my ($tDb, $qDb) = @_;
  (my $t = $tDb) =~ s/\d+$//;
  defined ($t = $dbToClade{$t}) || die "Can't figure out clade of '$tDb'";
  (my $q = $qDb) =~ s/\d+$//;
  defined ($q = $dbToClade{$q}) || die "Can't figure out clade of '$qDb'";
  my $e = $cladeEs[$t][$q] || die "No e for clades '$t', '$q'";
  return $e;
} # calcE

sub runPairwiseBlastp {
  # Do a pairwise blastp cluster run for the given query.  
  # $b is the blastp -b param value... note -b is at the end of $blastpParams.
  my ($tDb, $qDb, $e, $b) = @_;

  my $runDir = "$buildDir/run.$tDb.$qDb";
  &HgAutomate::mustMkdir($runDir);
  my $fh = &HgAutomate::mustOpen(">$runDir/blastSome");
  print $fh <<_EOF_
#!/bin/csh -ef
setenv BLASTMAT $blastPath/data
$blastPath/bin/blastall -p blastp -d $scratchDir/$qDb.formatdb/$qDb \\
    -i \$1 -o \$2 -e $e -m 8 -b $b
_EOF_
    ;
  close($fh);

  &HgAutomate::makeGsub($runDir,
    "blastSome {check in line+ \$(path1)} {check out line out/\$(root1).tab}");

  my $bossScript = "$runDir/doBlastp.csh";
  $fh = &HgAutomate::mustOpen(">$bossScript");
  print $fh <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0
# from $CONFIG.
# It is to be executed on $clusterHub in $runDir .
# It does the blastp cluster run.
# This script will fail if any of its commands fail.

cd $runDir
mkdir out
chmod a+x blastSome
ls -1S $scratchDir/$tDb.split/*.fa > split.lst
gensub2 split.lst single gsub spec
para make spec
para time

_EOF_
    ;
  close($fh);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::nfsNoodge("$bossScript");
  &HgAutomate::run("$HgAutomate::runSSH $clusterHub $bossScript");
} # runPairwiseBlastp


sub dbToPrefix {
  # Condense database name to a 2-character prefix for the *BlastTab table.
  my ($db) = @_;
  my $prefix;
  return $targetAbbrev if ($targetAbbrev && $db eq $tDb);
  if ($db =~ /^(\w\w)\d+$/) {
    $prefix = $1;
  } elsif ($db =~ /^(\w)\w\w(\w)\w\w\d+$/) {
    $prefix = $1 . lc($2);
  } else {
    die "Error: dbToPrefix: expecting database name, got $db";
  }
  return $prefix;
}

sub loadPairwise {
  my ($tDb, $qDb, $tablePrefix, $max, $filePattern) = @_;

  $filePattern = 'out/*.tab' if (! defined $filePattern);
  my $tableName = $tablePrefix . 'BlastTab';
  my $bestOnly = ($max > 1) ? '-bestOnly' : '';
  my $runDir = "$buildDir/run.$tDb.$qDb";
  my $bossScript = "$buildDir/run.$tDb.$qDb/loadPairwise.csh";
  my $fh = HgAutomate::mustOpen(">$bossScript");
  print $fh <<_EOF_
#!/bin/csh -efx
# This script was automatically generated by $0
# from $CONFIG.
# It is to be executed on $dbHost in $runDir .
# It does the blastp cluster run.
# This script will fail if any of its commands fail.

cd $runDir
hgLoadBlastTab $tDb $tableName -maxPer=$max $bestOnly $filePattern
_EOF_
    ;
  close($fh);
  &HgAutomate::run("chmod a+x $bossScript");
  &HgAutomate::nfsNoodge("$bossScript");

  return if ($opt_noLoad);

  &HgAutomate::run("$HgAutomate::runSSH $dbHost nice $bossScript");
} # loadPairwise


sub recipBest {
  my ($tDb, $qDb) = @_;
  my $runDir = "$buildDir/run.$tDb.$qDb";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It runs blastRecipBest on the $tDb-$qDb and $qDb-$tDb " .
                   "results.";
  my $bossScript = new HgRemoteScript("$runDir/doRecipBest.csh", $workhorse,
				      $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
cat $buildDir/run.$tDb.$qDb/out/*.tab \\
  > $tDb.$qDb.all.tab
cat $buildDir/run.$qDb.$tDb/out/*.tab \\
  > $qDb.$tDb.all.tab
blastRecipBest $tDb.$qDb.all.tab $qDb.$tDb.all.tab \\
  $tDb.$qDb.recipBest.tab $qDb.$tDb.recipBest.tab
mv $qDb.$tDb.recipBest.tab $buildDir/run.$qDb.$tDb/
_EOF_
    );
  $bossScript->execute();
} # recipBest


sub cleanup {
  # Remove what we added in $scratchDir.
  foreach my $db (@_) {
    &HgAutomate::run("$HgAutomate::runSSH $distrHost rm -rf $scratchDir/$db.split");
    &HgAutomate::run("$HgAutomate::runSSH $distrHost rm -rf $scratchDir/$db.formatdb");
  }
  &HgAutomate::run("$HgAutomate::runSSH $distrHost rmdir $scratchDir");
} # cleanup

sub celebrate {
  # Hooray, we're done.
  HgAutomate::verbose(1,
	"\n *** All done!\n");
  if ($opt_noLoad) {
    if (! $opt_noSelf) {
      HgAutomate::verbose(1,
	   " *** -noLoad was specified -- you can run this script " .
	   "manually to load $tDb tables:\n");
      HgAutomate::verbose(1,
	   "        run.$tDb.$tDb/loadPairwise.csh\n\n");
    }
    if (! $opt_queryOnly) {
      HgAutomate::verbose(1,
	   " *** -noLoad was specified -- you can run these scripts " .
	   "manually to load $tDb tables:\n");
      foreach my $qDb (@qDbs) {
	my $qPrefix = &dbToPrefix($qDb);
	HgAutomate::verbose(1,
	   "        run.$tDb.$qDb/loadPairwise.csh\n");
      }
      HgAutomate::verbose(1, "\n");
    }
    if (! $opt_targetOnly) {
      my $tPrefix = &dbToPrefix($tDb);
      HgAutomate::verbose(1,
	   " *** -noLoad was specified -- you can run these scripts " .
	   "manually to load ${tPrefix}BlastTab in query databases:\n");
      foreach my $qDb (@qDbs) {
	my $qPrefix = &dbToPrefix($qDb);
	HgAutomate::verbose(1,
	   "        run.$qDb.$tDb/loadPairwise.csh\n");
      }
      HgAutomate::verbose(1, "\n");
    }
  } else {
    if (! $opt_queryOnly) {
      HgAutomate::verbose(1,
			  " *** Check these tables in $tDb:\n *** ");
      if (! $opt_noSelf) {
	HgAutomate::verbose(1, $tGenesetPrefix . 'BlastTab ');
      }
      foreach my $qDb (@qDbs) {
	my $qPrefix = &dbToPrefix($qDb);
	HgAutomate::verbose(1, $qPrefix . 'BlastTab ');
      }
      HgAutomate::verbose(1, "\n");
    }
    if (! $opt_targetOnly) {
      my $tPrefix = &dbToPrefix($tDb);
      HgAutomate::verbose(1,
	   " *** Check $tPrefix" . "BlastTab in these databases:\n *** ");
      foreach my $qDb (@qDbs) {
	HgAutomate::verbose(1, "$qDb ");
      }
      HgAutomate::verbose(1, "\n");
    }
  }
  HgAutomate::verbose(1, "\n");
} # celebrate


###########################################################################
# MAIN

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();

# Parse config file into target and query db specs and fasta paths.
&usage(1) if (scalar(@ARGV) != 1);
($CONFIG) = @ARGV;
&parseConfig($CONFIG);

# Split target fasta.
&HgAutomate::run("$HgAutomate::runSSH $distrHost mkdir $scratchDir");
my $tFasta = $dbToFasta{$tDb};
&splitSequence($tDb, $tFasta);

# Self blastp.
&formatSequence($tDb, $tFasta);
if (! $opt_noSelf && ! $opt_queryOnly) {
  &runPairwiseBlastp($tDb, $tDb, $selfE, $selfMaxPer);
  &loadPairwise($tDb, $tDb, $tGenesetPrefix, $selfMaxPer);
}

# For each query db, pairwise blastp.
my $tPrefix = &dbToPrefix($tDb);
foreach my $qDb (@qDbs) {
  my $qFasta = $dbToFasta{$qDb};
  my $qPrefix = &dbToPrefix($qDb);
  my $e = &calcE($tDb, $qDb);
  if ($recipBest{$qDb} || ! $opt_queryOnly) {
    # tDb vs qDb
    &formatSequence($qDb, $qFasta);
    &runPairwiseBlastp($tDb, $qDb, $e, $pairwiseMaxPer);
  }
  if (! $recipBest{$qDb} && ! $opt_queryOnly) {
    &loadPairwise($tDb, $qDb, $qPrefix, $pairwiseMaxPer);
  }
  if ($recipBest{$qDb} || ! $opt_targetOnly) {
    # qDb vs tDb
    &splitSequence($qDb, $qFasta);
    &runPairwiseBlastp($qDb, $tDb, $e, $pairwiseMaxPer);
  }
  if (! $recipBest{$qDb} && ! $opt_targetOnly) {
    &loadPairwise($qDb, $tDb, $tPrefix, $pairwiseMaxPer);
  }
  if ($recipBest{$qDb}) {
    &recipBest($tDb, $qDb);
    if (! $opt_queryOnly) {
      &loadPairwise($tDb, $qDb, $qPrefix, $pairwiseMaxPer, "$tDb.$qDb.recipBest.tab");
    }
    if (! $opt_targetOnly) {
      &loadPairwise($qDb, $tDb, $tPrefix, $pairwiseMaxPer, "$qDb.$tDb.recipBest.tab");
    }
  }
}

# Clean up.
&cleanup($tDb, @qDbs);

&celebrate();

