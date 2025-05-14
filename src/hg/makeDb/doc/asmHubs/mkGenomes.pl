#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my %commonName;	# key is asmId, value is common name
my @monthNumber = qw( Zero Jan. Feb. Mar. Apr. May Jun. Jul. Aug. Sep. Oct. Nov. Dec. );

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "mkGenomes.pl blatHost blatPort [two column name list] > .../hub/genomes.txt\n";
  printf STDERR "e.g.: mkGenomes.pl dynablat-01 4040 vgp.primary.assemblies.tsv > .../vgp/genomes.txt\n";
  printf STDERR "e.g.: mkGenomes.pl hgwdev 4040 vgp.primary.assemblies.tsv > .../vgp/download.genomes.txt\n";
  printf STDERR "the name list is found in \$HOME/kent/src/hg/makeDb/doc/asmHubs/\n";
  printf STDERR "\nthe two columns are 1: asmId (accessionId_assemblyName)\n";
  printf STDERR "column 2: common name for species, columns separated by tab\n";
  printf STDERR "result will write a local asmId.genomes.txt file for each hub\n";
  printf STDERR "and a local asmId.hub.txt file for each hub\n";
  printf STDERR "and a local asmId.groups.txt file for each hub\n";
  printf STDERR "and the output to stdout will be the overall genomes.txt\n";
  printf STDERR "index file for all genomes in the given list\n";
  exit 255;
}

my $downloadHost = "hgwdev";
my @blatHosts = qw( dynablat-01 dynablat-01 );
my @blatPorts = qw( 4040 4040 );
my $blatHostDomain = ".soe.ucsc.edu";
my $groupsTxt = `cat ~/kent/src/hg/makeDb/doc/asmHubs/groups.txt`;

################### writing out hub.txt file, twice ##########################
sub singleFileHub($$$$$$$$$$$$$$) {
  my ($fh1, $fh2, $accessionId, $orgName, $descr, $asmId, $asmDate, $defPos, $taxId, $trackDb, $accessionDir, $buildDir, $chromAuthority, $hugeGenome) = @_;
  my @fhN;
  push @fhN, $fh1;
  push @fhN, $fh2;


  my %liftOverChain;	# key is 'otherDb' name, value is bbi path
  my %liftOverGz;	# key is 'otherDb' name, value is lift.over.gz file path
  my $hasChainNets = `ls -d $buildDir/trackData/lastz.* 2> /dev/null | wc -l`;
  chomp $hasChainNets;
  if ($hasChainNets) {
    printf STDERR "# hasChainNets: %d\t%s\n", $hasChainNets, $asmId;
    open (my $CN, ">>", "hasChainNets.txt") or die "can not write to hasChainNets.txt";
    printf $CN "%s\t%d\n", $asmId, $hasChainNets;
    open (CH, "ls -d $buildDir/trackData/lastz.*|") or die "can not ls -d $buildDir/trackData/lastz.*";
    while (my $line = <CH>) {
      chomp $line;
      my $otherDb = basename($line);
      $otherDb =~ s/lastz.//;
      my $OtherDb = ucfirst($otherDb);
      my $bbiPath = "$buildDir/bbi/${asmId}.chainLiftOver${OtherDb}.bb";
      if (-s "${bbiPath}") {
        $liftOverChain{$otherDb} = "bbi/${asmId}.chainLiftOver${OtherDb}.bb";
      }
     my $loPath = "$buildDir/liftOver/${accessionId}To${OtherDb}.over.chain.gz";
      if (-s "${loPath}") {
        printf $CN "\t%s\n", "${accessionId}To${OtherDb}.over.chain.gz";
    $liftOverGz{$otherDb} = "liftOver/${accessionId}To${OtherDb}.over.chain.gz";
      }
    }
    close (CH);
    close ($CN);
  }
  my $fileCount = 0;
  my @tdbLines;
  open (TD, "<$trackDb") or die "can not read trackDb: $trackDb";
  while (my $tdbLine = <TD>) {
     chomp $tdbLine;
     push @tdbLines, $tdbLine;
  }
  close (TD);
  my $assemblyName = $asmId;
  $assemblyName =~ s/${accessionId}_//;
  foreach my $fh (@fhN) {
    printf $fh "hub %s genome assembly\n", $accessionId;
    printf $fh "shortLabel %s\n", $orgName;
    printf $fh "longLabel %s/%s/%s genome assembly\n", $orgName, $descr, $asmId;
    printf $fh "useOneFile on\n";
    printf $fh "email hclawson\@ucsc.edu\n";
    printf $fh "descriptionUrl html/%s.description.html\n", $asmId;
    printf $fh "\n";
    printf $fh "genome %s\n", $accessionId;
    printf $fh "taxId %s\n", $taxId if (length($taxId) > 1);
    printf $fh "groups groups.txt\n";
    printf $fh "description %s\n", $orgName;
    printf $fh "twoBitPath %s.2bit\n", $accessionId;
    printf $fh "twoBitBptUrl %s.2bit.bpt\n", $accessionId;
    printf $fh "chromSizes %s.chrom.sizes.txt\n", $accessionId;
    if ( -s "${buildDir}/${asmId}.chromAlias.bb" ) {
      printf $fh "chromAliasBb %s.chromAlias.bb\n", $accessionId;
    } else {
      printf $fh "chromAlias %s.chromAlias.txt\n", $accessionId;
    }
    if ($chromAuthority =~ m/^chromAuthority/) {
       printf $fh "%s\n", $chromAuthority;
    }
    printf $fh "organism %s %s\n", $assemblyName, $asmDate;
    printf $fh "defaultPos %s\n", $defPos;
    printf $fh "scientificName %s\n", $descr;
    printf $fh "htmlPath html/%s.description.html\n", $asmId;
    # until blat server host is ready for hgdownload, avoid these lines
    if ($blatHosts[$fileCount] ne $downloadHost) {
      printf $fh "blat %s%s %s dynamic $accessionDir/$accessionId\n", $blatHosts[$fileCount], $blatHostDomain, $blatPorts[$fileCount]+$hugeGenome;
      printf $fh "transBlat %s%s %s dynamic $accessionDir/$accessionId\n", $blatHosts[$fileCount], $blatHostDomain, $blatPorts[$fileCount]+$hugeGenome;
      printf $fh "isPcr %s%s %s dynamic $accessionDir/$accessionId\n", $blatHosts[$fileCount], $blatHostDomain, $blatPorts[$fileCount]+$hugeGenome;
    }
    foreach my $otherDb (sort keys %liftOverGz) {
       printf $fh "liftOver.%s %s\n", $otherDb, $liftOverGz{$otherDb};
    }
    printf $fh "\n";
    foreach my $tdbLine (@tdbLines) {
      printf $fh "%s\n", $tdbLine;
    }
    ++$fileCount;
  }
}	#	sub singleFileHub($$$$$$$$$$$$$$)

