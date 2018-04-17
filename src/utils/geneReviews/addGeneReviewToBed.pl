#!/usr/bin/perl
use warnings;
use strict;
sub usage() {
    print "usage: ./addGRtoBed.pl dbName > outputFile\n";
}
my $argc=scalar(@ARGV);
if ($argc != 1) {
    usage; die "ERROR: Please supply a database name for results.\n";
}
#get the list of (unique) of gene symbols form geneReviews table
my @geneReviews = split('\n',
 `hgsql -N -e "select chrom, chromStart, chromEnd, name from geneReviews;" $ARGV[0]`);
my $grBed;
my $clickMsg = " (Click links below to search GeneReviews or GeneTests)";
my $firstTime = 1;
my $field;
my $details;
foreach $grBed(@geneReviews) {
   $details = "";
   my @col = split(/\t/, $grBed);
   #print "Processing name: ", $col[3], " <BR>";
   my @grShort = split('\n',
    `hgsql -N -e 'select  grShort, diseaseID, diseaseName from geneReviewsRefGene where geneSymbol="$col[3]"' $ARGV[0]`);
   $firstTime = 1;
   my $count = scalar(@grShort);
   my $i;
   my $j;
   for ($i=0; $i < $count; $i++) {
       my @f5 = split(/\t/, $grShort[$i]);
   if ($firstTime == 1) {
      $firstTime=0;
      $details = "";
      $details = "<BR><B>GeneReview available for " . $col[3] .  ": </B> " . $clickMsg . "<BR>";
      $details .= "<PRE><TT>";
      $details .= "Short name    Disease ID     GeneTests disease name<BR>";
      $details .= "-----------------------------------------------------------";
      $details .= "-----------------------------------------------------------";
      $details .= "----------------------------------<BR>";
      }
      $details .= "<A HREF=\"https://www.ncbi.nlm.nih.gov/books/n/gene/" . $f5[0] . "\" TARGET=_blank><B>" . $f5[0] . "</B></A>";
      if (length($f5[0]) <= 15) {
        for ($j = 0; $j <  15-length($f5[0]); $j ++ )
           {
              $details .= " ";
           }
         }
       $details .=  $f5[1] . "       ";
       $details .= "<A HREF=\"https://www.ncbi.nlm.nih.gov/sites/GeneTests/review/disease/" . $f5[2] . "?db=genetests&search_param==begins_with\" TARGET=_blank>" . $f5[2]  ."<BR>";
     }
       $details .= "</TT></PRE><BR>";
       print $col[0], "\t", $col[1], "\t", $col[2], "\t", $col[3], "\t", $details, "\n";
  }
