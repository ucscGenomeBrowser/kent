#!/usr/bin/perl -w
# Parse this particular flavor of GFF3 into GFF1 (stdout) 
# plus an association file (alias.tab) and a fixit SQL file (fixit.sql).  

use strict;

# Keep track of transcript names; our GFF-parsing code requires unique 
# transcript names but non-unique ones are used here.  Add uniquifying 
# suffix.  Rely on the fact that a REP_transcript line always immediately 
# precedes the REP_exon lines.  
my %txNameIndx;
my $tweakedName;

my $alias = "jaxRepTranscriptAlias.tab";
my $fixit = "fixJaxRepTranscript.sql";
open(OUT, ">$alias") || die "Cant open $alias for writing: $!\n";
open(SQL, ">$fixit") || die "Cant open $fixit for writing: $!\n";
while (<>) {
  chomp;
  s/\s*$//;
  my ($chr, undef, $type, $start, $end, undef, $strand, undef, $info) =
    split("\t");
  if ($type eq "mRNA") {
    my ($name1, $latter) = split(/_MGI:/, $info);
    $name1 =~ s/^mRNA //
      || die "line parse, $type line $.:\n$info\n";
    $latter =~ m/^(\d+)_([\w.\(\)\/-]+);/
      || die "latter parse, $type line $.:\n$latter\n";
    my ($mgiID, $name2) = ($1, $2);
    my $name = $name1 . "_" . $name2;
    if (defined $txNameIndx{$name}) {
      $tweakedName = $name . "_" . $txNameIndx{$name};
      print SQL "update jaxRepTranscript set name = \"$name\" " .
                "where name = \"$tweakedName\";\n";
    } else {
      undef $tweakedName;
      print OUT "$name\tMGI:$mgiID\n";
    }
    $txNameIndx{$name}++;
  } elsif ($type eq "exon" || $type eq "CDS") {
    $type = "exon" if ($type eq "CDS"); # They set cdsStart=txStart, cdsEnd=txEnd! even for N.C.
    my ($name1, $latter) = split(/_MGI:/, $info);
    $name1 =~ s/^mRNA //
      || die "line parse, $type line $.:\n$info\n";
    $latter =~ m/^(\d+)_([\w.\(\)\/-]+)$/
      || die "latter parse, $type line $.:\n$latter\n";
    my ($mgiID, $name2) = ($1, $2);
    my $name = $name1 . "_" . $name2;
    if (defined $tweakedName) {
      my $quotedName = $name;  $quotedName =~ s/\(/\\(/g;  $quotedName =~ s/\)/\\)/g;
      if ($tweakedName !~ /^${quotedName}_\d+$/) {
        die "tweakedName $tweakedName does not match /^${quotedName}_\\d+\$/ like expected";
      }
      $name = $tweakedName;
    }
    $chr =~ s/MT/M/;
    if (/^Chr/) {
      $chr =~ s/^Chr/chr/;
    } elsif (!/^chr/) {
      $chr = "chr$chr";
    }
    print "$chr\tMGI\t$type\t$start\t$end\t.\t$strand\t.\t$name\n";
  } elsif ($type ne "gene") {
    die "unrecognized type $type, line $.";
  }
}
close(OUT);
close(SQL);
