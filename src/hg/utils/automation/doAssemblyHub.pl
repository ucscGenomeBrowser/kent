#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doAssemblyHub.pl instead.

# HOW TO USE THIS TEMPLATE:
# 1. Global-replace doTemplate.pl with your actual script name.
# 2. Search for template and replace each instance with something appropriate.
#    Add steps and subroutines as needed.  Other do*.pl or make*.pl may have
#    useful example code -- this is just a skeleton.

use Getopt::Long;
use File::Temp qw(tempfile);
use File::stat;
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
    $opt_sourceDir
    $opt_ucscNames
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&doDownload },
      { name => 'sequence',   func => \&doSequence },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $sourceDir = "/hive/data/outside/ncbi/genomes";
my $ucscNames = 0;  # default 'FALSE' (== 0)

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] genbank|refseq subGroup species asmId
required arguments:
    genbank|refseq - specify either genbank or refseq hierarchy source
    subGroup       - specify subGroup at NCBI FTP site, examples:
                   - vertebrate_mammalian vertebrate_other plant etc...
    species        - species directory at NCBI FTP site, examples:
                   - Homo_sapiens Mus_musculus etc...
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir     Construct assembly hub in dir instead of default
       $HgAutomate::clusterData/asmHubs/{genbank|refseq}/subGroup/species/asmId/
    -sourceDir dir    Find assembly in dir instead of default
                          $sourceDir
                          the assembly is found at:
  sourceDir/{genbank|refseq}/subGroup/species/all_assembly_versions/asmId/
    -ucscNames        Translate NCBI/INSDC/RefSeq names to UCSC names
                      default is to use the given NCBI/INSDC/RefSeq names
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates build of assembly hub.  Steps:
    doDownload: sets up sym link working hierarchy from already mirrored
                files from NCBI in:
                      $sourceDir/{genbank|refseq}/
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
# Command line args: genbankRefseq subGroup species asmId
my ($genbankRefseq, $subGroup, $species, $asmId);
# Other:
my ($buildDir, $secondsStart, $secondsEnd, $assemblySource);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'ucscNames',
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

  my $whatItDoes = "setup work hierarchy of sym links to source files in\n\t$runDir/";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
if ${asmId}_genomic.fna.gz -nt $asmId.2bit ]; then
  rm -f ${asmId}_genomic.fna.gz \\
    ${asmId}_assembly_report.txt \\
    ${asmId}_rm.out.gz \\
    ${asmId}_assembly_structure \\
    $asmId.2bit

  ln -s $assemblySource/${asmId}_genomic.fna.gz .
  ln -s $assemblySource/${asmId}_assembly_report.txt .
  ln -s $assemblySource/${asmId}_rm.out.gz .
  if [ -d $assemblySource/${asmId}_assembly_structure ]; then
    ln -s $assemblySource/${asmId}_assembly_structure .
  fi
  faToTwoBit ${asmId}_genomic.fna.gz $asmId.2bit
  touch -r ${asmId}_genomic.fna.gz $asmId.2bit
else
  printf "# download step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # doDownload

#########################################################################
#  return true if result does not exist or it is older than source
#  else return false
sub needsUpdate($$) {
  my ($source, $result) = @_;
  if (-s $result) {
     if (stat($source)->mtime > stat($result)->mtime ) {
       return 1;
     } else {
       return 0;
     }
  }
  return 1;
}

#########################################################################
# read chr2acc file, return name correspondence in given hash pointer
sub readChr2Acc($$) {
  my ($chr2acc, $accToChr) = @_;
  open (FH, "<$chr2acc") or die "can not read $chr2acc";
  while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\t', $line);
    $chrN =~ s/ /_/g;   # some assemblies have spaces in chr names ...
    $accToChr->{$acc} = $chrN;
  }
  close (FH);
}

#########################################################################
# process NCBI AGP file into UCSC naming scheme
#   the agpNames result file is a naming correspondence file for later use
sub compositeAgp($$$$) {
  my ($chr2acc, $agpSource, $agpOutput, $agpNames) = @_;
  my %accToChr;
  readChr2Acc($chr2acc, \%accToChr);

  open (AGP, "|gzip -c >$agpOutput") or die "can not write to $agpOutput";
  open (NAMES, "|sort -u >$agpNames") or die "can not write to $agpNames";
  foreach my $acc (keys %accToChr) {
    my $chrN =  $accToChr{$acc};
    my $ncbiChr = "chr${chrN}";
    open (FH, "zcat $agpSource/${ncbiChr}.comp.agp.gz|") or die "can not read $ncbiChr.comp.agp.gz";
    while (my $line = <FH>) {
        if ($line =~ m/^#/) {
            print AGP $line;
        } else {
            $ncbiChr = "chrM" if ($ncbiChr eq "chrMT");
            printf NAMES "%s\t%s\n", $ncbiChr, $acc;
            $line =~ s/^$acc/$ncbiChr/ if ($ucscNames);
            print AGP $line;
        }
    }
    close (FH);
  }
  close (AGP);
  close (NAMES);
}	# sub compositeAgp($$$$)

