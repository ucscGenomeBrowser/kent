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
    $opt_species
    $opt_rmskSpecies
    $opt_runRepeatModeler
    $opt_ncbiRmsk
    $opt_noRmsk
    $opt_augustusSpecies
    $opt_noAugustus
    $opt_xenoRefSeq
    $opt_noXenoRefSeq
    $opt_ucscNames
    $opt_dbName
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&doDownload },
      { name => 'sequence',   func => \&doSequence },
      { name => 'assemblyGap',   func => \&doAssemblyGap },
      { name => 'chromAlias',   func => \&doChromAlias },
      { name => 'gatewayPage',   func => \&doGatewayPage },
      { name => 'cytoBand',   func => \&doCytoBand },
      { name => 'gc5Base',   func => \&doGc5Base },
      { name => 'repeatModeler',   func => \&doRepeatModeler },
      { name => 'repeatMasker',   func => \&doRepeatMasker },
      { name => 'simpleRepeat',   func => \&doSimpleRepeat },
      { name => 'allGaps',   func => \&doAllGaps },
      { name => 'idKeys',   func => \&doIdKeys },
      { name => 'windowMasker',   func => \&doWindowMasker },
      { name => 'addMask',   func => \&doAddMask },
      { name => 'gapOverlap',   func => \&doGapOverlap },
      { name => 'tandemDups',   func => \&doTandemDups },
      { name => 'cpgIslands',   func => \&doCpgIslands },
      { name => 'ncbiGene',   func => \&doNcbiGene },
      { name => 'ncbiRefSeq',   func => \&doNcbiRefSeq },
      { name => 'xenoRefGene',   func => \&doXenoRefGene },
      { name => 'augustus',   func => \&doAugustus },
      { name => 'trackDb',   func => \&doTrackDb },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $sourceDir = "/hive/data/outside/ncbi/genomes";
my $species = "";       # usually found in asmId_assembly_report.txt
my $ftpDir = "";	# will be determined from given asmId
my $rmskSpecies = "";
my $runRepeatModeler = 0;       # default off
my $noRmsk = 0;	# when RepeatMasker is not possible, such as bacteria
my $ncbiRmsk = 0;	# when =1 call doRepeatMasker.pl
                        # with -ncbiRmsk=path.out.gz and -liftSpec=...
my $augustusSpecies = "human";
my $xenoRefSeq = "/hive/data/genomes/asmHubs/xenoRefSeq";
my $noAugustus = 0;     # bacteria do *not* create an augustus track
my $noXenoRefSeq = 0;	# bacteria do *not* create a xenoRefSeq track
my $ucscNames = 0;  # default 'FALSE' (== 0)
my $dbName = "";  # default uses NCBI asmId for name, can specify: abcDef1
my $defaultName = "";  # will be asmId or dbName if present
my $workhorse = "hgwdev";  # default workhorse when none chosen
my $fileServer = "hgwdev";  # default when none chosen
my $bigClusterHub = "ku";  # default when none chosen
my $smallClusterHub = "ku";  # default when none chosen

my $base = $0;
$base =~ s/^(.*\/)?//;

# key is original accession name from the remove.dups.list, value is 1
my %dupAccessionList;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] asmId
required arguments:
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir     Construct assembly hub in dir instead of default
       $HgAutomate::clusterData/asmHubs/refseqBuild/GC[AF]/123/456/789/asmId/
    -sourceDir dir    Find assembly in dir instead of default:
       $sourceDir/GC[AF]/123/456/789/asmId
    -ucscNames        Translate NCBI/INSDC/RefSeq names to UCSC names
                      default is to use the given NCBI/INSDC/RefSeq names
    -dbName <name>    name for UCSC style database name, e.g. 'abcDef1'
    -species <name>   use this species designation if there is no
                      asmId_assembly_report.txt with an
                      'Organism name:' entry to obtain species
    -rmskSpecies <name> to override default 'species' name for repeat masker
                      the default is found in the asmId_asssembly_report.txt
                      e.g. -rmskSpecies=viruses
    -runRepeatModeler run RepeatModeler to supply custom lib to RepeatMasker,
                      default is to not run RepeatModeler
    -noRmsk           when RepeatMasker is not possible, such as bacteria
    -ncbiRmsk         use NCBI rm.out.gz file instead of local cluster run
                      for repeat masking
    -augustusSpecies <human|chicken|zebrafish> default 'human'
    -noAugustus       do *not* create the Augustus gene track
    -noXenoRefSeq     do *not* create the Xeno RefSeq gene track
    -xenoRefSeq </path/to/xenoRefSeqMrna> - location of xenoRefMrna.fa.gz
                expanded directory of mrnas/ and xenoRefMrna.sizes, default
                $xenoRefSeq
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => $workhorse,
						'fileServer' => $fileServer,
						'bigClusterHub' => $bigClusterHub,
						'smallClusterHub' => $smallClusterHub);
  print STDERR "
