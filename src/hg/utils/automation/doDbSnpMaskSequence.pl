#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doDbSnpMaskSequence.pl instead.

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
    [ { name => 'mask',  func => \&mask },
      { name => 'gzip',  func => \&gzip },
      { name => 'package',  func => \&package },
    ]
			       );

# Hardcoded (for now):
my $goldenPath = '/usr/local/apache/htdocs-hgdownload/goldenPath';

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
                          $HgAutomate::clusterData/<db>/snp<buildNum>Mask.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '');
  print STDERR "
Makes a copy of <db> assembly chromosome FASTA with IUPAC ambiguous nucleotide
characters in the place of each single-nucleotide substitution in snp<buildNum>.
Stages the new FASTA files for hgdownload-test.
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
# * step: mask [workhorse]
sub mask {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $whatItDoes = "It filters out SNPs with various exceptions and makes masked FASTA.";
  my $bossScript = new HgRemoteScript("$runDir/mask.csh", $workhorse,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf substitutions
mkdir substitutions
zcat /hive/data/outside/dbSNP/${buildNum}/human_$db/snp${buildNum}.bed.gz \\
| awk '\$18 !~ /MultipleAlignments|ObservedTooLong|ObservedWrongFormat|ObservedMismatch|MixedObserved/ {print;}' \\
| snpMaskSingle stdin /hive/data/genomes/$db/$db.2bit stdout diffObserved.txt \\
| faSplit byname stdin substitutions/
wc -l diffObserved.txt
foreach f (substitutions/chr*.fa)
  echo \$f:t:r
  mv \$f \$f:r.subst.fa
end

# At one point we also made masked sequence with indels but there wasn't much demand for
# it and it messes up the coords.  If we ever need to do that again, see hg19 snp131.
_EOF_
  );
  $bossScript->execute();
} # mask


#########################################################################
# * step: gzip [cluster]
sub gzip {
  my $runDir = "$buildDir/run.gzip";
  &HgAutomate::mustMkdir($runDir);
  my $paraHub = $opt_bigClusterHub ? $opt_bigClusterHub :
                                     &HgAutomate::chooseClusterByBandwidth();
  my $whatItDoes = "It gzips the per-chrom masked FASTA files.";
  my $bossScript = new HgRemoteScript("$runDir/gzip.csh", $paraHub,
				      $runDir, $whatItDoes);
  my $paraRun = &HgAutomate::paraRun();
  $bossScript->add(<<_EOF_
cp /dev/null jobList
foreach f (../substitutions/*.fa)
  echo gzip \$f >> jobList
end
$paraRun
_EOF_
  );
  $bossScript->execute();
} # gzip


#########################################################################
# * step: package [dbHost]
sub package {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "It stages files in a directory for download from hgdownload-test.";
  my $bossScript = new HgRemoteScript("$runDir/package.csh", $dbHost,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
# Create md5sum.txt and download links for hgdownload-test.
cd substitutions
md5sum *.gz > md5sum.txt
rm -rf $goldenPath/$db/snp${buildNum}Mask
mkdir $goldenPath/$db/snp${buildNum}Mask
ln -s `pwd`/* $goldenPath/$db/snp${buildNum}Mask/
_EOF_
  );
  # Make a README.txt before executing so it will be symlinked along with the other files.
  my ($genome, $date, $source) = HgAutomate::getAssemblyInfo($dbHost, $db);
  my $readmeFile = $opt_debug ? "/dev/null" : "$runDir/substitutions/README.txt";
  my $fh = &HgAutomate::mustOpen(">$readmeFile");
  print $fh <<EOF
This directory contains FASTA files which contain a modified version
of the $date reference human genome assembly.
The chromosomal sequences were assembled by the International Human
Genome Project sequencing centers.  The assembly sequence was changed
to use IUPAC ambiguous nucleotide characters at each base covered by a
stringently filtered subset of single-base substitutions annotated by
dbSNP build ${buildNum}.  For example, if the assembly has an 'A' at a position
where dbSNP has annotated an A/C/T substitution SNP, the 'A' is replaced
by 'H' in the FASTA file here.

dbSNP single-base substitutions were excluded from masking in the
following cases:
- UCSC tagged the dbSNP item with any of these exceptions (see also the
  exceptions field of the $db.snp${buildNum} database table as well as the
  $db.snp${buildNum}ExceptionDesc table):
  - MultipleAlignments: dbSNP mapped item to multiple locations
  - ObservedMismatch: the reference allele does not appear in the item's
    observed alleles.
  - ObservedWrongFormat: the observed sequence has an unexpected format
- dbSNP item class is not "single".
- dbSNP item length is not exactly one base.
- dbSNP item weight is greater than 1.  (lower weight = higher confidence)
The remaining single-base substitutions were used to mask the genomic
sequence.

Files included in this directory:

chr*.subst.fa.gz - FASTA files with IUPAC characters for substitution SNPs

md5sum.txt - checksums of files in this directory

------------------------------------------------------------------
If you plan to download a large file or multiple files from this
directory, we recommend that you use ftp rather than downloading the
files via our website. To do so, ftp to hgdownload.soe.ucsc.edu
[username: anonymous, password: your email address], then cd to the
directory goldenPath/$db/bigZips. To download multiple files, use
the "mget" command:

    mget <filename1> <filename2> ...
    - or -
    mget -a (to download all the files in the directory)

Alternate methods to ftp access.

Using an rsync command to download the entire directory:
    rsync -avzP rsync://hgdownload.soe.ucsc.edu/goldenPath/$db/snp${buildNum}Mask/ .
For a single file, e.g. chr1.subst.fa.gz
    rsync -avzP \
        rsync://hgdownload.soe.ucsc.edu/goldenPath/$db/snp${buildNum}Mask/chr1.subst.fa.gz .

Or with wget, all files:
    wget --timestamping \
        'ftp://hgdownload.soe.ucsc.edu/goldenPath/$db/snp${buildNum}Mask/*'
With wget, a single file:
    wget --timestamping \
        'ftp://hgdownload.soe.ucsc.edu/goldenPath/$db/snp${buildNum}Mask/chr1.subst.fa.gz' \
        -O chr1.subst.fa.gz

To uncompress the fa.gz files:
    gunzip <file>.fa.gz

EOF
  ;
  close($fh);
  $bossScript->execute();
} # package


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
  "$HgAutomate::clusterData/$db/snp${buildNum}Mask.$date";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'package') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1, <<_EOF_

 *** All done!$upThrough
 *** Steps were performed in $buildDir
_EOF_
);