#########################################################################
# process NCBI fasta sequence, use UCSC chrom names if requested
sub compositeFasta($$$) {
  my ($chr2acc, $twoBitFile, $fastaOut) = @_;

  my %accToChr;
  readChr2Acc($chr2acc, \%accToChr);
  if (! $ucscNames) {
    foreach my $acc (keys %accToChr) {
       $accToChr{$acc} = $acc;
    }
  }

  my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
  foreach my $acc (sort keys %accToChr) {
     printf $fh "%s\n", $acc;
  }
  close $fh;

  open (FA, "|gzip -c > $fastaOut") or die "can not write to $fastaOut";
  open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
  while (my $line = <FH>) {
    if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($accToChr{$line}));
      if (! $ucscNames) {
         printf FA ">%s\n", $accToChr{$line};
      } else {
        if ( $accToChr{$line} eq "MT" ) {
          printf FA ">chrM\n";
        } else {
          printf FA ">chr%s\n", $accToChr{$line};
        }
      }
    } else {
      print FA $line;
    }
  }
  close(FH);
  close(FA);
}	# sub compositeFasta($$$)

#########################################################################
# process NCBI unlocalized AGP file, perhaps translate into UCSC naming scheme
#   the agpNames result file is a naming correspondence file for later use
sub unlocalizedAgp($$$$) {
  my ($chr2acc, $agpSource, $agpOutput, $agpNames) = @_;

  my %accToChr;
  readChr2Acc($chr2acc, \%accToChr);
  if ($ucscNames) {
    foreach my $acc (keys %accToChr) {
      my $ucscAcc = $acc;
      $ucscAcc =~ s/\./v/;
      $accToChr{$ucscAcc} = $accToChr{$acc};
      $accToChr{$acc} = undef;
    }
  }

  open (AGP, "|gzip -c >$agpOutput") or die "can not write to $agpOutput";
  open (NAMES, "|sort -u >$agpNames") or die "can not write to $agpNames";
  foreach my $acc (keys %accToChr) {
    my $chrN = $accToChr{$acc};
    my $agpFile =  "$agpSource/chr$chrN.unlocalized.scaf.agp.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    while (my $line = <FH>) {
      if ($line !~ m/^#/) {
         chomp $line;
         my (@a) = split('\t', $line);
         my $ncbiAcc = $a[0];
         my $acc = $ncbiAcc;
         $acc =~ s/\./v/ if ($ucscNames);
         die "ERROR: chrN $chrN not correct for $acc"
             if ($accToChr{$acc} ne $chrN);
         my $ucscName = "${acc}";
         $ucscName = "chr${chrN}_${acc}_random" if ($ucscNames);
         printf NAMES "%s\t%s\n", $ucscName, $ncbiAcc;
         printf AGP "%s", $ucscName;    # begin AGP line with accession name
         for (my $i = 1; $i < scalar(@a); ++$i) {   # the rest of the AGP line
             printf AGP "\t%s", $a[$i];
         }
         printf AGP "\n";
      }
    }
    close (FH);
  }
  close (AGP);
  close (NAMES);
}	# sub unlocalizedAgp($$$$)

#########################################################################
# process unlocalized NCBI fasta sequence, use UCSC chrom names if requested
sub unlocalizedFasta($$$) {
  my ($chr2acc, $twoBitFile, $fastaOut) = @_;

  my %accToChr;
  readChr2Acc($chr2acc, \%accToChr);
  if (! $ucscNames) {
    foreach my $acc (keys %accToChr) {
       $accToChr{$acc} = $acc;
    }
  }

  my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
  foreach my $acc (sort keys %accToChr) {
     printf $fh "%s\n", $acc;
  }
  close $fh;

  open (FA, "|gzip -c > $fastaOut") or die "can not write to $fastaOut";
  open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
  while (my $line = <FH>) {
    if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($accToChr{$line}));
      my $chrN = $accToChr{$line};
      my $acc = $line;
      $acc =~ s/\./v/ if ($ucscNames);
      my $ucscName = "${acc}";
      $ucscName = "chr${chrN}_${acc}_random" if ($ucscNames);
      printf FA ">%s\n", $ucscName;
    } else {
      print FA $line;
    }
  }
  close(FH);
  close(FA);
}	# sub unlocalizedFasta($$$)

### XXX TO BE DONE - process alternate haplotypes via alt.scaf.agp.gz

