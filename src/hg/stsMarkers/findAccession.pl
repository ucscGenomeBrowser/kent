#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/stsMarkers/findAccession.pl instead.

# $Id: findAccession.pl,v 1.1 2009/05/04 17:29:07 hiram Exp $

# File: findAccession
# Author: Terry Furey
# Date: 5/17/01
# Project: Human
# Description:  Finds the accession for a marker on the golden path

use strict;
use warnings;

# Usage message
if ($#ARGV < 1) {
  print STDERR "USAGE: findAccession [-agp -ncbi -mouse -rat -chicken -all] <file> <accession_info>\n";
  exit(1);
}

# Read parameters
my $agp = 0;
my $ncbi = 0;
my $mouse = 0;
my $rat = 0;
my $chicken = 0;
my $all = 0;
my @chroms = qw/1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y M NA UL 22_h2_hap1 6_cox_hap1 5_h2_hap1 6_qbl_hap2/;

while ($#ARGV > 1) {
  my $arg = shift(@ARGV);
  if ($arg eq "-agp") {
    $agp = 1;
 } elsif ($arg eq "-ncbi") {
    $ncbi = 1;
    @chroms = (1..22,"X","Y","M","Un");
  } elsif ($arg eq "-mouse") {
    $mouse = 1;
    @chroms = (1..19,"X","Y","Un","M");
  } elsif ($arg eq "-rat") {
    $rat = 1;
    @chroms = (1..20,"X","Y","Un");
  } elsif ($arg eq '-chicken') {
    $chicken = 1;
    @chroms = (1..24,26..28,32,'E22C19W28','E26C13','E50C23','W','Z','Un');
  } elsif ($arg eq "-all") {
    $all = 1;
  } else {
    print STDERR "$arg is not a valid parameter\n";
  }
}
my $file = shift(@ARGV);
my $info = shift(@ARGV);
open(FILE, "<$file") || die("Could not open $file");
open(OUT, ">$file.acc");

# Get chromosomes from agp directory, if possible
if (($agp) && (-e "$info/chrom_names")) {
    print STDERR "reading chrom names from $info/chrom_names\n";
    @chroms = split("\n", `cat $info/chrom_names`);
}

my @acc;
my @chr;
my @start;
my @end;
my $index = -1;

# Read in the positions of the accessions
# Either read from accession_info.rdb file
if (!$agp) {
  print STDERR "Reading accession_info from $info file\n";
  if (! -e $info) { die("$info does not exist\n"); }
  `sorttbl Chr -r Ord Start < $info > $$.sort`;
  open(INFO, "<$$.sort") || die("Count not open $$.sort\n");
  my $line = <INFO>;
  $line = <INFO>;
  my $index = 0;
  while ($line = <INFO>) {
    chomp($line);
    my ($chr, $ord, $start, $end, $mid, $ctg, $ctgIdx, $bargeIdx, $clone, $acc, @rest) = split("\t",$line);
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
  print STDERR "Reading accession_info from agp files\n";
  $index = -1;
  my %filesRead;
  foreach my $chr (@chroms) {
    printf STDERR "# processing chr $chr from chroms[]\n";
    my $chrNum = $chr;
    $chrNum =~ s/_.*//;
    my $chrRandom = $chr;
    $chrRandom =~ s/_gl[0-9]+_random//;
    print STDERR "Loading chr$chr agp info ($chrRandom)\n";
# Loading chr16_KI270728v1_random agp info (16_KI270728v1_random)
# reading agpFile /hive/data/genomes/hg38/bed/stsMap/agp/16/chr16_KI270728v1_random.agp
# Loading chr15_KI270905v1_alt agp info (15_KI270905v1_alt)
# reading agpFile /hive/data/genomes/hg38/bed/stsMap/agp/15/chr15_KI270905v1_alt.agp
    for (my $ord = 0; $ord <=1; $ord++) {
      if ($ord == 0 && $chr =~ m/random/) {
	$file = "chr${chrRandom}.agp";
      } elsif ($ord == 0 && $chr =~ m/_alt/) {
	$file = "chr${chrRandom}.agp";
      } else {
	$file = "chr$chr.agp";
      }
      my $agpFile = "$info/$chrNum/$file";
      if ($chr =~ m/_random/) {
          $chrRandom =~ s/_random//;
	  $agpFile = "$info/$chrRandom/$file";
      } elsif ($chr =~ m/_alt/) {
	  $agpFile = "$info/$chrRandom/$file";
      } elsif ($chr =~ m/_hap/) {
	  $agpFile = "$info/$chrRandom/$file";
      } elsif ($chr =~ m/Un/) {
	  $agpFile = "$info/$chr/$file";
      }
      printf STDERR "# reading agpFile $agpFile\n";
      next if (! -e $agpFile);
      next if (exists($filesRead{$agpFile}));
      print STDERR "reading agp file $agpFile\n";
      if (open(AGP, "<$agpFile")) {
	$filesRead{$agpFile} = 1;
	my $prevacc = "";
	while (my $line = <AGP>) {
	  chomp($line);
	  my ($thischr, $start, $end, $idx, $type, $acc, @rest) = split(' ',$line);
	  next if ($type eq "N");

	  if (! $chicken) {
	      my ($acc, $vers) = split(/\./,$acc);
	  }
	  if ($acc ne $prevacc) {
	    $index++;
	    $acc[$index] = $acc;
	    $chr[$index] = "$thischr";
	    $start[$index] = $start;
	    $end[$index] = $end;
	  } else {
	    $end[$index] = $end;
	  }
	  $prevacc = $acc;
	}
	close(AGP);
      } else {
	print STDERR ("Could not open $agpFile\n");
      }
    }
  }
  $index++;
}

printf STDERR "have loaded %d names in chr[]\n", scalar(@chr);
# for (my $i = 0; $i < scalar(@chr); ++$i) {
#    printf STDERR "chr[%d] = '%s' '%s' %d-%d\n", $i, $chr[$i], $acc[$i], $start[$i], $end[$i];
#}

# Process each line in the position file
my $i = 0;
my $count = 0;
my $firsti = 0;
my $largest = 0;
my $largesti = 0;
print STDERR "Starting processing reading $file\n";
while(my $line = <FILE>) {
  chomp($line);
  my ($chr, $start, $end, @rest) = split(' ',$line);
  $count++;
  if ($count%10000 == 0) {
    print STDERR "$count\n";
  }

  # Make sure at current chromosome
  if ($chr ne $chr[$i]) {
    $i = 0;
    print STDERR "Processing $chr\n";
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
  my $acc = "";
  $i = $largesti;
  my $reset = 0;
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
	  my $diffi = $end[$i] - $start[$i];
	  my $j = $i + 1;
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
	my $overlap = 0;
	$acc = "";
	while (($start[$i] < $end) && ($chr eq $chr[$i]) && ($i < $index)) {
	  my $test = $end - $start + 1;
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
