#!/usr/bin/perl
# File: createNcbiCtgAgp
# Author: Terry Furey
# Date: 10/2001

# Create contig .agp files for the SCBI assembly based on
# the original contig .agp and whether the contig was
# flipped in the final assembly, and place them in the 
# correct location
 
# Usage message
if ($#ARGV != 2) {
  print stderr "USAGE: createNcbiCtgAgp <seq_contig.md> <allcontig.agp.buildXX> <dir>\n";
  exit(1);
}

# Read in and check arguments 
$ctgfile = shift(@ARGV);
$agpfile = shift(@ARGV);
$dir = shift(@ARGV);
open(CTG, "<$ctgfile") || die("Could not open $ctgfile\n");
open (AGP, "<$agpfile") || die("Could not open $agpfile\n");
if (! -e "$dir") { die("$dir does not exist\n"); }
 
# Determine which need to be reversed and what chrom each contig is in
# Then extract agp and reverse if necessary
while ($line = <CTG>) {
  chomp($line);
  ($build, $thischr, $start, $end, $orien, $thisctg, $id, $type, $weight) = split("\t",$line);

  # Skip non-contig related lines
  next if ($thisctg eq "start");
  next if ($thisctg eq "end");

  # Get contig and chrom in right format
  ($ctg, $vers) = split(/\./, $thisctg);
  ($chr, $other) = split(/\|/, $thischr);
 
  # Extract original agp info for his contig
  print stderr "Creating $chr/$ctg.agp\n";
  $agp = `grep $thisctg $agpfile`;
  @agp = split("\n", $agp);

  # Reverse complement agp info, if necessary
  if ($orien eq "-") {
    @agpline = split(" ", $agp[$#agp]);
    $size = $agpline[2];
    $num = $agpline[3];
    for ($i = 0; $i <= $#agp; $i++) {
      @agpline = split(" ", $agp[$i]);
      $agpline[0] = "$chr/$ctg";
      if ($agpline[8] eq "+") {
        $agpline[8] = "-";
      } elsif ($agpline[8] eq "-") {
        $agpline[8] = "+";
      }
      $start = $agpline[1];
      $agpline[1] = $size - $agpline[2] + 1;
      $agpline[2] = $size - $start + 1;
      $agpline[3] = $num - $agpline[3] + 1;
      $newagp[$#agp-$i] = join("\t",@agpline);
    }
    for ($i = 0; $i <=$#agp; $i++) {
      $agp[$i] = $newagp[$i];
    }

  # Else, just copy contig info
  } else {
    for ($i = 0; $i <=$#agp; $i++) {
      @agpline = split(" ", $agp[$i]);
      $agpline[0] = "$chr/$ctg";
      $agp[$i] = join("\t",@agpline);
    }
  }

  # Write out new agp to file
  if (!-e "$dir/$chr") {
     `mkdir $dir/$chr`;
  }
  if (!-e "$dir/$chr/$ctg") {
     `mkdir $dir/$chr/$ctg`;
  }
  open(OUT, ">$dir/$chr/$ctg/$ctg.agp") || die("Could not open $dir/$chr/$ctg/$ctg.agp");
  for ($i = 0; $i <= $#agp; $i++) {
    print OUT "$agp[$i]\n";
  }
  close(OUT);
  # Copy file to a gold file
  `cp $dir/$chr/$ctg/$ctg.agp $dir/$chr/$ctg/gold.102`;
}
close(CTG);
