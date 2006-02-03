#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/findAccession.pl instead.

# $Id: findAccession.pl,v 1.1 2006/02/03 22:49:47 hiram Exp $

# File: findAccession
# Author: Terry Furey
# Date: 5/17/01
# Project: Human
# Description:  Finds the accession for a marker on the golden path

# Usage message
if ($#ARGV < 1) {
  print stderr "USAGE: findAccession [-agp -ncbi -mouse -rat -chicken -all] <file> <accession_info>\n";
  exit(1);
}

# Read parameters
$agp = 0;
$ncbi = 0;
$mouse = 0;
$rat = 0;
$chicken = 0;
$all = 0;
@chroms = qw/1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y M NA UL 22_h2_hap1 6_cox_hap1 5_h2_hap1 6_qbl_hap2/;

while ($#ARGV > 1) {
  $arg = shift(@ARGV);
  if ($arg eq "-agp") {
    $agp = 1;
 } elsif ($arg eq "-ncbi") {
    $ncbi = 1;
    @chroms = (1..22,X,Y,M,Un);
  } elsif ($arg eq "-mouse") {
    $mouse = 1;
    @chroms = (1..19,X,Y,Un,M);
  } elsif ($arg eq "-rat") {
    $rat = 1;
    @chroms = (1..20,X,Y,Un);
  } elsif ($arg eq '-chicken') {
    $chicken = 1;
    @chroms = (1..24,26..28,32,'E22C19W28','E26C13','E50C23','W','Z','Un');
  } elsif ($arg eq "-all") {
    $all = 1;
  } else {
    print stderr "$arg is not a valid parameter\n";
  }
}
$file = shift(@ARGV);
$info = shift(@ARGV);
open(FILE, "<$file") || die("Could not open $file");
open(OUT, ">$file.acc");

# Get chromosomes from agp directory, if possible
if (($agp) && (-e "$info/chrom_names")) {
    print STDERR "reading chrom names from $info/chrom_names\n";
    @chroms = split("\n", `cat $info/chrom_names`);
}

# Read in the positions of the accessions
# Either read from accession_info.rdb file
if (!$agp) {
  print stderr "Reading accession_info from $info file\n";
  if (! -e $info) { die("$info does not exist\n"); }
  `sorttbl Chr -r Ord Start < $info > $$.sort`;
  open(INFO, "<$$.sort") || die("Count not open $$.sort\n");
  $line = <INFO>;
  $line = <INFO>;
  $index = 0;
  while ($line = <INFO>) {
    chomp($line);
    ($chr, $ord, $start, $end, $mid, $ctg, $ctgIdx, $bargeIdx, $clone, $acc, @rest) = split("\t",$line);
    $acc[$index] = $acc;
    if ($ord == 1) {
      $chr[$index] = $chr;
    } else {
      $chr[$index] = "${chr}_random";
    }
    $start[$index] = $start;
    $end[$index] = $end;
    $index++;
  }
  close(INFO);
# Or read directly from agp file
} else {
  print stderr "Reading accession_info from agp files\n";
  $index = -1;
  foreach $chr (@chroms) {
    print stderr "Loading chr$chr agp info\n";
    for ($ord = 0; $ord <=1; $ord++) {
      if ($ord == 0) {
	$file = "chr${chr}_random.agp";
      } else {
	$file = "chr$chr.agp";
      }
      if ( ! -e "$info/$chr/$file" ) { next; }
      print STDERR "reading agp file $info/$chr/$file\n";
      if (open(AGP, "<$info/$chr/$file")) {
	$prevacc = "";
	while ($line = <AGP>) {
	  chomp($line);
	  ($thischr, $start, $end, $idx, $type, $acc, @rest) = split(' ',$line);
	  next if ($type eq "N");

	  if (! $chicken) {
	      ($acc, $vers) = split(/\./,$acc);
	  }
	  if ($acc ne $prevacc) {
	    $index++;
	    $acc[$index] = $acc;
	    if ($ord == 1) {
	      $chr[$index] = "chr$chr";
	    } else {
	      $chr[$index] = "chr${chr}_random";
	    }
	    $start[$index] = $start;
	    $end[$index] = $end;
	  } else {
	    $end[$index] = $end;
	  }
	  $prevacc = $acc;
	}
	close(AGP);
      } else {
	print stderr ("Could not open $info/$chr/$file\n");
      }
    }
  }
  $index++;
}

