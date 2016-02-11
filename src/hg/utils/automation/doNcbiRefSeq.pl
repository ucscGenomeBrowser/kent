#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doNcbiRefSeq.pl instead.

# HOW TO USE THIS TEMPLATE:
# 1. Global-replace doNcbiRefSeq.pl with your actual script name.
# 2. Search for template and replace each instance with something appropriate.
#    Add steps and subroutines as needed.  Other do*.pl or make*.pl may have
#    useful example code -- this is just a skeleton.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

my $gff3ToRefLink = "$Bin/gff3ToRefLink.pl";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_genbank
    $opt_subgroup
    $opt_species
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download', func => \&doDownload },
      { name => 'process', func => \&doProcess },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] genbank|refseq subGroup species asmId db
required arguments:
    genbank|refseq - specify either genbank or refseq hierarchy source
    subGroup       - specify subGroup at NCBI FTP site, examples:
                   - vertebrate_mammalian vertebrate_other plant etc...
    species        - species directory at NCBI FTP site, examples:
                   - Homo_sapiens Mus_musculus etc...
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..
    db             - database to load with track tables

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ncbiRefSeq.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's ncbiRefSeq track build.  Steps:
    download: rsync required files from NCBI FTP site
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/template.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/hg or /iscratch, /san etc).
4. template?
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: genbankRefseq subGroup species asmId db
my ($genbankRefseq, $subGroup, $species, $asmId, $db);
# Other:
my ($buildDir);

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
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download required set of files from NCBI.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);
  my $ftpDir = "genomes/$genbankRefseq/$subGroup/$species/all_assembly_versions/$asmId";
  my $outsideCopy = "/hive/data/outside/ncbi/$ftpDir";
  my $localData = "/hive/data/inside/ncbi/$ftpDir";
  $localData =~ s/all_assembly_versions/latest_assembly_versions/;
  my $local2Bit = "$localData/$asmId.ncbi.2bit";

  # see if local symLinks can be made with copies already here from NCBI:
  if ( -d "$outsideCopy" && -s $local2Bit ) {
    $bossScript->add(<<_EOF_
ln -s $outsideCopy/${asmId}_genomic.gff.gz .
ln -s $local2Bit .
for F in _rna.gbff.gz _rna.fna.gz
do
   rsync -a -P \\
rsync://ftp.ncbi.nlm.nih.gov/$ftpDir/$asmId\${F} ./
done
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
for F in _rna.gbff.gz _rna.fna.gz _genomic.gff.gz _genomic.fna.gz
do
   rsync -a -P \\
rsync://ftp.ncbi.nlm.nih.gov/$ftpDir/$asmId\${F} ./
done
faToTwoBit ${asmId}_genomic.fna.gz ${asmId}.ncbi.2bit
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
twoBitDup -keyList=stdout $asmId.ncbi.2bit \\
  | sort > ncbi.$asmId.sequence.keys
twoBitDup -keyList=stdout /hive/data/genomes/$db/$db.2bit \\
  | sort > ucsc.$db.sequence.keys
twoBitInfo $asmId.ncbi.2bit stdout | sort -k2nr > $asmId.chrom.sizes
faToTwoBit ${asmId}_rna.fna.gz t.2bit
twoBitInfo t.2bit stdout | sort -k2nr > rna.chrom.sizes
rm -f t.2bit
join -t'\t' ucsc.$db.sequence.keys ncbi.$asmId.sequence.keys \\
   | cut -f2-3 | sort \\
     | join -t'\t' - <(sort /hive/data/genomes/$db/chrom.sizes) \\
       | awk -F'\t' '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$2, \$3, \$1, \$3}' \\
          | sort -k3nr > ${asmId}To${db}.lift
zegrep "VERSION|COMMENT" ${asmId}_rna.gbff.gz | awk '{print \$2}' \\
    | xargs -L 2 echo | tr '[ ]' '[\\t]' | sort > rna.status.tab
_EOF_
  );
  $bossScript->execute();
} # doDownload

#########################################################################
# * step: process [workhorse]
sub doProcess {
  my $runDir = "$buildDir/process";
  my $downloadDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process NCBI download files into track files.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doProcess.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
/cluster/home/hiram/kent/src/hg/utils/gff3ToGenePred/gff3ToGenePred -useName \\
  -attrsOut=$asmId.attrs.txt -allowMinimalGenes \\
    -unprocessedRootsOut=$asmId.unprocessedRoots.txt \\
      $downloadDir/${asmId}_genomic.gff.gz $asmId.gp
genePredCheck $asmId.gp
cut -f1 $asmId.gp | sort -u > genePred.name.list
$gff3ToRefLink $downloadDir/rna.status.tab $downloadDir/${asmId}_genomic.gff.gz 2> refLink.stderr.txt \\
  | sort > $asmId.refLink.tab
join -t'\t' genePred.name.list $asmId.refLink.tab > ncbi.metaData.tab
liftUp -extGenePred -type=.gp stdout $downloadDir/${asmId}To${db}.lift drop $asmId.gp \\
  | gzip -c > $asmId.$db.gp.gz
genePredCheck -db=$db $asmId.$db.gp.gz
zegrep "^N(M|R)|^YP" $asmId.$db.gp.gz > curated.gp
zegrep "^X(M|R)" $asmId.$db.gp.gz > predicted.gp
zegrep -v "^N(M|R)|^YP|X(M|R)" $asmId.$db.gp.gz > other.gp
genePredCheck -db=$db curated.gp
genePredCheck -db=$db predicted.gp
genePredCheck -db=$db other.gp
zgrep "^#" $downloadDir/${asmId}_genomic.gff.gz > gffForPsl.gff
zgrep -v "NG_" $downloadDir/${asmId}_genomic.gff.gz \\
  | egrep -w "match|cDNA_match" >> gffForPsl.gff
gff3ToPsl $downloadDir/$asmId.chrom.sizes $downloadDir/rna.chrom.sizes \\
  $asmId.psl 
simpleChain -outPsl $asmId.psl stdout | pslSwap stdin stdout \\
  | liftUp -type=.psl stdout $downloadDir/${asmId}To${db}.lift drop stdin \\
   | gzip -c $db.psl.gz
pslCheck -db=hg38 $db.psl.gz
_EOF_
  );
  $bossScript->execute();
} # doProcess

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
rm -rf run.template/raw/
rm -rf templateOtherBigTempFilesOrDirectories
gzip template
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
&usage(1) if (scalar(@ARGV) != 5);

# expected command line arguments after options are processed
($genbankRefseq, $subGroup, $species, $asmId, $db) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/template.$date";

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

