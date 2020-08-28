#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: chromAliasBedTest.pl <db> > chromAlias.<db>.bed\n";
  printf STDERR "writes to STDOUT a bed file that has every chrom name\n";
  exit 255;
}
# +----------+------------------+------+-----+---------+-------+
# | Field    | Type             | Null | Key | Default | Extra |
# +----------+------------------+------+-----+---------+-------+
# | chrom    | varchar(255)     | NO   | PRI | NULL    |       |
# | size     | int(10) unsigned | NO   |     | NULL    |       |
# | fileName | varchar(255)     | YES  |     | NULL    |       |
# +----------+------------------+------+-----+---------+-------+

my $db = shift;

my %chromSizes;	# key is chrom value is size
open (FH, "hgsql -N -e 'select * from chromInfo;' $db|") or die "can not hgsql select from chromInfo.$db";
while (my $line = <FH>) {
  chomp $line;
  my ($chrom, $size, $fileName) = split('\s+', $line);
  $chromSizes{$chrom} = $size;
}
close (FH);

# foreach my $chr (sort keys %chromSizes) {
#   printf "%s\t%d\n", $chr, $chromSizes{$chr};
# }

my %chromAlias;	#	key is external name value is UCSC name

open (FH, "hgsql -N -e 'select * from chromAlias;' $db|") or die "can not hgsql select from chromAlias.$db";
while (my $line = <FH>) {
  chomp $line;
  my ($external, $ucsc) = split('\s+', $line);
  $chromAlias{$external} = $ucsc;
}
close (FH);

# output bed item for each external name
my %ucscUsed;	# key is UCSC name, value is csv list of external names
foreach my $external (sort keys %chromAlias) {
  my $ucscName = $chromAlias{$external};
  printf "%s\t0\t%s\t%s\n", $external, $chromSizes{$ucscName}, $external;
  if (defined($ucscUsed{$ucscName})) {
    $ucscUsed{$ucscName} .= "," . $external;
  } else {
    $ucscUsed{$ucscName} = $external;
  }
}

# catch up for those UCSC names not mentioned in chromAlias
foreach my $ucscName (sort keys %chromSizes) {
  if (!defined($ucscUsed{$ucscName})) {
    printf "%s\t0\t%s\tnoAlias\n", $ucscName, $chromSizes{$ucscName};
  } else {
    if (length($ucscUsed{$ucscName}) > 254) {
      my $limit254 = substr($ucscUsed{$ucscName},0,254);
      printf "%s\t0\t%s\t%s\n", $ucscName, $chromSizes{$ucscName}, $limit254;
    } else {
      printf "%s\t0\t%s\t%s\n", $ucscName, $chromSizes{$ucscName}, $ucscUsed{$ucscName};
    }
  }
}