##############################################################################
my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";

my $blatHost = shift;
my $blatPort = shift;
my $inputList = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyCount = 0;

open (FH, "<${orderList}") or die "can not read ${orderList}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($asmId, $commonName) = split('\t', $line);
  if (!defined($commonName)) {
    printf STDERR "ERROR: missing tab sep common name:\n'%s'\n", $line;
    exit 255;
  }
  if (defined($commonName{$asmId})) {
    printf STDERR "ERROR: duplicate asmId: '%s'\n", $asmId;
    printf STDERR "previous name: '%s'\n", $commonName{$asmId};
    printf STDERR "duplicate name: '%s'\n", $commonName;
    exit 255;
  }
  $commonName{$asmId} = $commonName;
  push @orderList, $asmId;
  printf STDERR "orderList[$assemblyCount] = $asmId\n";
  ++$assemblyCount;
}
close (FH);

my $buildDone = 0;
my $orderKey = 0;
foreach my $asmId (@orderList) {
  ++$orderKey;
  next if ($asmId !~ m/^GC/);
  my $chromAuthority = "";
  $chromAuthority = `~/kent/src/hg/makeDb/doc/asmHubs/chromAuthority.pl $asmId 2> /dev/null`;
  chomp $chromAuthority;
  my ($gcPrefix, $accession, undef) = split('_', $asmId);
  my $accessionId = sprintf("%s_%s", $gcPrefix, $accession);
  my $accessionDir = substr($asmId, 0 ,3);
  $accessionDir .= "/" . substr($asmId, 4 ,3);
  $accessionDir .= "/" . substr($asmId, 7 ,3);
  $accessionDir .= "/" . substr($asmId, 10 ,3);
  my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
  my $destDir = "/hive/data/genomes/asmHubs/$accessionDir/$accessionId";
  if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
  }
  if ( ! -s "${buildDir}/${asmId}.chrom.sizes" ) {
    printf STDERR "# ERROR: missing ${asmId}.chrom.sizes in\n# ${buildDir}\n";
    next;
  }
  if ( ! -s "${buildDir}/${asmId}.chromAlias.txt" ) {
    printf STDERR "# ERROR: missing ${asmId}.chromAlias.txt in\n# ${buildDir}\n";
    next;
  }
  if ( ! -s "${buildDir}/${asmId}.chromAlias.bb" ) {
    printf STDERR "# ERROR: missing ${asmId}.chromAlias.bb in\n# ${buildDir}\n";
    next;
  }
  my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
  my $trackDb = "$buildDir/$asmId.trackDb.txt";
  if ( ! -s "${trackDb}" ) {
    printf STDERR "# %03d not built yet: %s\n", $orderKey, $asmId;
    printf STDERR "# '%s'\n", $trackDb;
    next;
  }
  if ( ! -s "${asmReport}" ) {
    printf STDERR "# %03d missing assembly_report: %s\n", $orderKey, $asmId;
    next;
  }
  ++$buildDone;
