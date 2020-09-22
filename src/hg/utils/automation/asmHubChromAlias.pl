#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: asmHubChromAlias.pl asmId > asmId.chromAlias.tab\n\n";
  printf STDERR "where asmId is something like: GCF_000001735.4_TAIR10.1\n";
  printf STDERR "Outputs a tab file suitable for processing with ixIxx to\n";
  printf STDERR "create an index file to use in an assembly hub.\n";
  printf STDERR "This command assumes it is in a work directory in the\n";
  printf STDERR "assembly hub: .../asmId/trackData/chromAlias/\n";
  printf STDERR "and .../asmId/downloads/ and .../asmId/sequence/ have\n";
  printf STDERR "been created and processed for the hub build.\n";
  exit 255;
}

my %ucscToRefSeq;	# key is UCSC sequence name, value is RefSeq name
my %ucscToGenbank;	# key is UCSC sequence name, value is GenBank name
my $ucscNames = 0;	# == 1 if sequence is UCSC names, == 0 if NCBI names

my %aliasOut;	# key is alias name, value is sequence name in this assembly

sub showAlias() {
printf "# sequenceName\talias names\n";
  my %chromIndex;	# key is sequence name in assembly, value
                        # is a tab separated list of aliases
  foreach my $alias (sort keys %aliasOut) {
    my $name = $aliasOut{$alias};
#    printf "%s\t%s\n", $alias, $aliasOut{$alias};
    if (defined($chromIndex{$name})) {
      $chromIndex{$name} .= "\t" . $alias;
    } else {
      $chromIndex{$name} = $alias;
    }
  }
  foreach my $name (sort keys %chromIndex) {
    printf "%s\t%s\n", $name, $chromIndex{$name};
  }
}

#  given an alias and a sequence name, add to result or verify identical
#  to previous add
sub addAlias($$) {
  my ($alias, $sequence) = @_;
  # do not need to add the sequence name itself
  return if ($alias eq $sequence);
  # already done, verify it is equivalent to previous request
  if (defined($aliasOut{$alias})) {
     if ($sequence ne $aliasOut{$alias}) {
        printf STDERR "ERROR: additional alias '%s:%s' does not match previous '%s'\n", $alias, $sequence, $aliasOut{$alias};
        exit 255;
     }
     return;
  }
  $aliasOut{$alias} = $sequence;
  return;
}

##############################################################################
########## experimental addVariations, not in use at this time ###############
##############################################################################
sub addVariations($$) {
  my ($alias, $sequence) = @_;
  addAlias($alias, $sequence);
  return;

  addAlias(lc($alias), $sequence);      # add lower case equivalent
  my $noDotName = $alias;
  $noDotName =~ s/\..*//;
  addAlias($noDotName, $sequence);      # add noDot name
  addAlias(lc($noDotName), $sequence);  # and lower case noDot nae
  return;
}

my $asmId = shift;

my $refSeq = 0; #       == 0 for Genbank assembly, == 1 for RefSeq assembly
$refSeq = 1 if ($asmId =~ m/^GCF/);

my $twoBit = "../../$asmId.2bit";
my $sequenceCount = 0;

my %sequenceSizes;	# key is sequence name, value is sequence size

open (FH, "twoBitInfo $twoBit stdout|") or die "can not twoBitInfo $twoBit stdout";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $size) = split('\s+', $line);
  $sequenceSizes{$name} = $size;
  ++$sequenceCount;
}

close (FH);

my $nameCount = 0;
my %ncbiNames;	# key is NCBI sequence name, value is 'chr' UCSC chromosome name
my %ucscNames;	# key is 'chr' UCSC name, value is NCBI sequence name
open (FH, "cat ../../sequence/*.names|") or die "can not cat ../../sequence/*.names";
while (my $line = <FH>) {
  chomp $line;
  my ($ucscName, $seqName) = split('\s+', $line);
  $ncbiNames{$seqName} = $ucscName;
  $ucscNames{$ucscName} = $seqName;
  ++$nameCount;
  $ucscNames = 1 if (defined($sequenceSizes{$ucscName}));
  if ($refSeq) {
    $ucscToRefSeq{$ucscName} = $seqName;
  } else {
    $ucscToGenbank{$ucscName} = $seqName;
  }
}
close (FH);

if ($sequenceCount != $nameCount) {
  printf STDERR "ERROR: do not find the same name count in sequence vs. names files\n";
  printf STDERR "sequenceCount %d != %d names count\n", $sequenceCount, $nameCount;
  exit 255;
}

printf STDERR "# initial set with UCSC names\n";
### first set of names is the UCSC to NCBI sequence names
foreach my $sequence (sort keys %sequenceSizes) {
  my $seqName = $sequence;
  my $alias = $sequence;
#   addVariations($seqName, $alias);      # obsolete identical usage
  if ($ucscNames) {
     $alias = $ucscNames{$seqName};
  } else {
     $alias = $ncbiNames{$seqName};
  } 
  if (!defined($alias)) {
    printf STDERR "ERROR: can not find alias name for '%s' sequence\n", $seqName;
    exit 255;
  }
  addVariations($alias, $seqName);
}

# foreach my $sequence (sort keys %chrNames) {
#   printf "%s\t%s\n",  $chrNames{$sequence}, $sequence;
# }

### next set of names are the equivalents declared by NCBI
### if they exist
my %chr2acc;	# key is sequence name, value is NCBI chromosome name
my $asmStructCount = `ls  ../../download/${asmId}_assembly_structure/*/*/chr2acc 2> /dev/null | wc -l`;
chomp $asmStructCount;
if ( $asmStructCount > 0 ) {
  printf STDERR "# second name set processing chr2acc files\n";
  open (FH, "grep -h -v '^#' ../../download/${asmId}_assembly_structure/*/*/chr2acc|") or die "can not grep chr2acc files";
  while (my $line = <FH>) {
    chomp $line;
    my ($alias, $seqName) = split('\s+', $line);
    $chr2acc{$seqName} = $alias;
    if ($ucscNames) {
       $seqName = $ncbiNames{$seqName};
    }
    addVariations($alias, $seqName);
  }
  close (FH);
}

# foreach my $sequence (sort keys %chr2acc) {
#   printf "%s\t%s\n",  $chr2acc{$sequence}, $sequence;
# }

my %gbk2acc;	# key is refseq name, value is genbank accession
  printf STDERR "# third set processing assembly_report\n";
# column 5 is the GenBank-Accn, column 7 is the RefSeq-Accn
open (FH, "grep -v '^#' ../../download/${asmId}_assembly_report.txt | cut -d\$'\t' -f5,7|") or die "can not grep assembly_report";
while (my $line = <FH>) {
  chomp $line;
  my ($gbkName, $refSeqName) = split('\s+', $line);
  next if ($refSeqName eq "na");	# may not be any RefSeq names
  $gbk2acc{$gbkName} = $refSeqName;
  if ($refSeq) {
    my $seqName = $refSeqName;
    if ($ucscNames) {
       $seqName = $ncbiNames{$seqName};
    }
    addVariations($gbkName, $seqName);
  } else {
    my $seqName = $gbkName;
    if ($ucscNames) {
       $seqName = $ncbiNames{$seqName};
    }
    addVariations($refSeqName, $seqName);
  }
}
close (FH);

showAlias();
