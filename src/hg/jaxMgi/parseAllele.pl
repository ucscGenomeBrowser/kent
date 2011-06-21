#!/usr/bin/perl -w
use strict;
# Parse this particular flavor of GFF3 into GFF1 (stdout) with allele source 
# appended to the name (for later parsing back out in to bed+ field).  
# Also, *append* to an info file and a fixit SQL file.  
# Add uniquifying suffix to transcript names when necessary.  
# Rely on the fact that an mRNA line always immediately 
# precedes the exon lines.  

my $sourceSuffixes = "Induced|Other|Spontaneous|Targeted|Transgenic";
my %gffSource = ( "AL_IND" => "Induced", "AL_OTHER" => "Other", "AL_SPON" => "Spontaneous",
                  "AL_TARG" => "Targeted", "AL_TRANS" => "Transgenic",
                  "MGI_DNA_GTRAP" => "GeneTrappedDna", "MGI_RNA_GTRAP" => "GeneTrappedRna", );

my $alias = "jaxAlleleInfo.tab";
my $fixit = "fixJaxAllele.sql";
open(OUT, ">>$alias") || die "Cant open $alias for appending: $!\n";
open(SQL, ">>$fixit") || die "Cant open $fixit for appending: $!\n";
my (%txNameIndx, $tweakedName);
while (<>) {
  chomp;
  s/\s*$//;
  my ($chr, $source, $type, $start, $end, undef, $strand, undef, $info) = split("\t");
  die "Unrecognized GFF source $source" if (! defined $gffSource{$source});
  $chr =~ s/MT/M/;  $chr =~ s/Chr/chr/ || $chr =~ s/^([^c])/chr$1/;
  $source = $gffSource{$source};
  $info =~ s/Lexicon Genetics/LG/;  $info =~ s/BayGenomics/BG/;
  if ($info =~ /Note "(\d)-RACE"/) {
    $source .= $1;
  }
  if ($type eq "mRNA" || $type eq "genetrap_DNA") {
    my ($name, $alName, $mgiID);
    $info =~ s/^([^;]+)_($sourceSuffixes);/$1;/;
    if ($info =~ /^mRNA ([\w.&\/ -]+(\<[\w\(\). ,\*\/+-]+(\>|m1Nt))?); Dbxref "(MGI:\d+)";/) {
      ($name, $mgiID) = ($1, $4);
      $name =~ s/ /_/g;
      if ($name =~ /\<.*[^\>]$/) {
        print STDERR "Missing > for mRNA name $name\n";
        $name = $name . ">";
      }
    } elsif ($info =~ /^Genetrap_DNA ([\w.&\/ -]+); Dbxref "(MGI:\d+)"/) {
      ($name, $mgiID) = ($1, $2);
      $name =~ s/ /_/g;
      print "$chr\tMGI\texon\t$start\t$end\t.\t$strand\t.\t$name\|\|$source\n";
    } else {
      die "parse, mRNA/Genetrap_DNA line $.:\n$info\n";
    }
    if (defined $txNameIndx{$name}) {
      $tweakedName = $name . "_" . $txNameIndx{$name};
      print SQL "update jaxAllele set name = \"$name\" " .
                "where name = \"$tweakedName\";\n";
    } else {
      undef $tweakedName;
      $source =~ s/GeneTrapped([DR])na/Gene trapped $1NA/;
      $source =~ s/([35])$/ ($1'-RACE)/;
      if ($source =~ /GeneTrapped([DR])na([35]?)/) {
        $source = "Gene";
      }
      print OUT "$name\t$mgiID\t$source\n";
    }
    $txNameIndx{$name}++;
  } elsif ($type =~ /^exon$/) {
    my ($name);
    $info =~ s/_($sourceSuffixes)$//;
    if ($info =~ /^mRNA ([\w.&\/ -]+(\<[\w\(\). ,\*\/+-]+(\>|m1Nt))?)$/) {
      $name = $1;
      $name =~ s/ /_/g;
      if ($name =~ /\<.*[^\>]$/) {
        $name = $name . ">";
      }
    } elsif ($info =~ /^mRNA ([\w.&\/ -]+);/) {
      $name = $1;
      $name =~ s/ /_/g;
    } else {
      die "parse, exon line $.:\n$info\n";
    }
    if (defined $tweakedName) {
      my $escName = $name;
      $escName =~ s/\(/\\(/g;  $escName =~ s/\)/\\)/g;
      if ($tweakedName !~ /^${escName}_\d+$/) {
        die "tweakedName $tweakedName does not start with name $name and " .
            " have a numeric suffix like expected";
      }
      $name = $tweakedName;
    }
    print "$chr\tMGI\t$type\t$start\t$end\t.\t$strand\t.\t$name\|\|$source\n";
  } elsif ($type ne "gene") {
    die "unrecognized type $type, line $.";
  }
}
close(OUT);
close(SQL);
