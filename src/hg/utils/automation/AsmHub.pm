# AsmHub: common routines for building assembly hubs
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/AsmHub.pm instead.

package AsmHub;

use warnings;
use strict;
use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

# This is a listing of the public methods and variables (which should be
# treated as constants) exported by this module:
@EXPORT_OK = (
    # Support for common command line options:
    qw( commify asmSize
      ),
);

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters, input is a large number with no commas:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

# given an asmId.chrom.sizes, return the assembly size from the
# sum of column 2:
sub asmSize($) {
    my ($chromSizes) = @_;
    my $asmSize=`ave -col=2 $chromSizes | grep "total" | sed -e 's/total //; s/.000000//;'`;
    chomp $asmSize;
    return $asmSize;
}

# given a fully qualified asmId, e.g.: GCA_018504075.1_HG02723.alt.pat.f1_v2
# return the string representating the path: GCA/018/504/075
sub asmIdToPath($) {
  my ($asmId) = @_;
  my $gcX = substr($asmId, 0, 3);
  my $d0 = substr($asmId, 4, 3);
  my $d1 = substr($asmId, 7, 3);
  my $d2 = substr($asmId, 10, 3);
  my $ret = sprintf("%s/%s/%s/%s", $gcX, $d0, $d1, $d2);
  return $ret;
}