# Process each line in the position file
$i = 0;
$count = 0;
print stderr "Starting processing\n";
while($line = <FILE>) {
  chomp($line);
  ($chr, $start, $end, @rest) = split(' ',$line);
  $count++;
  if ($count%10000 == 0) {
    print stderr "$count\n";
  }

  # Make sure at current chromosome
  if ($chr ne $chr[$i]) {
    $i = 0;
    print stderr "Processing $chr\n";
    while ($chr[$i] ne $chr) {
      $i++;
      if ($i > $#chr) {
	die("Can't find chrom $chr in \@chr (\$i=$i)\n");
      }
    }
    $firsti = $i;
    $largest = $end[$i];
    $largesti = $i;
  }

  # Find a range that overlaps the placement
  $acc = "";
  $i = $largesti;
  $reset = 0;
  # If want all accessions, make sure start with first accession that overlaps
  if ($all) {
    while (($end[$i] > $start) && ($i > $firsti)) {
      $i--;
    }
    while (($start[$i] <= $end) && ($chr[$i] eq $chr)) {
      if (($start[$i] <= $end) && ($end[$i] >= $start)) {
	if ($acc) {
	  $acc .= ",$acc[$i]";
	} else {
	  $acc = $acc[$i];
	}
      }
      $i++;
    }
  # If want only best accession that overlaps
  } else {
    while (!$acc) {
      # Check if completely contained
      if (($start >= $start[$i]) && ($end <= $end[$i])) {
	# Check if this is a very long accession and is contained in next accession
	if (($end[$i] - $start[$i]) >= 500000) {
	  $diffi = $end[$i] - $start[$i];
	  $j = $i + 1;
	  while (($start <= $end[$j]) && ($chr[$j] eq $chr)) {
	    if (($start >= $start[$j]) && ($end <= $end[$j]) && (($end[$j] - $start[$j]) < $diffi)) {
	      $i = $j;
	      $diffi = $end[$j] - $start[$j];
	      if ($end[$j] > $largest) {
		$largest = $end[$j];
		$largesti = $j;
	      }
	    }
	    $j++;
	  }
	}
	$acc = $acc[$i];
	if ($chr[$i] ne $chr) {
	  die("Doesn't work - $chr $start $end\n");
	}
      # Check if out of range for chromosome
      } elsif (($start >= $largest)) {
	$i++;
	if ($end[$i] > $largest) {
	  $largest = $end[$i];
	  $largesti = $i;
	}
	if ($chr ne $chr[$i]) {
	  die("1 - Out of chrom - $chr $start $end - $chr[$i] $start[$i] $end[$i] $i\n");
	}
      # Check if accession really does contain feature
      } elsif ($start[$i] > $end)  {
	$i--;
	if ($i < $largesti) {
	  if ($reset) {
	    die("Doesn't line up - $chr $start $end - $chr[$i] $start[$i] $end[$i] $i\n");
	  }
	  $reset = 1;
	  $i = $firsti;
	  $largest = $end[$i];
	  $largesti = $i;	
	}
	if ($chr ne $chr[$i]) {
	  die("2 - Out of chrom - $chr $start $end - $chr[$i] $start[$i] $end[$i] $i\n");
	}

      # If feature rrosses the border of more than one accession - find the largest overlap
      } else {
	while (($end[$i] > $start) && ($chr eq $chr[$i]) && ($i >= 0)) {
	  $i--;
	} 
	$i++;
	# Find largest overlap
	$overlap = 0;
	$acc = "";
	while (($start[$i] < $end) && ($chr eq $chr[$i]) && ($i < $index)) {
	  $test = $end - $start + 1;
	  if ($start < $start[$i]) {
	    $test -= ($start[$i] - $start);
	  }
	  if ($end > $end[$i]) {
	    $test -= ($end - $end[$i]);
	  }
	  if ($test > $overlap) {
	    $overlap = $test;
	    $acc = $acc[$i];
	  }
	  $i++;
	}
	if ($chr ne $chr[$i]) {
	  $i--;
	}
      }
    }
  }
  # Print it out
  print OUT "$line\t$acc\n";
}
close(FILE);
close(OUT);
if (!$agp) {
  `rm $$.*`;
}