Automates build of assembly hub.  Steps:
    download: sets up sym link working hierarchy from already mirrored
                files from NCBI in:
                      $sourceDir/GC[AF]/123/456/789/asmId
    sequence: establish AGP and 2bit file from NCBI directory
    assemblyGap: create assembly and gap bigBed files and indexes
                 for assembly track names
    chromAlias:  construct asmId.chromAlias.txt for alias name recognition
    gatewayPage: create html/asmId.description.html contents
    cytoBand: create cytoBand track and navigation ideogram
    gc5Base: create bigWig file for gc5Base track
    repeatModeler: optionally, run RepeatModeler to construct custom library
                   for repeatMasker run.  Use: -runRepeatModeler to perform
                   this procedure, warning: can take a considerable amount
                   of time (12 to 48 hours or more), and consumes an entire
                   ku cluster node
    repeatMasker: run repeat masker cluster run and create bigBed files for
                  the composite track categories of repeats
    simpleRepeat: run trf cluster run and create bigBed file for simple repeats
    allGaps: calculate all actual real gaps due to N's in sequence, can be
                  more than were specified in the AGP file
    idKeys: calculate md5sum for each sequence in the assembly to be used to
            find identical sequences in similar assemblies
    windowMasker: run windowMasker cluster run, create windowMasker bigBed file
                  and compute intersection with repeatMasker results
    addMask: combine the higher masking of (windowMasker or repeatMasker) with
                  trf simpleRepeats into one 2bit file
    gapOverlap: find duplicated sequence on each side of a gap
    tandemDups: annotate all pairs of duplicated sequence with some gap between
    cpgIslands: run CpG islands cluster runs for both masked and unmasked
                sequences and create bigBed files for this composite track
    ncbiGene: on RefSeq assemblies, construct a gene track from the
              NCBI gff3 predictions
    ncbiRefSeq on RefSeq assemblies, construct a gene track from the
              NCBI gff3 predictions
    xenoRefSeq: map RefSeq mRNAs to the assembly to construct a 'xeno'
                gene prediction track
    augustus: run the augustus gene prediction on the assembly
    trackDb: create trackDb.txt file for assembly hub to include all constructed
             bigBed and bigWig tracks
    cleanup: Removes or compresses intermediate files. (NOOP at this time !)
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
# Command line args: asmId
my ( $asmId);
# Other:
my ($buildDir, $secondsStart, $secondsEnd, $assemblySource, $asmReport);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'sourceDir=s',
		      'rmskSpecies=s',
		      'runRepeatModeler',
		      'noRmsk',
		      'ncbiRmsk',
		      'augustusSpecies=s',
		      'xenoRefSeq=s',
		      'dbName=s',
		      'noXenoRefSeq',
		      'noAugustus',
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
#########################################################################
#  assistant subroutines here.  The 'do' steps follow this section
#########################################################################
#########################################################################
# if there are duplicates in original sequence, read in that list for use here
sub readDupsList() {
  my $downloadDir = "$buildDir/download";

  if ( -s "${downloadDir}/${asmId}.remove.dups.list" ) {
    open (FH, "<${downloadDir}/${asmId}.remove.dups.list") or die "can not read ${downloadDir}/${asmId}.remove.dups.list";
    while (my $accession = <FH>) {
      chomp $accession;
      $dupAccessionList{$accession} = 1;
    }
    close (FH);
  }
}

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
    $chrN =~ s/:/_/g;   # one assembly GCF_002910315.2 had : in a chr name
    $chrN = "chr" if ($chrN eq "na");	# seen in bacteria with only one chr
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
    $ncbiChr = "chr" if ($chrN eq "chr");    # seen in bacteria with only 1 chr
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
     if (! exists($dupAccessionList{$acc})) {
       printf $fh "%s\n", $acc;
     }
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
  `rm -f $tmpFile`;
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
      delete $accToChr{$acc};
    }
  }

  open (AGP, "|gzip -c >$agpOutput") or die "can not write to $agpOutput";
  open (NAMES, "|sort -u >$agpNames") or die "can not write to $agpNames";
  my %chrNDone;	# key is chrom name, value is 1 when done
  foreach my $acc (keys %accToChr) {
    my $chrN = $accToChr{$acc};
    next if (exists($chrNDone{$chrN}));
    $chrNDone{$chrN} = 1;
    my $agpFile =  "$agpSource/chr$chrN.unlocalized.scaf.agp.gz";
    open (FH, "zcat $agpFile|") or die "can not read $agpFile";
    while (my $line = <FH>) {
      if ($line !~ m/^#/) {
         chomp $line;
         my (@a) = split('\t', $line);
         my $ncbiAcc = $a[0];
         next if (exists($dupAccessionList{$ncbiAcc}));
         my $acc = $ncbiAcc;
         $acc =~ s/\./v/ if ($ucscNames);
         die "ERROR: chrN $chrN not correct for $acc"
             if ($accToChr{$acc} ne $chrN);
         my $ucscName = "$ncbiAcc";
         $ucscName = "chr${chrN}_${acc}_random";
         $ucscName =~ s/\./v/;
         printf NAMES "%s\t%s\n", $ucscName, $ncbiAcc;
         if ($ucscNames) {
            printf AGP "%s", $ucscName;    # begin AGP line with accession name
         } else {
            printf AGP "%s", $ncbiAcc;    # begin AGP line with accession name
         }
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
     if (! exists($dupAccessionList{$acc})) {
       printf $fh "%s\n", $acc;
     }
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
  `rm -f $tmpFile`;
}	# sub unlocalizedFasta($$$)

#########################################################################
# read alt_scaffold_placement file, return name correspondence in
# given hash pointer
sub readAltPlacement($$) {
  my ($altPlacementFile, $accToChr) = @_;
  open (AP, "<$altPlacementFile") or die "can not read $altPlacementFile";
  while (my $line = <AP>) {
    chomp $line;
    next if ($line =~ m/^#/);
    my $fixAlt = "alt";
    $fixAlt = "fix" if ($altPlacementFile =~ m/PATCH/);
    my ($alt_asm_name, $prim_asm_name, $alt_scaf_name, $alt_scaf_acc, $parent_type, $parent_name, $parent_acc, $region_name, $ori, $alt_scaf_start, $alt_scaf_stop, $parent_start, $parent_stop, $alt_start_tail, $alt_stop_tail) = split('\t', $line);
    my $acc = $alt_scaf_acc;
    $alt_scaf_acc = $acc;
    my $ucscName = $acc;
    # should always be calculating UCSC names here, the resulting names
    # can be used for either naming scheme later in AGP and FA processing
    $alt_scaf_acc =~ s/\./v/;
    $ucscName = sprintf("chr%s_%s_%s", $parent_name, $alt_scaf_acc, $fixAlt);
    if ( $prim_asm_name ne "Primary Assembly" ) {
      $ucscName = sprintf("%s_%s", $alt_scaf_acc, $fixAlt);
    }
    $accToChr->{$acc} = $ucscName;
    printf STDERR "# warning: name longer than 31 characters: '%s'\n# in: '%s'\n", $ucscName, $altPlacementFile if (length($ucscName) > 31);
  }
  close (AP);
}	#	sub readAltPlacement($$)

#########################################################################
### process one of the alternate AGP files, changing names via the nameHash
### and writing to the given fileHandle (fh)
sub processAltAgp($$$) {
  my ($agpFile, $nameHash, $fh) = @_;
  open (AG, "zcat $agpFile|") or die "can not read $agpFile";
  while (my $line = <AG>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my (@a) = split('\t', $line);
    my $ncbiAcc = $a[0];
    next if (exists($dupAccessionList{$ncbiAcc}));
    my $ucscName = $nameHash->{$ncbiAcc};
    if ($ucscNames) {
      printf $fh "%s", $ucscName;  # begin AGP line with accession nam
    } else {
      printf $fh "%s", $ncbiAcc;  # begin AGP line with accession nam
    }
    for (my $i = 1; $i < scalar(@a); ++$i) {  # the reset of the AGP line
      printf $fh "\t%s", $a[$i];
    }
    printf $fh "\n";
  }
  close (AG);
}	#	sub processAltAgp($$$)

#########################################################################
### process one of the alternate FASTA files, changing names via the nameHash
### and writing to the given fileHandle (fh)

sub processAltFasta($$$) {
  my ($faFile, $nameHash, $fh) = @_;
  open (FF, "zcat $faFile|") or die "can not read $faFile";
  while (my $line = <FF>) {
    if ($line =~ m/^>/) {
      chomp $line;
      my $ncbiAcc = $line;
      $ncbiAcc =~ s/^>//;
      $ncbiAcc =~ s/ .*//;
      die "ERROR: can not find accession $ncbiAcc in nameHash" if (! exists($nameHash->{$ncbiAcc}));
      my $ucscName = $nameHash->{$ncbiAcc};
      if ($ucscNames) {
        printf $fh ">%s\n", $ucscName;
      } else {
        printf $fh ">%s\n", $ncbiAcc;
      }
    } else {
      print $fh $line;
    }
  }
  close (FF);
}	#	sub processAltFasta($$$)

#########################################################################
# there are alternate sequences, process their multiple AGP and FASTA files
# into single AGP and FASTA files
sub altSequences($) {
  my $runDir = "$buildDir/sequence";
  my ($assemblyStructure) = @_;
  # construct the name correspondence hash #################################
  my %accToChr;	# hash for accession name to browser chromosome name
  open (FH, "find -L '${assemblyStructure}' -type f | grep '/alt_scaffolds/alt_scaffold_placement.txt'|") or die "can not find alt_scaffolds in ${assemblyStructure}";;
  while (my $line = <FH>) {
   chomp $line;
   readAltPlacement($line, \%accToChr);
  }
  close (FH);
  # and write to alt.names
  open (AN, ">$runDir/$asmId.alt.names");
  foreach my $acc (sort keys %accToChr) {
    printf AN "%s\t%s\n", $accToChr{$acc}, $acc;
  }
  close (AN);
  # process the AGP files, writing the agpOutput file ######################
  my $agpOutput = "$runDir/$asmId.alt.agp.gz";
  open (AGP, "|gzip -c > $agpOutput") or die "can not write to $agpOutput";
  open (FH, "find -L '${assemblyStructure}' -type f | grep '/alt_scaffolds/AGP/alt.scaf.agp.gz'|") or die "can not find alt.scaf.agp.gz in ${assemblyStructure}";;
  while (my $agpFile = <FH>) {
    chomp $agpFile;
    processAltAgp($agpFile, \%accToChr, \*AGP);
    printf STDERR "%s\n", $agpFile;
  }
  close (FH);
  close (AGP);
  # process the FASTA files, writing the fastaOut file ######################
  my $fastaOut = "$runDir/$asmId.alt.fa.gz";
  open (FA, "|gzip -c > $fastaOut") or die "can not write to $fastaOut";
  open (FH, "find -L '${assemblyStructure}' -type f | grep '/alt_scaffolds/FASTA/alt.scaf.fna.gz'|") or die "can not find alt.scaf.fna.gz in ${assemblyStructure}";;
  while (my $faFile = <FH>) {
    chomp $faFile;
    processAltFasta($faFile, \%accToChr, \*FA);
    printf STDERR "%s\n", $faFile;
  }
  close (FH);
  close (FA);
}	#	sub altSequences($)

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
        my ($ncbiAcc, undef) = split('\s+', $line, 2);
        next if (exists($dupAccessionList{$ncbiAcc}));
        my $ucscAcc = $ncbiAcc;
        $ucscAcc =~ s/\./v/;
        printf NAMES "%s%s\t%s\n", $chrPrefix, $ucscAcc, $ncbiAcc;
        if ($ucscNames) {
          $line =~ s/\./v/;
          printf AGP "%s%s", $chrPrefix, $line;
        } else {
          printf AGP "%s", $line;
        }
    }
  }
  close (FH);
  close (NAMES);
  close (AGP);
}	# sub unplacedAgp($$$$)

#########################################################################
# process NCBI unplaced FASTA file, perhaps translate into UCSC naming scheme
#   optional chrPrefix can be "chrUn_" for assemblies that have other chr names
sub unplacedFasta($$$$$$$) {
  my ($agpFile, $faFile, $twoBitFile, $chrPrefix, $fastaOut, $agpOutput, $agpNames) = @_;

  my %contigName;  # key is NCBI contig name, value is UCSC contig name
  if ( -s $agpFile ) {
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
  } else {	# no AGP file, get the contig names from the fasta file
    open (FH, "zgrep '^>' $faFile | cut -d' ' -f1 | tr -d '>'|") or die "can not read $faFile";
    while (my $ncbiAcc = <FH>) {
      chomp $ncbiAcc;
      my $ucscAcc = $ncbiAcc;
      $ucscAcc =~ s/\./v/ if ($ucscNames);
      $contigName{$ncbiAcc} = $ucscAcc;
    }
    close (FH);
  }

  my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
  foreach my $ctg (sort keys %contigName) {
     if (! exists($dupAccessionList{$ctg})) {
       printf $fh "%s\n", $ctg;
     }
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
      if ($ucscNames) {
        printf FA ">%s%s\n", $chrPrefix, $contigName{$line};
      } else {
        printf FA ">%s\n", $contigName{$line};
      }
    } else {
      print FA $line;
    }
  }
  close(FH);
  close(FA);
  if ( ! -s $agpFile ) {	# there was no AGP file, make a fake one
     printf STDERR "# no AGP for unplaced sequence, making up a fake AGP\n";
     print `hgFakeAgp -singleContigs -minContigGap=1 -minScaffoldGap=50000 $fastaOut stdout | gzip -c > $agpOutput`;
     open (NAMES, "|sort -u >$agpNames") or die "can not write to $agpNames";
     open (FH, "zcat $agpOutput|") or die "can not read $agpOutput";
     while (my $line = <FH>) {
       if ($line !~ m/^#/) {
          my ($ncbiAcc, undef) = split('\s+', $line, 2);
          next if (exists($dupAccessionList{$ncbiAcc}));
          my $ucscAcc = $ncbiAcc;
          $ucscAcc =~ s/\./v/;
          printf NAMES "%s%s\t%s\n", $chrPrefix, $ucscAcc, $ncbiAcc;
       }
     }
    close (FH);
    close (NAMES);
  }
  `rm -f $tmpFile`;
}	# sub unplacedFasta($$$$)

#########################################################################
#########################################################################
# do Steps section
#########################################################################
#########################################################################
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "setup work hierarchy of sym links to source files in\n\t$runDir/";
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$asmId

if [ ! -s \${asmId}.2bit -o \${asmId}_genomic.fna.gz -nt \$asmId.2bit ]; then
  rm -f \${asmId}_genomic.fna.gz \\
    \${asmId}_genomic.fna.dups.gz \\
    \${asmId}_assembly_report.txt \\
    \${asmId}_rm.out.gz \\
    \${asmId}_rm.run \\
    \${asmId}_assembly_structure \\
    \$asmId.2bit

  ln -s $assemblySource/\${asmId}_genomic.fna.gz .
  ln -s $assemblySource/\${asmId}_assembly_report.txt .
  if [ -s $assemblySource/\${asmId}_rm.out.gz ]; then
    ln -s $assemblySource/\${asmId}_rm.out.gz .
  fi
  if [ -s $assemblySource/\${asmId}_rm.run ]; then
    ln -s $assemblySource/\${asmId}_rm.run .
  fi
  if [ -d $assemblySource/\${asmId}_assembly_structure ]; then
    ln -s $assemblySource/\${asmId}_assembly_structure .
  fi
  export asmSize=`grep -v "^#" \${asmId}_assembly_report.txt | head | cut -f9 | ave stdin | grep total | awk '{printf "%d", \$NF}'`
  export longArg=""
  if [ "\$asmSize" -gt 4294967295 ]; then
    longArg="-long"
  fi
  faToTwoBit \${longArg} \${asmId}_genomic.fna.gz \$asmId.2bit
  twoBitDup \$asmId.2bit > \$asmId.dups.txt
  if [ -s "\$asmId.dups.txt" ]; then
    printf "WARNING duplicate sequences found in \$asmId.2bit\\n" 1>&2
    cat \$asmId.dups.txt 1>&2
    awk '{print \$1}' \$asmId.dups.txt > \$asmId.remove.dups.list
    mv \${asmId}_genomic.fna.gz \${asmId}_genomic.fna.dups.gz
    faSomeRecords -exclude \${asmId}_genomic.fna.dups.gz \\
      \$asmId.remove.dups.list stdout | gzip -c > \${asmId}_genomic.fna.gz
    rm -f \$asmId.2bit
    faToTwoBit \${longArg} \${asmId}_genomic.fna.gz \$asmId.2bit
  fi
  gzip -f \$asmId.dups.txt
  touch -r \${asmId}_genomic.fna.gz \$asmId.2bit
