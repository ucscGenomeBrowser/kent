#!/usr/bin/perl -w
# Parse this particular flavor of GFF3 into GFF1 (stdout) 
# plus *append* to an association file (alias.tab) and write a fixit SQL file.

use strict;

# Keep track of transcript names; our GFF-parsing code requires unique 
# transcript names but non-unique ones are used here.  Add uniquifying 
# suffix.  Rely on the fact that a REP_transcript line always immediately 
# precedes the REP_exon lines.  
my %txNameIndx;
my $tweakedName;

my $alias = "jaxPhenotypeAlias.tab";
my $fixit = "fixJaxPhenotype.sql";
open(OUT, ">>$alias") || die "Can't open $alias for appending: $!\n";
open(SQL, ">>$fixit") || die "Can't open $fixit for appending: $!\n";
while (<>) {
  chomp;
  s/\s*$//;
  my ($chr, undef, $type, $start, $end, undef, $strand, undef, $info) =
    split("\t");
  if ($type eq "mRNA") {
    my ($name, $mgiID);
    if ($info =~ /^mRNA ([\w.-]+)_MP_\d+; Dbxref "(MGI:\d+)";/) {
      ($name, $mgiID) = ($1, $2);
    } else {
      die "parse, line $.:\n$info\n";
    }
    if (defined $txNameIndx{$name}) {
      $tweakedName = $name . '_' . $txNameIndx{$name};
      print SQL "update jaxPhenotype set name = '$name' " .
                "where name = '$tweakedName';\n";
    } else {
      undef $tweakedName;
      print OUT "$name\t$mgiID\n";
    }
    $txNameIndx{$name}++;
  } elsif ($type eq "exon") {
    $type = "exon";
    my $name;
    if ($info =~ /^mRNA ([\w.-]+)_MP_\d+$/) {
      $name = $1;
    } else {
      die "parse, line $.:\n$info\n";
    }
    if (defined $tweakedName) {
      if ($tweakedName !~ /^${name}_\d+$/) {
        die "tweakedName $tweakedName does not start with name $name and " .
            " have a numeric suffix like expected";
      }
      $name = $tweakedName;
    }
    $chr =~ s/Chr/chr/;
    print "$chr\tMGI\t$type\t$start\t$end\t.\t$strand\t.\t$name\n";
  } elsif ($type ne "gene") {
    die "unrecognized type $type, line $.";
  }
}
close(OUT);
close(SQL);
