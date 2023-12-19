#!/usr/bin/env perl

use strict;
use warnings;

printf STDERR "# # # ! ! ! Obsolete program, please instead use:\n";
printf STDERR "  aliasTabToText.pl asmId.chromAlias.tab > asmId.chromAlias.txt\n";

exit 255;
__END__
my $argc = scalar(@ARGV);

if ($argc !=1 ) {
  printf STDERR "usage: chromAliasToTxt.pl ucscDb > ucscDb.chromAlias.txt\n";
  printf STDERR "expecting to find chromAlias table in ucscDb\n";
  exit 255;
}

my %chrAliases;	# key is chrom name, value is hash pointer to
                # key is alias name, value is count of times seen

my $ucscDb = shift;
open (FH, "hgsql -N -e 'select * from chromAlias;' $ucscDb|") or die "can not select from $ucscDb.chromAlias";
while (my $line = <FH>) {
  chomp $line;
  my ($alias, $chrom, $source) = split('\s+', $line);
  if (! defined($chrAliases{$chrom})) {
     my %h;
     $chrAliases{$chrom} = \%h;
  }
  my $aliases = $chrAliases{$chrom};
  $aliases->{$alias} += 1;
}
close (FH);

printf "# sequenceName\talias names\tUCSC database: $ucscDb\n";
foreach my $chr (sort keys %chrAliases) {
  my $aliases = $chrAliases{$chr};
  printf "%s", $chr;
  foreach my $alias (sort keys %$aliases) {
    printf "\t%s", $alias;
  }
  printf "\n";
}