printf STDERR "# %03d genomes.txt %s/%s %s\n", $buildDone, $accessionDir, $accessionId, ($chromAuthority =~ m/^chromAuthority/) ?  "chromAuthority ucsc" : "no authority";
  my $taxId=`grep -i "taxid:" $asmReport | head -1 | awk '{printf \$(NF)}' | tr -d \$'\\r'`;
  chomp $taxId;
  my $descr=`grep -i "organism name:" $asmReport | head -1 | tr -d \$'\\r' | sed -e 's#.*organism name: *##i; s# (.*\$##;'`;
  chomp $descr;
  my $orgName=`grep -i "organism name:" $asmReport | head -1 | tr -d \$'\\r' | sed -e 's#.* name: .* (##; s#).*##;'`;
  chomp $orgName;
  my $asmDate=`grep -i "Date:" $asmReport | head -1 | tr -d \$'\\r'`;
  chomp $asmDate;
  $asmDate =~ s/.*Date:\s+//;
  my ($year, $month, $day) = split('-', $asmDate);
  if (defined($month)) {
    $asmDate = sprintf("%s %s", $monthNumber[$month], $year);
  } else {
    printf STDERR "# error: can not find month in $asmDate in $asmReport\n";
  }

  if (defined($commonName{$asmId})) {
     $orgName = $commonName{$asmId};
  }
  my $assemblyName = $asmId;
  $assemblyName =~ s/${accessionId}_//;

  printf "genome %s\n", $accessionId;
  printf "taxId %s\n", $taxId if (length($taxId) > 1);
  printf "trackDb ../%s/%s/trackDb.txt\n", $accessionDir, $accessionId;
  printf "groups groups.txt\n";
  printf "description %s\n", $orgName;
  printf "twoBitPath ../%s/%s/%s.2bit\n", $accessionDir, $accessionId, $accessionId;
  printf "twoBitBptUrl ../%s/%s/%s.2bit.bpt\n", $accessionDir, $accessionId, $accessionId;
  printf "chromSizes ../%s/%s/%s.chrom.sizes.txt\n", $accessionDir, $accessionId, $accessionId;

  # wait until code gets out for v429 release before using chromAlias.bb
  # for the chromInfoPage display of hgTracks
  if ( -s "${buildDir}/${asmId}.chromAlias.bb" ) {
    printf "chromAliasBb ../%s/%s/%s.chromAlias.bb\n", $accessionDir, $accessionId, $accessionId;
  } else {
    printf "chromAlias ../%s/%s/%s.chromAlias.txt\n", $accessionDir, $accessionId, $accessionId;
  }
  if ($chromAuthority =~ m/^chromAuthority/) {
     printf "%s\n", $chromAuthority;
  }
  printf "organism %s %s\n", $assemblyName, $asmDate;
  my $hugeGenome = 0;
  my $fourGb = 2**32 - 1;
  my $asmSize=`ave -col=2 $buildDir/$asmId.chrom.sizes | grep -w total | awk '{printf "%d", \$NF}'`;
  chomp $asmSize;
  $hugeGenome = 1 if ($asmSize > $fourGb);
  my $chrName=`head -1 $buildDir/$asmId.chrom.sizes | awk '{print \$1}'`;
  chomp $chrName;
  my $bigChrom=`head -1 $buildDir/$asmId.chrom.sizes | awk '{print \$NF}'`;
  chomp $bigChrom;
  my $oneThird = int($bigChrom/3);
  my $tenK = $oneThird + 10000;
  $tenK = $bigChrom if ($tenK > $bigChrom);
  my $defPos="${chrName}:${oneThird}-${tenK}";
  if ( -s "$buildDir/defaultPos.txt" ) {
    $defPos=`cat "$buildDir/defaultPos.txt"`;
    chomp $defPos;
  }
  printf "defaultPos %s\n", $defPos;
  printf "orderKey %d\n", $buildDone;
  printf "scientificName %s\n", $descr;
  printf "htmlPath ../%s/%s/html/%s.description.html\n", $accessionDir, $accessionId, $asmId;
  # until blat server host is ready for hgdownload, avoid these lines
  if ($blatHost ne $downloadHost) {
    if ( -s "${destDir}/$accessionId.trans.gfidx" ) {
      printf "blat $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
    printf "transBlat $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
      printf "isPcr $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
    } else {
      printf STDERR "# missing ${destDir}/$accessionId.trans.gfidx\n";
    }
  }
  printf "\n";

  # the original multi-file system:
  my $localHubTxt = "$buildDir/${asmId}.hub.txt";
  open (HT, ">$localHubTxt") or die "can not write to $localHubTxt";
  printf HT "hub %s genome assembly\n", $accessionId;
  printf HT "shortLabel %s\n", $orgName;
  printf HT "longLabel %s/%s/%s genome assembly\n", $orgName, $descr, $asmId;
  printf HT "genomesFile genomes.txt\n";
  printf HT "email hclawson\@ucsc.edu\n";
  printf HT "descriptionUrl html/%s.description.html\n", $asmId;
  close (HT);

  # try creating single file hub.txt, one for hgwdev, one for hgdownload
  my $downloadHubTxt = "$buildDir/${asmId}.download.hub.txt";
  open (DL, ">$downloadHubTxt") or die "can not write to $downloadHubTxt";
  $localHubTxt = "$buildDir/${asmId}.singleFile.hub.txt";
  open (HT, ">$localHubTxt") or die "can not write to $localHubTxt";

  singleFileHub(\*HT, \*DL, $accessionId, $orgName, $descr, $asmId, $asmDate,
	$defPos, $taxId, $trackDb, $accessionDir, $buildDir, $chromAuthority,
           $hugeGenome);

  my $localGenomesFile = "$buildDir/${asmId}.genomes.txt";
  open (GF, ">$localGenomesFile") or die "can not write to $localGenomesFile";
  printf GF "genome %s\n", $accessionId;
  printf GF "taxId %s\n", $taxId if (length($taxId) > 1);
  printf GF "trackDb trackDb.txt\n";
  printf GF "groups groups.txt\n";
  printf GF "description %s\n", $orgName;
  printf GF "twoBitPath %s.2bit\n", $accessionId;
  printf GF "twoBitBptUrl %s.2bit.bpt\n", $accessionId;
  printf GF "chromSizes %s.chrom.sizes.txt\n", $accessionId;
  if ( -s "${buildDir}/${asmId}.chromAlias.bb" ) {
    printf GF "chromAliasBb %s.chromAlias.bb\n", $accessionId;
  } else {
    printf GF "chromAlias %s.chromAlias.txt\n", $accessionId;
  }
  if ($chromAuthority =~ m/^chromAuthority/) {
     printf GF "%s\n", $chromAuthority;
  }
  printf GF "organism %s %s\n", $assemblyName, $asmDate;
  printf GF "defaultPos %s\n", $defPos;
  printf GF "scientificName %s\n", $descr;
  printf GF "htmlPath html/%s.description.html\n", $asmId;
  # until blat server host is ready for hgdownload, avoid these lines
  if ($blatHost ne $downloadHost) {
    if ( -s "${destDir}/$accessionId.trans.gfidx" ) {
      printf GF "blat $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
      printf GF "transBlat $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
     printf GF "isPcr $blatHost$blatHostDomain %d dynamic $accessionDir/$accessionId\n", $blatPort + $hugeGenome;
    }
  }
  close (GF);

  my $localGroups = "$buildDir/${asmId}.groups.txt";
  open (GR, ">$localGroups") or die "can not write to $localGroups";
  printf GR "%s", $groupsTxt;
  close (GR);
}

__END__

description Mastacembelus armatus
twoBitPath GCA_900324485.2_fMasArm1.2/trackData/addMask/GCA_900324485.2_fMasArm1.2.masked.2bit
organism Zig-Zag eel
defaultPos LR535842.1:14552035-14572034
orderKey 1
scientificName Mastacembelus armatus
htmlPath GCA_900324485.2_fMasArm1.2/html/GCA_900324485.2_fMasArm1.2.description.html

# head -25 GCA_002180035.3_HG00514_prelim_3.0_assembly_report.txt

# Assembly name:  HG00514_prelim_3.0
# Organism name:  Homo sapiens (human)
# Isolate:  HG00514
# Sex:  female
# Taxid:          9606
# BioSample:      SAMN04229552
# BioProject:     PRJNA300843
# Submitter:      The Genome Institute at Washington University School of Medicine
# Date:           2018-05-22
# Assembly type:  haploid
# Release type:   major
# Assembly level: Chromosome
# Genome representation: full
# WGS project:    NIOH01
# Assembly method: Falcon v. November 2016
# Expected final version: no
# Genome coverage: 80.0x
# Sequencing technology: PacBio RSII
# GenBank assembly accession: GCA_002180035.3
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_002180045.3              Primary Assembly
