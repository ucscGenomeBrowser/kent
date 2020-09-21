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

my %aliasOut;	# key is alias name, value is sequence name in this assembly

sub showAlias() {
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

sub addVariations($$) {
  my ($alias, $sequence) = @_;
  addAlias($alias, $sequence);
  return;
  addAlias(lc($alias), $sequence);
  my $noDotName = $alias;
  $noDotName =~ s/\..*//;
  addAlias($noDotName, $sequence);
  addAlias(lc($noDotName), $sequence);
  return;
}

my $asmId = shift;

my $twoBit = "../../$asmId.2bit";

my %sequenceNames;	# key is sequence name, value is sequence size

open (FH, "twoBitInfo $twoBit stdout|") or die "can not twoBitInfo $twoBit stdout";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $size) = split('\s+', $line);
  $sequenceNames{$name} = $size;
  addVariations($name, $name);
}

close (FH);

# foreach my $sequence (sort keys %sequenceNames) {
#   printf "# %s\t%d\n", $sequence, $sequenceNames{$sequence};
# }

my %chrNames;	# key is sequence name, value is 'chr' UCSC chromosome name
open (FH, "cat ../../sequence/*.names|") or die "can not cat ../../sequence/*.names";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $name) = split('\s+', $line);
  $chrNames{$name} = $chr;
  addVariations($chr, $name);
}
close (FH);

# foreach my $sequence (sort keys %chrNames) {
#   printf "%s\t%s\n",  $chrNames{$sequence}, $sequence;
# }

my %chr2acc;	# key is sequence name, value is NCBI chromosome name
open (FH, "grep -h -v '^#' ../../download/${asmId}_assembly_structure/*/*/chr2acc|") or die "can not gep chr2acc files";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $name) = split('\s+', $line);
  $chr2acc{$name} = $chr;
  addVariations($chr, $name);
}
close (FH);

# foreach my $sequence (sort keys %chr2acc) {
#   printf "%s\t%s\n",  $chr2acc{$sequence}, $sequence;
# }

my %gbk2acc;	# key is refseq name, value is genbank accession
open (FH, "grep -v '^#' ../../download/${asmId}_assembly_report.txt | cut -d\$'\t' -f5,7|") or die "can not grep assembly_report";
while (my $line = <FH>) {
  chomp $line;
  my ($gbk, $name) = split('\s+', $line);
  next if ($name eq "na");	# may not be any RefSeq names
  $gbk2acc{$name} = $gbk;
  addVariations($gbk, $name);
}
close (FH);

showAlias();
exit 0;

foreach my $sequence (sort keys %gbk2acc) {
  printf "%s\t%s\n",  $gbk2acc{$sequence}, $sequence;
}

my %seq2gbk;	# key is genbank name, value is sequence name
open (FH, "grep -v '^#' ../../download/${asmId}_assembly_report.txt | cut -d\$'\t' -f1,5|") or die "can not grep assembly_report";
while (my $line = <FH>) {
  chomp $line;
  my ($seq, $name) = split('\s+', $line);
  next if ($name eq "na");	# may not be any genbank name
  $seq2gbk{$name} = $seq;
}
close (FH);

foreach my $sequence (sort keys %seq2gbk) {
  printf "%s\t%s\n",  $seq2gbk{$sequence}, $sequence;
}

__END__

export asmId="GCF_000001735.4_TAIR10.1"

grep -v "^#" ../../download/${asmId}_assembly_report.txt \
  | cut -d$'\t' -f1,5

grep -v "^#" ../../download/${asmId}_assembly_report.txt \
  | cut -d$'\t' -f1,7

grep -v "^#" ../../download/${asmId}_assembly_report.txt \
  | cut -d$'\t' -f5,7

2       assembled-molecule      2       Chromosome      CP002685.1      =      NC_003071.7      Primary Assembly        19698289        na