#########################################################################
# process NCBI unplaced AGP file, perhaps translate into UCSC naming scheme
#   the agpNames result file is a naming correspondence file for later use
#   optional chrPrefix can be "chrUn_" for assemblies that have other chr names
sub unplacedAgp($$$$) {
  my ($agpFile, $agpOutput, $agpNames, $chrPrefix) = @_;

  open (AGP, "|gzip -c >$agpOutput") or die "can not write to $agpOutput";
  open (NAMES, "|sort -u >$agpNames") or die "can not write to $agpNames";
  open (FH, "zcat $agpFile|") or die "can not read $agpFile";
  while (my $line = <FH>) {
    if ($line =~ m/^#/) {
        print AGP $line;
    } else {
        if ($ucscNames) {
          my ($ncbiAcc, undef) = split('\s+', $line, 2);
          my $ucscAcc = $ncbiAcc;
          $ucscAcc =~ s/\./v/;
          printf NAMES "%s%s\t%s\n", $chrPrefix, $ucscAcc, $ncbiAcc;
          $line =~ s/\./v/;
        }
        printf AGP "%s%s", $chrPrefix, $line;
    }
  }
  close (FH);
  close (NAMES);
  close (AGP);
}	# sub unplacedAgp($$$$)

#########################################################################
# process NCBI unplaced FASTA file, perhaps translate into UCSC naming scheme
#   optional chrPrefix can be "chrUn_" for assemblies that have other chr names
sub unplacedFasta($$$$) {
  my ($agpFile, $twoBitFile, $chrPrefix, $fastaOut) = @_;

  my %contigName;  # key is NCBI contig name, value is UCSC contig name
  open (FH, "zcat $agpFile|") or die "can not read $agpFile";
  while (my $line = <FH>) {
    if ($line !~ m/^#/) {
        my ($ncbiAcc, undef) = split('\s+', $line, 2);
        my $ucscAcc = $ncbiAcc;
        $ucscAcc =~ s/\./v/ if ($ucscNames);
        $contigName{$ncbiAcc} = $ucscAcc;
    }
  }
  close (FH);

  my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
  foreach my $ctg (sort keys %contigName) {
     printf $fh "%s\n", $ctg;
  }
  close $fh;

  open (FA, "|gzip -c > $fastaOut") or die "can not write to $fastaOut";
  open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
  while (my $line = <FH>) {
    if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($contigName{$line}));
      printf FA ">%s%s\n", $chrPrefix, $contigName{$line};
    } else {
      print FA $line;
    }
  }
  close(FH);
  close(FA);
}	# sub unplacedFasta($$$$)