else
  printf "# download step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();

} # doDownload


#########################################################################
# * step: sequence [workhorse]
sub doSequence {
  my $runDir = "$buildDir/sequence";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process source files into 2bit sequence and agp";
  my $bossScript = newBash HgRemoteScript("$runDir/doSequence.bash", $workhorse,
				      $runDir, $whatItDoes);

  my $twoBitFile = "$buildDir/download/$asmId.2bit";
  my $otherChrParts = 0;  # to see if this is unplaced scaffolds only
  my $primaryAssembly = "$buildDir/download/${asmId}_assembly_structure/Primary_Assembly";
  my $partsDone = 0;

  readDupsList();

  ###########  Assembled chromosomes  ################
  my $chr2acc = "$primaryAssembly/assembled_chromosomes/chr2acc";
  if ( -s $chr2acc ) {
    ++$otherChrParts;
    my $agpSource = "$primaryAssembly/assembled_chromosomes/AGP";
    my $agpOutput = "$runDir/$asmId.chr.agp.gz";
    my $agpNames = "$runDir/$asmId.chr.names";
    my $fastaOut = "$runDir/$asmId.chr.fa.gz";
    $partsDone += 1;
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
    $partsDone += 1;
    if (needsUpdate($chr2scaf, $agpOutput)) {
      unlocalizedAgp($chr2scaf, $agpSource, $agpOutput, $agpNames);
      `touch -r $chr2scaf $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      unlocalizedFasta($chr2scaf, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  ###########  alternate sequences  ##############
  my $assemblyStructure = "$buildDir/download/${asmId}_assembly_structure";
  my $altCount = `find -L "${assemblyStructure}" -type f | grep "/alt_scaffolds/alt_scaffold_placement.txt" | wc -l`;
  chomp $altCount;
  if ($altCount > 0) {
    $partsDone += 1;
    ++$otherChrParts;
    altSequences($assemblyStructure);
  }

  ###########  unplaced sequence  ################
  my $unplacedScafAgp = "$primaryAssembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz";
  my $unplacedScafFa = "$primaryAssembly/unplaced_scaffolds/FASTA/unplaced.scaf.fna.gz";
  if ( -s $unplacedScafAgp || -s $unplacedScafFa) {
    my $chrPrefix = "";   # no prefix if no other chrom parts
    $chrPrefix = "chrUn_" if ($otherChrParts && ! $ucscNames);
    my $agpOutput = "$runDir/$asmId.unplaced.agp.gz";
    my $agpNames = "$runDir/$asmId.unplaced.names";
    $partsDone += 1;

    if ( -s $unplacedScafAgp ) {
      if (needsUpdate($unplacedScafAgp, $agpOutput)) {
        unplacedAgp($unplacedScafAgp, $agpOutput, $agpNames, $chrPrefix);
        `touch -r $unplacedScafAgp $agpOutput`;
      }
    }
    if ( -s $unplacedScafFa ) {	# will make an AGP file if there was none
      my $fastaOut = "$runDir/$asmId.unplaced.fa.gz";
      if (needsUpdate($twoBitFile, $fastaOut)) {
        unplacedFasta($unplacedScafAgp, $unplacedScafFa, $twoBitFile, $chrPrefix, $fastaOut, $agpOutput, $agpNames);
        `touch -r $twoBitFile $fastaOut`;
      }
    }
  }

  ###########  non-nuclear chromosome sequence  ################
  my $nonNucAsm = "$buildDir/download/${asmId}_assembly_structure/non-nuclear";
  my $nonNucChr2acc = "$nonNucAsm/assembled_chromosomes/chr2acc";
  if ( -s $nonNucChr2acc ) {
    my $agpSource = "$nonNucAsm/assembled_chromosomes/AGP";
    my $agpOutput = "$runDir/$asmId.nonNucChr.agp.gz";
    my $agpNames = "$runDir/$asmId.nonNucChr.names";
    my $fastaOut = "$runDir/$asmId.nonNucChr.fa.gz";
    $partsDone += 1;
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
    my $agpSource = "$nonNucAsm/unlocalized_scaffolds/AGP";
    my $agpOutput = "$runDir/$asmId.nonNucUnlocalized.agp.gz";
    my $agpNames = "$runDir/$asmId.nonNucUnlocalized.names";
    my $fastaOut = "$runDir/$asmId.nonNucUnlocalized.fa.gz";
    $partsDone += 1;
    if (needsUpdate($nonNucChr2scaf, $agpOutput)) {
      unlocalizedAgp($nonNucChr2scaf, $agpSource, $agpOutput, $agpNames);
      `touch -r $nonNucChr2scaf $agpOutput`;
    }
    if (needsUpdate($twoBitFile, $fastaOut)) {
      unlocalizedFasta($nonNucChr2scaf, $twoBitFile, $fastaOut);
      `touch -r $twoBitFile $fastaOut`;
    }
  }

  $bossScript->add(<<_EOF_
export asmId="$asmId"
export dbName="$defaultName"

if [ -s ../\$dbName.chrom.sizes ]; then
  printf "sequence step previously completed\\n" 1>&2
  exit 0
fi

_EOF_
  );

### construct sequence when no AGP files exist
  if (0 == $partsDone) {
    printf STDERR "# partsDone is zero, creating fake AGP\n";
    if ($ucscNames) {
      $bossScript->add(<<_EOF_
twoBitToFa ../download/\$asmId.2bit stdout | sed -e "s/\\.\\([0-9]\\+\\)/v\\1/;" | gzip -c > \$asmId.fa.gz
hgFakeAgp -singleContigs -minContigGap=1 -minScaffoldGap=50000 \$asmId.fa.gz stdout | gzip -c > \$asmId.fake.agp.gz
zgrep "^>" \$asmId.fa.gz | sed -e 's/>//;' | sed -e 's/\\(.*\\)/\\1 \\1/;' | sed -e 's/v\\([0-9]\\+\\)/.\\1/;' | awk '{printf "%s\\t%s\\n", \$2, \$1}' > \$asmId.fake.names
_EOF_
      );
    } else {
      $bossScript->add(<<_EOF_
twoBitToFa ../download/\$asmId.2bit stdout | gzip -c > \$asmId.fa.gz
hgFakeAgp -singleContigs -minContigGap=1 -minScaffoldGap=50000 \$asmId.fa.gz stdout | gzip -c > \$asmId.fake.agp.gz
twoBitInfo ../download/\$asmId.2bit stdout | cut -f1 \\
  | sed -e "s/\\.\\([0-9]\\+\\)/v\\1/;" \\
    | sed -e 's/\\(.*\\)/\\1 \\1/;' | sed -e 's/v\\([0-9]\\+\$\\)/.\\1/;' \\
      | awk '{printf "%s\\t%s\\n", \$1, \$2}' | sort > \$asmId.fake.names
_EOF_
      );
    }
} else {
printf STDERR "partsDone: %d\n", $partsDone;
  }

  $bossScript->add(<<_EOF_
zcat *.agp.gz | gzip > ../\$dbName.agp.gz
export asmSize=`zgrep -v "^#" ../\$dbName.agp.gz | cut -f3 | ave stdin | grep total | awk '{printf "%d", \$NF}'`
export longArg=""
if [ "\$asmSize" -gt 4294967295 ]; then
  longArg="-long"
fi
faToTwoBit \${longArg} *.fa.gz ../\$dbName.2bit
faToTwoBit \${longArg} -noMask *.fa.gz ../\$dbName.unmasked.2bit
twoBitDup ../\$dbName.unmasked.2bit > \$asmId.dups.txt
if [ -s "\$asmId.dups.txt" ]; then
  printf "ERROR: duplicate sequences found in ../\$dbName.unmasked.2bit\\n" 1>&2
  cat \$asmId.dups.txt 1>&2
  awk '{print \$1}' \$asmId.dups.txt > \$asmId.remove.dups.list
  mv ../\$dbName.unmasked.2bit ../\$dbName.unmasked.dups.2bit
  twoBitToFa ../\$dbName.unmasked.dups.2bit stdout | faSomeRecords -exclude \\
    stdin \$asmId.remove.dups.list stdout | gzip -c > \$asmId.noDups.fasta.gz
  rm -f ../\$dbName.2bit ../\$dbName.unmasked.2bit
  faToTwoBit \${longArg} \$asmId.noDups.fasta.gz ../\$dbName.2bit
  faToTwoBit \${longArg} -noMask \$asmId.noDups.fasta.gz ../\$dbName.unmasked.2bit
fi
gzip -f \$asmId.dups.txt
touch -r ../download/\$asmId.2bit ../\$dbName.2bit
touch -r ../download/\$asmId.2bit ../\$dbName.unmasked.2bit
touch -r ../download/\$asmId.2bit ../\$dbName.agp.gz
twoBitInfo ../\$dbName.2bit stdout | sort -k2nr > ../\$dbName.chrom.sizes
touch -r ../\$dbName.2bit ../\$dbName.chrom.sizes
# verify everything is there
twoBitInfo ../download/\$asmId.2bit stdout | sort -k2nr > source.\$asmId.chrom.sizes
export newTotal=`ave -col=2 ../\$dbName.chrom.sizes | grep "^total"`
export oldTotal=`ave -col=2 source.\$asmId.chrom.sizes | grep "^total"`
if [ "\$newTotal" != "\$oldTotal" ]; then
  printf "# ERROR: sequence construction error: not same totals source vs. new:\\n" 1>&2
  printf "# \$newTotal != \$oldTotal\\n" 1>&2
  exit 255
fi
rm source.\$asmId.chrom.sizes
export checkAgp=`checkAgpAndFa ../\$dbName.agp.gz ../\$dbName.2bit 2>&1 | tail -1`
if [ "\$checkAgp" != "All AGP and FASTA entries agree - both files are valid" ]; then
  printf "# ERROR: checkAgpAndFa \$dbName.agp.gz \$dbName.2bit failing\\n" 1>&2
  exit 255
fi
_EOF_
  );
  if ($ucscNames) {
    $bossScript->add(<<_EOF_
join -t\$'\\t' <(sort ../\$dbName.chrom.sizes) <(sort \${asmId}*.names) | awk '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$3,\$2,\$1,\$2}' > \$asmId.ncbiToUcsc.lift
join -t\$'\\t' <(sort ../\$dbName.chrom.sizes) <(sort \${asmId}*.names) | awk '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$1,\$2,\$3,\$2}' > \$asmId.ucscToNcbi.lift
export c0=`cat \$asmId.ncbiToUcsc.lift | wc -l`
export c1=`cat \$asmId.ucscToNcbi.lift | wc -l`
export c2=`cat ../\$dbName.chrom.sizes | wc -l`
# verify all names are accounted for
if [ "\$c0" -ne "\$c2" ]; then
  printf "# ERROR: not all names accounted for in \$asmId.ncbiToUcsc.lift" 1>&2
  exit 255
fi
if [ "\$c1" -ne "\$c2" ]; then
  printf "# ERROR: not all names accounted for in \$asmId.ucscToNcbi.lift" 1>&2
  exit 255
fi
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
twoBitToFa ../\$dbName.2bit stdout | faCount stdin | gzip -c > \$asmId.faCount.txt.gz
touch -r ../\$dbName.2bit \$asmId.faCount.txt.gz
zgrep -P "^total\t" \$asmId.faCount.txt.gz > \$asmId.faCount.signature.txt
touch -r ../\$dbName.2bit \$asmId.faCount.signature.txt
_EOF_
  );
  $bossScript->execute();
} # doSequence

#########################################################################
# * step: assemblyGap [workhorse]
sub doAssemblyGap {
  my $runDir = "$buildDir/trackData/assemblyGap";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct assembly and gap tracks from AGP file";
  my $bossScript = newBash HgRemoteScript("$runDir/doAssemblyGap.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId="$defaultName"

if [ ../../\$asmId.agp.gz -nt \$asmId.assembly.bb ]; then

zcat ../../\$asmId.agp.gz | grep -v "^#" | awk '\$5 != "N" && \$5 != "U"' \\
   | awk '{printf "%s\\t%d\\t%d\\t%s\\t0\\t%s\\n", \$1, \$2-1, \$3, \$6, \$9}' \\
      | sort -k1,1 -k2,2n > \$asmId.assembly.bed

zcat ../../\$asmId.agp.gz | grep -v "^#" | awk '\$5 == "N" || \$5 == "U"' \\
   | awk '{printf "%s\\t%d\\t%d\\t%s\\n", \$1, \$2-1, \$3, \$8}' \\
      | sort -k1,1 -k2,2n > \$asmId.gap.bed

bedToBigBed -extraIndex=name -verbose=0 \$asmId.assembly.bed \\
    ../../\$asmId.chrom.sizes \$asmId.assembly.bb
touch -r ../../\$asmId.agp.gz \$asmId.assembly.bb
\$HOME/kent/src/hg/utils/automation/genbank/nameToIx.pl \\
    \$asmId.assembly.bed | sort -u > \$asmId.assembly.ix.txt
if [ -s \$asmId.assembly.ix.txt ]; then
  ixIxx \$asmId.assembly.ix.txt \$asmId.assembly.ix \$asmId.assembly.ixx
fi

if [ -s \$asmId.gap.bed ]; then
  bedToBigBed -extraIndex=name -verbose=0 \$asmId.gap.bed \\
    ../../\$asmId.chrom.sizes \$asmId.gap.bb
  touch -r ../../\$asmId.agp.gz \$asmId.gap.bb
fi

rm -f \$asmId.assembly.bed \$asmId.gap.bed \$asmId.assembly.ix.txt

else
  printf "# assemblyGap step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # assemblyGap

#########################################################################
# * step: chromAlias [workhorse]
sub doChromAlias {
  my $runDir = "$buildDir/trackData/chromAlias";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct asmId.chromAlias.txt for alias name recognition";
  my $bossScript = newBash HgRemoteScript("$runDir/doChromAlias.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export buildDir=$buildDir
export asmId="$defaultName"

\$HOME/kent/src/hg/utils/automation/asmHubChromAlias.pl \\
    \${asmId} $asmId | sort > \${asmId}.chromAlias.txt

\$HOME/kent/src/hg/utils/automation/aliasTextToBed.pl \\
  -chromSizes=\$buildDir/\$asmId.chrom.sizes \\
    -aliasText=\${asmId}.chromAlias.txt \\
      -aliasBed=\${asmId}.chromAlias.bed \\
        -aliasAs=\${asmId}.chromAlias.as \\
        -aliasBigBed=\${asmId}.chromAlias.bb

bigBedToBed -header \${asmId}.chromAlias.bb test.chromAlias.bed
\$HOME/kent/src/hg/utils/automation/aliasBedToCt.pl \\
  test.chromAlias.bed \${asmId}.chromAlias.bb .

# verify each sequence name has an alias
export sizeCount=`grep -c . ../../\${asmId}.chrom.sizes`
export aliasCount=`grep -c -v "^#" \${asmId}.chromAlias.txt`
export testCount=`grep -c -v "^#" test.chromAlias.bed`
if [ "\${sizeCount}" -ne "\${aliasCount}" ]; then
  printf "ERROR: chromAlias: incorrect number of aliases chromSizes %d > %d aliasCount\\n" "\${sizeCount}" "\${aliasCount}" 1>&2
  exit 255
fi
if [ "\${sizeCount}" -ne "\${testCount}" ]; then
  printf "ERROR: chromAlias: incorrect number of aliases chromSizes %d > %d testCount in bigBed file\\n" "\${sizeCount}" "\${testCount}" 1>&2
  exit 255
fi

exit 0

_EOF_
  );
  $bossScript->execute();
} # chromAlias

#########################################################################
# * step: gatewayPage [workhorse]
sub doGatewayPage {
  my $runDir = "$buildDir/html";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct html/$defaultName.description.html";
  my $bossScript = newBash HgRemoteScript("$runDir/doGatewayPage.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $photoJpg = "noPhoto";
  my $photoCredit = "noPhoto";
  my $photoLink = "";
  my $speciesNoBlank = $species;
  $speciesNoBlank =~ s/ /_/g;
printf STDERR "# looking for photo species: %s\n", ${speciesNoBlank};
  if ( -s "$runDir/../photo/$speciesNoBlank.jpg" ) {
     $photoJpg = "../photo/${speciesNoBlank}.jpg";
     $photoCredit = "../photo/photoCredits.txt";
     $photoLink = "rm -f ${speciesNoBlank}.jpg; ln -s ../photo/${speciesNoBlank}.jpg ."
  } else {
     printf STDERR "# gatewayPage: warning: no photograph available\n";
  }

printf STDERR "# asmId: %s\n", $defaultName;
printf STDERR "# asmReport %s\n", $asmReport;
printf STDERR "# chrom.sizes: ../%s.chrom.sizes\n", $defaultName;
printf STDERR "# photoJpg %s\n", $photoJpg;
printf STDERR "# photoCredit %s\n", $photoCredit;

  $bossScript->add(<<_EOF_
export asmId="$defaultName"
export asmReport="$asmReport"

\$HOME/kent/src/hg/utils/automation/asmHubGatewayPage.pl \\
     \${asmReport} ../\${asmId}.chrom.sizes \\
         $photoJpg $photoCredit \\
           > \$asmId.description.html 2> \$asmId.names.tab
\$HOME/kent/src/hg/utils/automation/genbank/buildStats.pl \\
       ../\$asmId.chrom.sizes 2> \$asmId.build.stats.txt
$photoLink
_EOF_
  );
  $bossScript->execute();
} # gatewayPage

#########################################################################
# * step: cytoBand [workhorse]
sub doCytoBand {
  my $runDir = "$buildDir/trackData/cytoBand";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct cytoBand track and navigation ideogram";
  my $bossScript = newBash HgRemoteScript("$runDir/doCytoBand.bash",
                    $workhorse, $runDir, $whatItDoes);

  if ( ! -s "$buildDir/$defaultName.chrom.sizes" ) {
      printf STDERR "ERROR: sequence step not completed\n";
      printf STDERR "can not find: $buildDir/$defaultName.chrom.sizes\n";
      exit 255;
  }

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ ../../\$asmId.chrom.sizes -nt \$asmId.cytoBand.bb ]; then
  awk '{printf "%s\\t0\\t%d\\t\\tgneg\\n", \$1, \$2}' ../../\$asmId.chrom.sizes | sort -k1,1 -k2,2n > \$asmId.cytoBand.bed
  bedToBigBed -type=bed4+1 -as=\$HOME/kent/src/hg/lib/cytoBand.as -tab \$asmId.cytoBand.bed ../../\$asmId.chrom.sizes \$asmId.cytoBand.bb

  touch -r ../../\$asmId.chrom.sizes \$asmId.cytoBand.bb
else
  printf "# cytoBand step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # cytoBand

#########################################################################
# * step: gc5Base [workhorse]
sub doGc5Base {
  my $runDir = "$buildDir/trackData/gc5Base";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct gc5Base bigWig track data";
  my $bossScript = newBash HgRemoteScript("$runDir/doGc5Base.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ ../../\$asmId.2bit -nt \$asmId.gc5Base.bw ]; then
  hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \\
    ../../\$asmId.2bit \\
      | gzip -c > \$asmId.wigVarStep.gz
  wigToBigWig \$asmId.wigVarStep.gz ../../\$asmId.chrom.sizes \$asmId.gc5Base.bw
  rm -f \$asmId.wigVarStep.gz
  touch -r ../../\$asmId.2bit \$asmId.gc5Base.bw
else
  printf "# gc5Base step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # gc5Base

#########################################################################
# * step: repeatModeler [workhorse, bigClusterHub]
sub doRepeatModeler {
  if (! $runRepeatModeler) {
       &HgAutomate::verbose(1, "# RepeatModeler not being run, add argument: -runRepeatModeler to run this step before RepeatMasker\n");
	return;
  }
  my $runDir = "$buildDir/trackData/repeatModeler";
  #  check if been run before
  if ( -d "${runDir}" ) {
    if ( -s "${runDir}/${defaultName}-families.fa" ) {
      &HgAutomate::verbose(1, "\nRepeatModeler step previously completed\n");
      return;
    } elsif ( -s "$buildDir/trackData/repeatModeler/blastDb.bash" ) {
        &HgAutomate::verbose(1, "\nERROR: repeatModeler step may be running\n");
         exit 255;
    }
  }
  &HgAutomate::mustMkdir($runDir);
  my $whatItDoes = "run RepeatModeler on sequence to prepare RepeatMasker custom library";
  my $bossScript = newBash HgRemoteScript("$runDir/doRepeatModeler.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId="${defaultName}"
export buildDir="${buildDir}"

doRepeatModeler.pl -buildDir=`pwd` \\
  -unmaskedSeq=\$buildDir/\$asmId.unmasked.2bit \\
    -bigClusterHub=$bigClusterHub -workhorse=$workhorse \$asmId
_EOF_
  );
  $bossScript->execute();
}	# sub doRepeatModeler

#########################################################################
# * step: repeatMasker [workhorse]
sub doRepeatMasker {
  if ($noRmsk) {
       &HgAutomate::verbose(1, "# -noRmsk == RepeatMasker not being run\n");
	return;
  }
  my $runDir = "$buildDir/trackData/repeatMasker";
  if ( -d "$buildDir/trackData/repeatMasker/run.cluster" ) {
     if ( ! -s "$buildDir/trackData/repeatMasker/faSize.rmsk.txt" ) {
       &HgAutomate::verbose(1,
	"\nERROR: step repeatmasker may be running\n");
       exit 255;
     }
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct repeatMasker track data";
  my $bossScript = newBash HgRemoteScript("$runDir/doRepeatMasker.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $repeatModeler = "$buildDir/trackData/repeatModeler";
  my $customLib = "";
  if ( -s "${repeatModeler}/${defaultName}-families.fa" ) {
     $customLib = "-customLib=\"${repeatModeler}/${defaultName}-families.fa\"";
  }
  my $rmskOpts = "";
  if ($ncbiRmsk) {
     if ( -s "$buildDir/download/${asmId}_rm.out.gz" ) {
       $rmskOpts = " \\
  -ncbiRmsk=\"$buildDir/download/${asmId}_rm.out.gz\" ";
       if ( -s "${buildDir}/download/${asmId}.remove.dups.list" ) {
         $rmskOpts .= " \\
  -dupList=\"${buildDir}/download/${asmId}.remove.dups.list\" ";
       }
       if ($ucscNames) {
         $rmskOpts .= " \\
  -liftSpec=\"$buildDir/sequence/$asmId.ncbiToUcsc.lift\"";
       }
     }
  }

  my $speciesOrLib = "-species=\"\$species\"";
  if ( length(${customLib}) ) {
     $speciesOrLib = "${customLib}";
  }
  $bossScript->add(<<_EOF_
export asmId=$defaultName
export ncbiAsmId=$asmId

if [ $buildDir/\$asmId.2bit -nt faSize.rmsk.txt ]; then
export species=`echo $rmskSpecies | sed -e 's/_/ /g;'`

rm -f versionInfo.txt

doRepeatMasker.pl -stop=mask -buildDir=`pwd` -unmaskedSeq=$buildDir/\$asmId.2bit $rmskOpts \\
  -bigClusterHub=$bigClusterHub -workhorse=$workhorse $speciesOrLib \$asmId

if [ -s "\$asmId.fa.out" ]; then
  gzip \$asmId.fa.out
fi
gzip \$asmId.sorted.fa.out \$asmId.nestedRepeats.bed

doRepeatMasker.pl -continue=cleanup -buildDir=`pwd` -unmaskedSeq=$buildDir/\$asmId.2bit $rmskOpts \\
  -bigClusterHub=$bigClusterHub -workhorse=$workhorse $speciesOrLib \$asmId

if [ ! -s versionInfo.txt ]; then
  if [ -s ../../download/\${ncbiAsmId}_rm.run ]; then
    ln -s ../../download/\${ncbiAsmId}_rm.run versionInfo.txt
  fi
fi

\$HOME/kent/src/hg/utils/automation/asmHubRepeatMasker.sh \$asmId `pwd`/\$asmId.sorted.fa.out.gz `pwd`
else
  printf "# repeatMasker step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # repeatMasker

#########################################################################
# * step: simpleRepeat [workhorse]
sub doSimpleRepeat {
  my $runDir = "$buildDir/trackData/simpleRepeat";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct TRF/simpleRepeat track data";
  my $bossScript = newBash HgRemoteScript("$runDir/doSimpleRepeat.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $trfClusterHub = $smallClusterHub;

  my $seqCount = `cat $buildDir/$defaultName.chrom.sizes | wc -l`;
  chomp $seqCount;
  # check for large seqCount and large genome, then use bigCluster
  # the 100000 and 20000000 are from doSimpleRepeat.pl
  if ( $seqCount > 100000 ) {
     my $genomeSize = `ave -col=2 $buildDir/$defaultName.chrom.sizes | grep -w total | awk '{printf "%d", \$NF}'`;
     chomp $genomeSize;
     if ($genomeSize > 200000000) {
	$trfClusterHub = $bigClusterHub;
     }
  }

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export buildDir=$buildDir

if [ \$buildDir/\$asmId.2bit -nt trfMask.bed.gz ]; then
  doSimpleRepeat.pl -stop=filter -buildDir=`pwd` \\
    -unmaskedSeq=\$buildDir/\$asmId.2bit \\
      -trf409=6 -dbHost=$dbHost -smallClusterHub=$trfClusterHub \\
        -workhorse=$workhorse \$asmId
  doSimpleRepeat.pl -buildDir=`pwd` \\
    -continue=cleanup -stop=cleanup -unmaskedSeq=\$buildDir/\$asmId.2bit \\
      -trf409=6 -dbHost=$dbHost -smallClusterHub=$trfClusterHub \\
        -workhorse=$workhorse \$asmId
  if [ -s simpleRepeat.bed ]; then
    gzip simpleRepeat.bed &
  else
    rm -f simpleRepeat.bed
  fi
  if [ -s trfMask.bed ]; then
    gzip trfMask.bed &
  fi
  wait
fi
_EOF_
  );
  $bossScript->execute();
} # simpleRepeat

##   my $rmskResult = "$buildDir/trackData/repeatMasker/$asmId.rmsk.2bit";
##   if (! -s $rmskResult) {
##     die "simpleRepeat: previous step repeatMasker has not completed\n" .
##       "# not found: $rmskResult\n";
##   }
##   twoBitMask ../repeatMasker/\$asmId.rmsk.2bit -add trfMask.bed \\
##     \$asmId.RM_TRF_masked.2bit

#########################################################################
# * step: allGaps [workhorse]
sub doAllGaps {
  my $runDir = "$buildDir/trackData/allGaps";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct 'all' gap track data";
  my $bossScript = newBash HgRemoteScript("$runDir/doAllGaps.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export buildDir=$buildDir

if [ \$buildDir/\$asmId.2bit -nt \$asmId.allGaps.bb ]; then
  twoBitInfo -nBed ../../\$asmId.2bit stdout | awk '{printf "%s\\t%d\\t%d\\t%d\\t%d\\t+\\n", \$1, \$2, \$3, NR, \$3-\$2}' > \$asmId.allGaps.bed
  if [ ! -s \$asmId.allGaps.bed ]; then
    exit 0
  fi
  if [ -s ../assemblyGap/\$asmId.gap.bb ]; then
    bigBedToBed ../assemblyGap/\$asmId.gap.bb \$asmId.gap.bed
    # verify the 'all' gaps should include the gap track items
    bedIntersect -minCoverage=0.0000000014 \$asmId.allGaps.bed \$asmId.gap.bed \\
      \$asmId.verify.annotated.gap.bed
    gapTrackCoverage=`awk '{print \$3-\$2}' \$asmId.gap.bed \\
      | ave stdin | grep "^total" | awk '{print \$NF}' | sed -e 's/.000000//;'`
    intersectCoverage=`ave -col=5 \$asmId.verify.annotated.gap.bed \\
      | grep "^total" | awk '{print \$NF}' | sed -e 's/.000000//;'`
    if [ \$gapTrackCoverage -ne \$intersectCoverage ]; then
      printf "ERROR: 'all' gaps does not include gap track coverage\\n" 1>&2
      printf "gapTrackCoverage: \$gapTrackCoverage != \$intersectCoverage intersection\\n" 1>&2
      exit 255
    fi
  else
    touch \$asmId.gap.bed
  fi
  bedInvert.pl ../../\$asmId.chrom.sizes \$asmId.allGaps.bed \\
    > \$asmId.NOT.allGaps.bed
  # verify bedInvert worked correctly
  #   sum of both sizes should equal genome size
  both=`cat \$asmId.NOT.allGaps.bed \$asmId.allGaps.bed \\
    | awk '{print \$3-\$2}' | ave stdin | grep "^total" \\
      | awk '{print \$NF}' | sed -e 's/.000000//;'`
  genomeSize=`ave -col=2 ../../\$asmId.chrom.sizes | grep "^total" \\
    | awk '{print \$NF}' | sed -e 's/.000000//;'`
  if [ \$genomeSize -ne \$both ]; then
     printf "ERROR: bedInvert.pl did not function correctly on allGaps.bed\n" 1>&2
     printf "genomeSize: \$genomeSize != \$both both gaps data\n" 1>&2
     exit 255
  fi
  bedInvert.pl ../../\$asmId.chrom.sizes \$asmId.gap.bed \\
      > \$asmId.NOT.gap.bed
  # again, verify bedInvert is working correctly, sum of both == genomeSize
  both=`cat \$asmId.NOT.gap.bed \$asmId.gap.bed \\
    | awk '{print \$3-\$2}' | ave stdin | grep "^total" \\
      | awk '{print \$NF}' | sed -e 's/.000000//;'`
  if [ \$genomeSize -ne \$both ]; then
     printf "ERROR: bedInvert did not function correctly on gap.bed\n" 1>&2
     printf "genomeSize: \$genomeSize != \$both both gaps data\n" 1>&2
     exit 255
  fi
  bedIntersect -minCoverage=0.0000000014 \$asmId.allGaps.bed \\
     \$asmId.NOT.gap.bed \$asmId.notAnnotated.gap.bed
  # verify the intersect functioned correctly
  # sum of new gaps plus gap track should equal all gaps
  allGapCoverage=`awk '{print \$3-\$2}' \$asmId.allGaps.bed \\
     | ave stdin | grep "^total" | awk '{print \$NF}' | sed -e 's/.000000//;'`
  both=`cat \$asmId.notAnnotated.gap.bed \$asmId.gap.bed \\
    | awk '{print \$3-\$2}' | ave stdin | grep "^total" | awk '{print \$NF}' | sed -e 's/.000000//;'`
  if [ \$allGapCoverage -ne \$both ]; then
     printf "ERROR: bedIntersect to identify new gaps did not function correctly\n" 1>&2
     printf "allGaps: \$allGapCoverage != \$both (new + gap track)\n" 1>&2
  fi
  cut -f1-3 \$asmId.allGaps.bed | sort -k1,1 -k2,2n > toBbi.bed
  bedToBigBed -type=bed3 toBbi.bed ../../\$asmId.chrom.sizes \$asmId.allGaps.bb
  rm -f toBbi.bed
  gzip *.bed
else
  printf "# allgaps step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # allGaps

#########################################################################
# * step: idKeys [workhorse]
sub doIdKeys {
  my $runDir = "$buildDir/trackData/idKeys";
  if (! -s "$buildDir/$defaultName.2bit") {
    &HgAutomate::verbose(1, "ERROR: idKeys can not find $defaultName.2bit\n");
    exit 255;
  }
  if (! needsUpdate("$buildDir/$defaultName.2bit", "$runDir/$defaultName.keySignature.txt")) {
     &HgAutomate::verbose(1, "# idKeys step previously completed\n");
     return;
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct ID key data for each contig/chr";
  my $bossScript = newBash HgRemoteScript("$runDir/doIdKeys.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export twoBit=$buildDir/\$asmId.2bit

if [ ../../\$asmId.2bit -nt \$asmId.keySignature.txt ]; then
  doIdKeys.pl -bigClusterHub=$bigClusterHub \$asmId -buildDir=`pwd` -twoBit=\$twoBit
  touch -r \$twoBit \$asmId.keySignature.txt
else
  printf "# idKeys step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # doIdKeys

#########################################################################
# * step: addMask [workhorse]
sub doAddMask {
  my $runDir = "$buildDir/trackData/addMask";

  my $atLeast1 = 0;
  my $goNoGo = 0;
  if (! $noRmsk) {
    if ( ! -s "$buildDir/trackData/repeatMasker/$defaultName.rmsk.2bit" ) {
       printf STDERR "ERROR: repeatMasker step not completed\n";
       printf STDERR "can not find: $buildDir/trackData/repeatMasker/$defaultName.rmsk.2bit\n";
       $atLeast1 += 1;
    }
  }
  if ( ! -s "$buildDir/trackData/windowMasker/$defaultName.cleanWMSdust.2bit" ) {
      printf STDERR "ERROR: windowMasker step not completed\n";
      printf STDERR "can not find: $buildDir/trackData/windowMasker/$defaultName.cleanWMSdust.2bit\n";
      $atLeast1 += 1;
  }
  if ( ! -s "$buildDir/trackData/simpleRepeat/doCleanup.csh" ) {
      printf STDERR "ERROR: simpleRepeat step not completed\n";
      printf STDERR "can not find: $buildDir/trackData/simpleRepeat/doCleanup.csh\n";
      $goNoGo = 1;
  }
  if ($atLeast1 && $goNoGo) {
      printf STDERR "ERROR: must complete repeatMasker, windowMasker and simpleRepeat before addMask\n";
      exit 255;
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "add together (windowMasker or repeatMasker) and trf/simpleRepeats to construct masked 2bit file";
  my $bossScript = newBash HgRemoteScript("$runDir/doAddMask.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $wmMasked=`grep "masked total" $buildDir/trackData/windowMasker/faSize.$defaultName.cleanWMSdust.txt | awk '{print \$1}' | sed -e 's/%//;'`;
  chomp $wmMasked;
  $wmMasked = 0 if ($wmMasked > 98);
  my $rmMasked = 0;
  if (! $noRmsk) {
    if ( -s "$buildDir/trackData/repeatMasker/faSize.rmsk.txt" ) {
      $rmMasked=`grep "masked total" $buildDir/trackData/repeatMasker/faSize.rmsk.txt | awk '{print \$1}' | sed -e 's/%//;'`;
    }
  }

  my $src2BitToMask = "../repeatMasker/$defaultName.rmsk.2bit";
  if ($noRmsk || ($wmMasked > $rmMasked)) {
    $src2BitToMask = "../windowMasker/$defaultName.cleanWMSdust.2bit";
  }

  my $accessionId = $defaultName;
  if ($accessionId =~ m/^GC[AF]_/) {
     my @a = split('_', $defaultName);
     $accessionId = sprintf("%s_%s", $a[0], $a[1]);
  }

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export src2Bit=$src2BitToMask
export accessionId=$accessionId

# if simple repeat has a result, add it, otherwise no add
if [ -s ../simpleRepeat/trfMask.bed.gz ]; then
  if [ ../simpleRepeat/trfMask.bed.gz -nt \$asmId.masked.faSize.txt ]; then
    twoBitMask \$src2Bit -type=.bed \\
       -add ../simpleRepeat/trfMask.bed.gz \$asmId.masked.2bit
  fi
else
  cp -p \$src2Bit \$asmId.masked.2bit
fi

if [ \$asmId.masked.2bit -nt \$asmId.masked.faSize.txt ]; then
  twoBitToFa \$asmId.masked.2bit stdout | gzip -c > \$asmId.fa.gz
  touch -r \$asmId.masked.2bit \$asmId.fa.gz
  faSize \$asmId.fa.gz > \$asmId.masked.faSize.txt
  touch -r \$asmId.masked.2bit \$asmId.masked.faSize.txt
  bptForTwoBit \$asmId.masked.2bit \$asmId.masked.2bit.bpt
  touch -r \$asmId.masked.2bit \$asmId.masked.2bit.bpt
  cp -p \$asmId.fa.gz ../../\$asmId.fa.gz
  cp -p \$asmId.masked.faSize.txt ../../\$asmId.faSize.txt
  cp -p \$asmId.masked.2bit.bpt ../../\$asmId.2bit.bpt
  size=`grep -w bases \$asmId.masked.faSize.txt | cut -d' ' -f1`
  if [ \$size -lt 4294967297 ]; then
    ln \$asmId.masked.2bit \$accessionId.2bit
    gfServer -trans index ../../\$accessionId.trans.gfidx \$accessionId.2bit &
    gfServer -stepSize=5 index ../../\$accessionId.untrans.gfidx \$accessionId.2bit
    wait
  else
    ln \$asmId.masked.2bit \$accessionId.2bit
    gfServerHuge -trans index ../../\$accessionId.trans.gfidx \$accessionId.2bit &
    gfServerHuge -stepSize=5 index ../../\$accessionId.untrans.gfidx \$accessionId.2bit
    wait
  fi
  rm \$accessionId.2bit
  touch -r \$asmId.masked.2bit ../../\$accessionId.trans.gfidx
  touch -r \$asmId.masked.2bit ../../\$accessionId.untrans.gfidx
else
  printf "# addMask step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );

  $bossScript->execute();
} # addMask

#########################################################################
# * step: windowMasker [workhorse]
sub doWindowMasker {
  my $runDir = "$buildDir/trackData/windowMasker";

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "run windowMasker procedure";
  my $bossScript = newBash HgRemoteScript("$runDir/doWindowMasker.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ ../../\$asmId.unmasked.2bit -nt faSize.\$asmId.cleanWMSdust.txt ]; then
  \$HOME/kent/src/hg/utils/automation/doWindowMasker.pl -stop=twobit -buildDir=`pwd` -dbHost=$dbHost \\
    -workhorse=$workhorse -unmaskedSeq=$buildDir/\$asmId.unmasked.2bit \$asmId
  bedInvert.pl ../../\$asmId.chrom.sizes ../allGaps/\$asmId.allGaps.bed.gz \\
    > not.gap.bed
  bedIntersect -minCoverage=0.0000000014 windowmasker.sdust.bed \\
     not.gap.bed stdout | sort -k1,1  -k2,2n > cleanWMask.bed
  twoBitMask $buildDir/\$asmId.unmasked.2bit cleanWMask.bed \\
    \$asmId.cleanWMSdust.2bit
  twoBitToFa \$asmId.cleanWMSdust.2bit stdout \\
    | faSize stdin > faSize.\$asmId.cleanWMSdust.txt
  export intersectRmskWM=0
  if [ -s ../repeatMasker/\$asmId.sorted.fa.out.gz ]; then
    zcat ../repeatMasker/\$asmId.sorted.fa.out.gz | sed -e 's/^  *//; /^\$/d;' \\
      | (egrep -v "^SW|^score" || true) | awk '{printf "%s\\t%d\\t%d\\n", \$5, \$6-1, \$7}' \\
        | bedSingleCover.pl stdin > rmsk.bed
    if [ -s rmsk.bed ]; then
      anyOneHome=`bedIntersect -minCoverage=0.0000000014 cleanWMask.bed \\
          rmsk.bed stdout | bedSingleCover.pl stdin | wc -l`
      if [ \$anyOneHome -gt 0 ]; then
       intersectRmskWM=`bedIntersect -minCoverage=0.0000000014 cleanWMask.bed \\
          rmsk.bed stdout | bedSingleCover.pl stdin | ave -col=4 stdin \\
           | grep "^total" | awk '{print \$2}' | sed -e 's/.000000//;'`
       if [ "x\${intersectRmskWM}y" == "xy" ]; then
         intersectRmskWM=0
       fi
      fi
    fi
  fi
  chromSize=`ave -col=2 ../../\$asmId.chrom.sizes \\
     | grep "^total" | awk '{print \$2}' | sed -e 's/.000000//;'`
  echo \$intersectRmskWM \$chromSize | \\
     awk '{ printf "%d bases of %d (%.3f%%) in intersection\\n", \$1, \$2, 100.0*\$1/\$2}' \\
      > fb.\$asmId.rmsk.windowmaskerSdust.txt
  rm -f not.gap.bed rmsk.bed
  bedToBigBed -type=bed3 cleanWMask.bed ../../\$asmId.chrom.sizes \$asmId.windowMasker.bb
  gzip cleanWMask.bed
  \$HOME/kent/src/hg/utils/automation/doWindowMasker.pl -continue=cleanup -stop=cleanup -buildDir=`pwd` -dbHost=$dbHost \\
    -workhorse=$workhorse -unmaskedSeq=$buildDir/\$asmId.unmasked.2bit \$asmId
else
  printf "# windowMasker step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # windowMasker

#########################################################################
# * step: gapOverlap [workhorse]
sub doGapOverlap {
  my $runDir = "$buildDir/trackData/gapOverlap";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct gap overlap track (duplicate sequence on each side of a gap)";
  my $bossScript = newBash HgRemoteScript("$runDir/doGapOverlap.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ ../../\$asmId.unmasked.2bit -nt \$asmId.gapOverlap.bed.gz ]; then
  doGapOverlap.pl -buildDir=`pwd` -bigClusterHub=$bigClusterHub -smallClusterHub=$smallClusterHub -workhorse=$workhorse -twoBit=../../\$asmId.2bit \$asmId
else
  printf "# gapOverlap step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # doGapOverlap

#########################################################################
# * step: tandemDups [workhorse]
sub doTandemDups {
  my $runDir = "$buildDir/trackData/tandemDups";
  if (! -s "$buildDir/$defaultName.unmasked.2bit") {
    &HgAutomate::verbose(1,
	"ERROR: tandemDups: can not find $buildDir/$defaultName.unmasked.2bit\n");
    exit 255;
  }
  my $ctgCount = `grep -c '^' $buildDir/$defaultName.chrom.sizes`;
  chomp $ctgCount;
  if ( $ctgCount > 100000) {
   &HgAutomate::verbose(1, "# tandemDups step too many contigs at $ctgCount\n");
       return;
  }
  if (-d "${runDir}" ) {
     if (! -s "$runDir/$defaultName.tandemDups.bb") {
       &HgAutomate::verbose(1,
       "WARNING tandemDups step may already be running, but not completed ?\n");
       return;
     } elsif (! needsUpdate("$buildDir/$defaultName.unmasked.2bit", "$runDir/$defaultName.tandemDups.bb")) {
       &HgAutomate::verbose(1, "# tandemDups step previously completed\n");
       return;
     }
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct tandem dups track (nearby pairs of exact duplicate sequence)";
  my $bossScript = newBash HgRemoteScript("$runDir/doTandemDups.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export twoBit=$buildDir/\$asmId.unmasked.2bit

if [ \$twoBit -nt \$asmId.tandemDups.bb ]; then
  doTandemDup.pl -buildDir=`pwd` -bigClusterHub=$bigClusterHub -smallClusterHub=$smallClusterHub -workhorse=$workhorse -twoBit=\$twoBit \$asmId
  touch -r \$twoBit \$asmId.tandemDups.bb
else
  printf "# tandemDups step previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # doTandemDups

#########################################################################
# * step: cpgIslands [workhorse]
sub doCpgIslands {
  my $runDir = "$buildDir/trackData/cpgIslands";

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "run CPG Islands procedures, both masked and unmasked";
  my $bossScript = newBash HgRemoteScript("$runDir/doCpgIslands.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

mkdir -p masked unmasked
cd unmasked
if [ ../../../\$asmId.unmasked.2bit -nt \$asmId.cpgIslandExtUnmasked.bb ]; then
  doCpgIslands.pl -stop=makeBed -buildDir=`pwd` -dbHost=$dbHost \\
    -smallClusterHub=$smallClusterHub -bigClusterHub=$bigClusterHub -tableName=cpgIslandExtUnmasked \\
    -workhorse=$workhorse -maskedSeq=$buildDir/\$asmId.unmasked.2bit \\
    -chromSizes=$buildDir/\$asmId.chrom.sizes \$asmId
  doCpgIslands.pl -continue=cleanup -stop=cleanup -buildDir=`pwd` \\
    -dbHost=$dbHost \\
    -smallClusterHub=$smallClusterHub -bigClusterHub=$bigClusterHub -tableName=cpgIslandExtUnmasked \\
    -workhorse=$workhorse -maskedSeq=$buildDir/\$asmId.unmasked.2bit \\
    -chromSizes=$buildDir/\$asmId.chrom.sizes \$asmId
else
  printf "# cpgIslands unmasked previously completed\\n" 1>&2
fi
cd ../masked
if [ ../../addMask/\$asmId.masked.2bit -nt \$asmId.cpgIslandExt.bb ]; then
  doCpgIslands.pl -stop=makeBed -buildDir=`pwd` -dbHost=$dbHost \\
    -smallClusterHub=$smallClusterHub -bigClusterHub=$bigClusterHub -workhorse=$workhorse \\
    -maskedSeq=$buildDir/trackData/addMask/\$asmId.masked.2bit \\
    -chromSizes=$buildDir/\$asmId.chrom.sizes \$asmId
  doCpgIslands.pl -continue=cleanup -stop=cleanup -buildDir=`pwd` \\
    -dbHost=$dbHost \\
    -smallClusterHub=$smallClusterHub -bigClusterHub=$bigClusterHub -workhorse=$workhorse \\
    -maskedSeq=$buildDir/trackData/addMask/\$asmId.masked.2bit \\
    -chromSizes=$buildDir/\$asmId.chrom.sizes \$asmId
else
  printf "# cpgIslands masked previously completed\\n" 1>&2
  exit 0
fi
_EOF_
  );
  $bossScript->execute();
} # sub doCpgIslands

#########################################################################
# * step: ncbiGene [workhorse]
sub doNcbiGene {
  my $gffFile = "$assemblySource/${asmId}_genomic.gff.gz";
  if ( ! -s "${gffFile}" ) {
    &HgAutomate::verbose(1, "# step ncbiGene: no gff file found at:\n#  $gffFile\n");
    return;
  }
  if ($ucscNames) {
    if ( ! -s "$buildDir/sequence/$asmId.ncbiToUcsc.lift" ) {
      &HgAutomate::verbose(1, "# ERROR: ncbiGene: can not find $buildDir/sequence/$asmId.ncbiToUcsc.lift\n");
      exit 255;
    }
  }
  my $runDir = "$buildDir/trackData/ncbiGene";
  if (-d "${runDir}" ) {
     if (! -s "$runDir/$defaultName.ncbiGene.bb") {
       &HgAutomate::verbose(1,
       "WARNING ncbiGene step may already be running, but not completed ?\n");
       return;
     } elsif (! needsUpdate("$gffFile", "$runDir/$defaultName.ncbiGene.bb")) {
       &HgAutomate::verbose(1, "# ncbiGene step previously completed\n");
       return;
     }
  }
  if (! -s "$buildDir/$defaultName.faSize.txt") {
    &HgAutomate::verbose(1, "# step ncbiGene: can not find faSize.txt at:\n#  $buildDir/$defaultName.faSize.txt\n");
    exit 255;
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "translate NCBI GFF3 gene definitions into a track";
  my $bossScript = newBash HgRemoteScript("$runDir/doNcbiGene.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $dupList = "";
  if ( -s "${buildDir}/download/${asmId}.remove.dups.list" ) {
    $dupList = " | (grep -v -f \"${buildDir}/download/${asmId}.remove.dups.list\"  || true)";
  }

  $bossScript->add(<<_EOF_
export asmId=$defaultName
export gffFile=$gffFile
export DS=`date "+%F"`

function cleanUp() {
  rm -f \$asmId.ncbiGene.genePred.gz \$asmId.ncbiGene.genePred
  rm -f \$asmId.geneAttrs.ncbi.txt
}

if [ \$gffFile -nt \$asmId.ncbiGene.bb ]; then
  ln -s \$gffFile ./
  (gff3ToGenePred -warnAndContinue -useName \\
    -attrsOut=\$asmId.geneAttrs.ncbi.txt \$gffFile stdout \\
      2>> \$asmId.ncbiGene.log.txt || true) | genePredFilter \\
         -chromSizes=../../\$asmId.chrom.sizes stdin stdout \\
        $dupList | gzip -c > \$asmId.ncbiGene.genePred.gz
  genePredCheck \$asmId.ncbiGene.genePred.gz
  zcat \$asmId.ncbiGene.genePred.gz > ncbiGene.\$DS
  genePredToGtf -utr file ncbiGene.\$DS  stdout | gzip -c > \$asmId.ncbiGene.gtf.gz
  rm -f ncbiGene.\$DS
  export howMany=`genePredCheck \$asmId.ncbiGene.genePred.gz 2>&1 | grep "^checked" | awk '{print \$2}'`
  if [ "\${howMany}" -eq 0 ]; then
     printf "# ncbiGene: no gene definitions found in \$gffFile\n";
     cleanUp
     exit 0
  fi
  export ncbiGenePred="\$asmId.ncbiGene.genePred.gz"
_EOF_
  );
  if ($ucscNames) {
    $bossScript->add(<<_EOF_
  liftUp -extGenePred -type=.gp stdout \\
    ../../sequence/$asmId.ncbiToUcsc.lift warn \\
      \$asmId.ncbiGene.genePred.gz | gzip -c \\
        > \$asmId.ncbiGene.ucsc.genePred.gz
  ncbiGenePred="\$asmId.ncbiGene.ucsc.genePred.gz"
_EOF_
      );
  }
  $bossScript->add(<<_EOF_
  genePredToBed -tab -fillSpace \$ncbiGenePred stdout \\
    | bedToExons stdin stdout | bedSingleCover.pl stdin > \$asmId.exons.bed
  export baseCount=`awk '{sum+=\$3-\$2}END{printf "%d", sum}' \$asmId.exons.bed`
  export asmSizeNoGaps=`grep sequences ../../\$asmId.faSize.txt | awk '{print \$5}'`
  export perCent=`echo \$baseCount \$asmSizeNoGaps | awk '{printf "%.3f", 100.0*\$1/\$2}'`
  rm -f \$asmId.exons.bed
  ~/kent/src/hg/utils/automation/gpToIx.pl \$ncbiGenePred \\
    | sort -u > \$asmId.ncbiGene.ix.txt
  if [ -s \$asmId.ncbiGene.ix.txt ]; then
    ixIxx \$asmId.ncbiGene.ix.txt \$asmId.ncbiGene.ix \$asmId.ncbiGene.ixx
  fi
  rm -f \$asmId.ncbiGene.ix.txt
  genePredToBigGenePred \$ncbiGenePred stdout \\
      | sort -k1,1 -k2,2n > \$asmId.ncbiGene.bed
  (bedToBigBed -type=bed12+8 -tab -as=\$HOME/kent/src/hg/lib/bigGenePred.as \\
      -extraIndex=name \$asmId.ncbiGene.bed \\
        ../../\$asmId.chrom.sizes \$asmId.ncbiGene.bb || true)
  if [ ! -s "\$asmId.ncbiGene.bb" ]; then
    printf "# ncbiGene: failing bedToBigBed\\n" 1>&2
    exit 255
  fi
  touch -r\$gffFile \$asmId.ncbiGene.bb
  bigBedInfo \$asmId.ncbiGene.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$asmId.ncbiGene.stats.txt
  LC_NUMERIC=en_US /usr/bin/printf "# ncbiGene %s %'d %s %'d\\n" `cat \$asmId.ncbiGene.stats.txt` | xargs echo
  printf "%d bases of %d (%s%%) in intersection\\n" "\$baseCount" "\$asmSizeNoGaps" "\$perCent" > fb.\$asmId.ncbiGene.txt
else
  printf "# ncbiGene step previously completed\\n" 1>&2
fi
_EOF_
  );
  $bossScript->execute();
} # doNcbiGene

#########################################################################
# * step: ncbiRefSeq [workhorse]
sub doNcbiRefSeq {
  # skip this procedure if all the required files are not available
  my $gffFile = "$assemblySource/${asmId}_genomic.gff.gz";
  if ( ! -s "${gffFile}" ) {
    printf STDERR "# step ncbiRefSeq no gff file found at:\n#  %s\n", $gffFile;
    return;
  }
  my $filesFound = 0;
 my @requiredFiles = qw( genomic.gff.gz rna.fna.gz rna.gbff.gz protein.faa.gz );
  my $filesExpected = scalar(@requiredFiles);
  foreach my $expectFile (@requiredFiles) {
    if ( -s "$assemblySource/${asmId}_${expectFile}" ) {
      ++$filesFound;
    } else {
      printf STDERR "# step ncbiRefSeq missing required file $assemblySource/${asmId}_${expectFile}\n";
    }
  }

  if ($filesFound < $filesExpected) {
    printf STDERR "# step ncbiRefSeq does not have all files required\n";
    return;
  }

  my $runDir = "$buildDir/trackData/ncbiRefSeq";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "run NCBI RefSeq gene procedures";
  my $bossScript = newBash HgRemoteScript("$runDir/doNcbiRefSeq.bash",
                    $workhorse, $runDir, $whatItDoes);

  my $liftSpec = "";
  if ($ucscNames) {
    $liftSpec="-liftFile=\"\$buildDir/sequence/$asmId.ncbiToUcsc.lift\"";
  }
  $bossScript->add(<<_EOF_
export asmId="$defaultName"
export buildDir="$buildDir"
export liftSpec="$liftSpec"
export target2bit="\$buildDir/\$asmId.2bit"

if [ $buildDir/\$asmId.2bit -nt \$asmId.ncbiRefSeq.bb ]; then

~/kent/src/hg/utils/automation/doNcbiRefSeq.pl -toGpWarnOnly -buildDir=`pwd` \\
      -assemblyHub -bigClusterHub=$bigClusterHub -dbHost=$dbHost $liftSpec \\
      -target2bit="\$target2bit" \\
      -stop=load -fileServer=$fileServer -smallClusterHub=$smallClusterHub -workhorse=$workhorse \\
      $asmId \$asmId
else
  printf "# ncbiRefSeq step previously completed\\n" 1>&2
fi
_EOF_
  );
  $bossScript->execute();
} # ncbiRefSeq

#########################################################################
# * step: augustus [workhorse]
sub doAugustus {
  if ($noAugustus) {
  &HgAutomate::verbose(1, "# -noAugustus == Augustus gene track not created\n");
	return;
  }
  my $runDir = "$buildDir/trackData/augustus";
  if (! -s "$buildDir/$defaultName.2bit") {
    &HgAutomate::verbose(1,
	"ERROR: augustus step can not find $buildDir/$defaultName.2bit\n");
    exit 255;
  }
  if (-d "${runDir}" ) {
     if (! -s "$runDir/$defaultName.augustus.bb") {
       &HgAutomate::verbose(1,
       "WARNING augustus step may already be running, but not completed ?\n");
       return;
     } elsif (! needsUpdate("$buildDir/$defaultName.2bit", "$runDir/$defaultName.augustus.bb")) {
       &HgAutomate::verbose(1, "# augustus step previously completed\n");
       return;
     }
  }

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "run Augustus gene prediction procedures";
  my $bossScript = newBash HgRemoteScript("$runDir/doAugustus.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ $buildDir/\$asmId.2bit -nt \$asmId.augustus.bb ]; then
  time (~/kent/src/hg/utils/automation/doAugustus.pl -ram=6g -stop=makeGp -buildDir=`pwd` -dbHost=$dbHost \\
    -bigClusterHub=$bigClusterHub -species=$augustusSpecies -workhorse=$workhorse \\
    -noDbGenePredCheck -maskedSeq=$buildDir/\$asmId.2bit \$asmId) > makeDb.log 2>&1
  time (~/kent/src/hg/utils/automation/doAugustus.pl -continue=cleanup -stop=cleanup -buildDir=`pwd` -dbHost=$dbHost \\
    -bigClusterHub=$bigClusterHub -species=$augustusSpecies -workhorse=$workhorse \\
    -noDbGenePredCheck -maskedSeq=$buildDir/\$asmId.2bit \$asmId) > cleanup.log 2>&1
else
  printf "# augustus genes step previously completed\\n" 1>&2
fi
_EOF_
  );
  $bossScript->execute();
} # doAugustus

#########################################################################
# * step: xenoRefGene [bigClusterHub]
sub doXenoRefGene {
  if ($noXenoRefSeq) {
	&HgAutomate::verbose(1, "# -noXenoRefSeq == Xeno RefSeq gene track not created\n");
	return;
  }
  my $runDir = "$buildDir/trackData/xenoRefGene";

  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "run xeno RefSeq gene mapping procedures";
  my $bossScript = newBash HgRemoteScript("$runDir/doXenoRefGene.bash",
                    $workhorse, $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
export asmId=$defaultName

if [ $buildDir/\$asmId.2bit -nt \$asmId.xenoRefGene.bb ]; then
  time (~/kent/src/hg/utils/automation/doXenoRefGene.pl -buildDir=`pwd` -dbHost=$dbHost \\
    -bigClusterHub=$bigClusterHub -mrnas=$xenoRefSeq -workhorse=$workhorse \\
    -maskedSeq=$buildDir/trackData/addMask/\$asmId.masked.2bit \$asmId) > do.log 2>&1
  if [ -s "\$asmId.xenoRefGene.bb" ]; then
  bigBedInfo \$asmId.xenoRefGene.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$asmId.xenoRefGene.stats.txt
  LC_NUMERIC=en_US /usr/bin/printf "# xenoRefGene %s %'d %s %'d\\n" `cat \$asmId.xenoRefGene.stats.txt` | xargs echo
  fi
else
  printf "# xenoRefGene step previously completed\\n" 1>&2
fi
_EOF_
  );
  $bossScript->execute();
} # doXenoRefGene

#########################################################################
# * step: trackDb [workhorse]
sub doTrackDb {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "construct asmId.trackDb.txt file";
  my $bossScript = newBash HgRemoteScript("$runDir/doTrackDb.bash",
                    $workhorse, $runDir, $whatItDoes);

  if (! -s "${buildDir}/trackData/chromAlias/${defaultName}.chromAlias.txt" ) {
    die "ERROR: can not find ${defaultName}.chromAlias.txt in\n# ${buildDir}/trackData/chromAlias/\n";
  }

  $bossScript->add(<<_EOF_
export defaultName=$defaultName
export asmId=$asmId
export buildDir=$buildDir

rm -f \$defaultName.chromAlias.txt
ln -s trackData/chromAlias/\${defaultName}.chromAlias.txt .
if [ -s trackData/chromAlias/\${defaultName}.chromAlias.bb ]; then
  rm -f \${defaultName}.chromAlias.bb
  ln -s trackData/chromAlias/\${defaultName}.chromAlias.bb .
fi
\$HOME/kent/src/hg/utils/automation/asmHubTrackDb.sh \$defaultName \$asmId \$buildDir \\
   > \$defaultName.trackDb.txt

_EOF_
  );
  $bossScript->execute();
} # trackDb

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "clean up or compresses intermediate files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doCleanup.bash", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
printf "to be done\\n" 1>&2
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
if (scalar(@ARGV) != 1) {
  printf STDERR "ERROR: can not find 1 argument in ARGV, instead: %d\n", scalar(@ARGV);
  for (my $i = 0; $i < scalar(@ARGV); ++$i) {
    printf "# ARGV[%d] : '%s'\n", $i, $ARGV[$i];
  }
}
&usage(1) if (scalar(@ARGV) != 1);
$secondsStart = `date "+%s"`;
chomp $secondsStart;

# expected command line arguments after options are processed
($asmId) = @ARGV;
# yes, there can be more than two fields separated by _
# but in this case, we only care about the first two:
# GC[AF]_123456789.3_assembly_Name
#   0         1         2      3 ....
my @partNames = split('_', $asmId);
$ftpDir = sprintf("%s/%s/%s/%s/%s", $partNames[0],
   substr($partNames[1],0,3), substr($partNames[1],3,3),
   substr($partNames[1],6,3), $asmId);

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/asmHubs/refseqBuild/$ftpDir";

$sourceDir = $opt_sourceDir ? $opt_sourceDir : $sourceDir;
$assemblySource = $opt_sourceDir ? "$opt_sourceDir" : "$sourceDir/$ftpDir";
$asmReport = "$assemblySource/${asmId}_assembly_report.txt";

$species = $opt_species ? $opt_species : $species;

if (length($species) < 1) {
  if (-s "$asmReport") {
     $species = `grep -i "organism name:" $asmReport`;
     chomp $species;
     $species =~ s/.*organism\s+name:\s+//i;
     $species =~ s/\s+\(.*//;
  } else {
     die "ERROR: no -species specified and can not find $asmReport";
  }
  if (length($species) < 1) {
     die "ERROR: no -species specified and can not find Organism name: in $asmReport";
  }
}

$dbName = $opt_dbName ? $opt_dbName : $dbName;
$rmskSpecies = $opt_rmskSpecies ? $opt_rmskSpecies : $species;
$runRepeatModeler = $opt_runRepeatModeler ? $opt_runRepeatModeler : $runRepeatModeler;
$augustusSpecies = $opt_augustusSpecies ? $opt_augustusSpecies : $augustusSpecies;
$xenoRefSeq = $opt_xenoRefSeq ? $opt_xenoRefSeq : $xenoRefSeq;
$ucscNames = $opt_ucscNames ? 1 : $ucscNames;   # '1' == 'TRUE'
$noAugustus = $opt_noAugustus ? 1 : $noAugustus;   # '1' == 'TRUE'
$noXenoRefSeq = $opt_noXenoRefSeq ? 1 : $noXenoRefSeq;   # '1' == 'TRUE'
$workhorse = $opt_workhorse ? $opt_workhorse : $workhorse;
$bigClusterHub = $opt_bigClusterHub ? $opt_bigClusterHub : $bigClusterHub;
$smallClusterHub = $opt_smallClusterHub ? $opt_smallClusterHub : $smallClusterHub;
$fileServer = $opt_fileServer ? $opt_fileServer : $fileServer;
$ncbiRmsk = $opt_ncbiRmsk ? 1 : 0;
$noRmsk = $opt_noRmsk ? 1 : 0;
$defaultName = $asmId;
$defaultName = $dbName if (length($dbName));


die "can not find assembly source directory\n$assemblySource" if ( ! -d $assemblySource);
printf STDERR "# buildDir: %s\n", $buildDir;
printf STDERR "# sourceDir %s\n", $sourceDir;
if (length($dbName)) {
printf STDERR "# dbName %s - building in /hive/data/genomes/%s\n", $dbName, $dbName;
}
printf STDERR "# augustusSpecies %s\n", $augustusSpecies;
printf STDERR "# xenoRefSeq %s\n", $xenoRefSeq;
printf STDERR "# assemblySource: %s\n", $assemblySource;
printf STDERR "# runRepeatModeler %s\n", $runRepeatModeler ? "TRUE" : "FALSE";
printf STDERR "# rmskSpecies %s\n", $rmskSpecies;
printf STDERR "# augustusSpecies %s\n", $augustusSpecies;
printf STDERR "# ncbiRmsk %s\n", $ncbiRmsk ? "TRUE" : "FALSE";
printf STDERR "# ucscNames %s\n", $ucscNames ? "TRUE" : "FALSE";
printf STDERR "# noAugustus %s\n", $noAugustus ? "TRUE" : "FALSE";
printf STDERR "# noXenoRefSeq %s\n", $noXenoRefSeq ? "TRUE" : "FALSE";
printf STDERR "# noRmsk %s\n", $noRmsk ? "TRUE" : "FALSE";

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