#########################################################################
# * step: sequence [workhorse]
sub doSequence {
  my $runDir = "$buildDir/sequence";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process source files into 2bit sequence and agp";
  my $workhorse = &HgAutomate::chooseWorkhorse();
  my $bossScript = newBash HgRemoteScript("$runDir/doSequence.bash", $workhorse,
				      $runDir, $whatItDoes);

  my $twoBitFile = "$buildDir/download/$asmId.2bit";
  my $otherChrParts = 0;  # to see if this is unplaced scaffolds only
  my $primaryAssembly = "$buildDir/download/${asmId}_assembly_structure/Primary_Assembly";

  ###########  Assembled chromosomes  ################
  my $chr2acc = "$primaryAssembly/assembled_chromosomes/chr2acc";
  if ( -s $chr2acc ) {
    ++$otherChrParts;
    my $agpSource = "$primaryAssembly/assembled_chromosomes/AGP";
    my $agpOutput = "$runDir/$asmId.chr.agp.gz";
    my $agpNames = "$runDir/$asmId.chr.names";
    my $fastaOut = "$runDir/$asmId.chr.fa.gz";
    if (needsUpdate($chr2acc, $agpOutput)) {
      compositeAgp($chr2acc, $agpSource, $agpOutput, $agpNames);
      `touch -r $chr2acc $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      compositeFasta($chr2acc, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  ###########  unlocalized sequence  ################
  my $chr2scaf = "$primaryAssembly/unlocalized_scaffolds/unlocalized.chr2scaf";
  if ( -s $chr2scaf ) {
    ++$otherChrParts;
    my $agpSource = "$primaryAssembly/unlocalized_scaffolds/AGP";
    my $agpOutput = "$runDir/$asmId.unlocalized.agp.gz";
    my $agpNames = "$runDir/$asmId.unlocalized.names";
    my $fastaOut = "$runDir/$asmId.unlocalized.fa.gz";
    if (needsUpdate($chr2scaf, $agpOutput)) {
      unlocalizedAgp($chr2scaf, $agpSource, $agpOutput, $agpNames);
      `touch -r $chr2scaf $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      unlocalizedFasta($chr2scaf, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  ###########  unplaced sequence  ################
  my $unplacedScafAgp = "$primaryAssembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz";
  if ( -s $unplacedScafAgp ) {
    my $chrPrefix = "";   # no prefix if no other chrom parts
    $chrPrefix = "chrUn_" if ($otherChrParts);
    my $agpOutput = "$runDir/$asmId.unplaced.agp.gz";
    my $agpNames = "$runDir/$asmId.unplaced.names";

    if (needsUpdate($unplacedScafAgp, $agpOutput)) {
      unplacedAgp($unplacedScafAgp, $agpOutput, $agpNames, $chrPrefix);
      `touch -r $unplacedScafAgp $agpOutput`;
    }
    my $fastaOut = "$runDir/$asmId.unplaced.fa.gz";
    if (needsUpdate($twoBitFile, $fastaOut)) {
      unplacedFasta($unplacedScafAgp, $twoBitFile, $chrPrefix, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  ###########  non-nuclear chromosome sequence  ################
  my $nonNucAsm = "$buildDir/download/${asmId}_assembly_structure/non-nuclear";
  my $nonNucChr2acc = "$nonNucAsm/assembled_chromosomes/chr2acc";
  my $agpSource = "$nonNucAsm/assembled_chromosomes/AGP";
  if ( -s $nonNucChr2acc ) {
    my $agpOutput = "$runDir/$asmId.nonNucChr.agp.gz";
    my $agpNames = "$runDir/$asmId.nonNucChr.names";
    my $fastaOut = "$runDir/$asmId.nonNucChr.fa.gz";
    if (needsUpdate($nonNucChr2acc, $agpOutput)) {
      compositeAgp($nonNucChr2acc, $agpSource, $agpOutput, $agpNames);
      `touch -r $nonNucChr2acc $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      compositeFasta($nonNucChr2acc, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  ###########  non-nuclear scaffold unlocalized sequence  ################
  my $nonNucChr2scaf = "$nonNucAsm/unlocalized_scaffolds/unlocalized.chr2scaf";
  if ( -s $nonNucChr2scaf ) {
    my $agpOutput = "$runDir/$asmId.nonNucUnlocalized.agp.gz";
    my $agpNames = "$runDir/$asmId.nonNucUnlocalized.names";
    my $fastaOut = "$runDir/$asmId.nonNucUnlocalized.fa.gz";
    if (needsUpdate($nonNucChr2scaf, $agpOutput)) {
      compositeAgp($nonNucChr2scaf, $agpSource, $agpOutput, $agpNames);
      `touch -r $nonNucChr2scaf $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      compositeFasta($nonNucChr2scaf, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

### XXX TO BE DONE - construct sequence when no AGP files exist

  $bossScript->add(<<_EOF_
export asmId="$asmId"


if [ -s ../\$asmId.chrom.sizes ]; then
  printf "sequence stop already completed\\n" 1>&2
  exit 0
fi

zcat *.agp.gz | gzip > ../\$asmId.agp
faToTwoBit *.fa.gz ../\$asmId.2bit
twoBitInfo ../\$asmId.2bit stdout | sort -k2nr > ../\$asmId.chrom.sizes
# verify everything is there
twoBitInfo ../download/\$asmId.2bit stdout | sort -k2nr > source.\$asmId.chrom.sizes
export newTotal=`ave -col=2 ../\$asmId.chrom.sizes | grep "^total"`
export oldTotal=`ave -col=2 source.\$asmId.chrom.sizes | grep "^total"`
if [ "\$newTotal" != "\$oldTotal" ]; then
  printf "# sequence construction error: not same totals\n" 1>&2
  printf "# \$newTotal != \$oldTotal\n" 1>&2
  exit 255
fi

_EOF_
  );
  $bossScript->execute();
} # doSequence

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "clean up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
printf "to be done\n" 1>&2
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
&usage(1) if (scalar(@ARGV) != 4);
$secondsStart = `date "+%s"`;
chomp $secondsStart;

# expected command line arguments after options are processed
($genbankRefseq, $subGroup, $species, $asmId) = @ARGV;

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/asmHubs/$genbankRefseq/$subGroup/$species/$asmId";

$sourceDir = $opt_sourceDir ? $opt_sourceDir : $sourceDir;
$ucscNames = $opt_ucscNames ? 1 : $ucscNames;   # '1' == 'TRUE'

$assemblySource = "$sourceDir/$genbankRefseq/$subGroup/$species/all_assembly_versions/$asmId";

die "can not find assembly source directory\n$assemblySource" if ( ! -d $assemblySource);
printf STDERR "# buildDir: %s\n", $buildDir;
printf STDERR "# assemblySource: %s\n", $assemblySource;

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

&HgAutomate::verbose(1,
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

