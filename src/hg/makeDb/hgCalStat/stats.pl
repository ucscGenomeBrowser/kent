#!/usr/bin/perl -w

use strict;


if(@ARGV < 1)
{
    print STDERR "Usage: stats.pl [-i] <build dir> 
-i: internal version\n";
    exit(1);
}

my $internal = 0;
my $dir = "/Unspecified";
while(my $arg = shift)
{
    if($arg eq "-i")
    {
	$internal = 1;
    }
    else
    {
	$dir = $arg;
    }
}

opendir(BUILD, "$dir") || die "Can't open build dir: $dir";

my $totalBase = 0;  ## total base include gap and contigs
my $totalGap = 0;  ##total gap size
my $totalTelo = 0; ##total telomere size;
my $totalCent = 0; ##total centromere size;
my $totalHeter = 0; ##total heterochromatin size;
my $totalOther = 0; ## total fragment, clone, contig, hidden  gap size
my $totalShort = 0; ## total short arm size
my $totalRandom = 0; ## total random size.
my $totalHidden = 0; #hidden gap size

# my $gapCount = 0; ## gap number
my $teloCount = 0; ## telomere gap count
my $heterCount = 0; ##heterochromatin gap count
my $otherCount = 0; ## fragment, clone, contig, hidden gap count
my $centCount  = 0;
my $shortCount = 0;
my $hiddenCount = 0; ## hidden gap count

if($internal)
{
    print "Chr\tAssembled Size (including gaps)\tSequenced Size\tTotal Gap Size\tCentromere Gap Size\tTotal Telomeric gap Sizes\tTelomere Gap Count\tShort Arm Gap Size\tTotal HeteroChromatin Gap Sizes\tHeteroChromatin Gap Count\tTotal Other Gap Sizes\tOther Gap Count\tHidden Gap Sizes\tHidden Gap Count\tBases in Unplaced Auxiliary Clones\n";
}
else
{
   # print "Chr\tAssembled Size (including gaps)\tSequenced Size\tTotal Gap Size\tEuchromatic Gap Sizes\tEuchromatic Gap Count\tNon-euchromatic Gap Sizes\tNon-euchromatic Gap Count\tBases in Unplaced Auxiliary Clones\n";
     print "Chr\tAssembled Size (including gaps)\tSequenced Size\tTotal Gap Size\tNon-euchromatic Gap Sizes\tNon-euchromatic Gap Count\tEuchromatic Gap Sizes\tEuchromatic Gap Count\tBases in Unplaced Auxiliary Clones\n";
}
my @chrs = readdir(BUILD);

while( my $chr = shift(@chrs))
{##read chr dir
    
     next if(length($chr) > 2 || $chr eq '.' || $chr eq '..');
     
     ##base count
     my $gap = 0;
     my $cent = 0;
     my $telo = 0;
     my $other = 0;
     my $heter = 0;
     my $short = 0;
     my $base = 0;
     my $baseR = 0;
     my $gapR = 0;
     my $hide = 0;

     ##gap count
     my $centC = 0;
     my $teloC = 0;
     my $otherC = 0;
     my $heterC = 0;
     my $shortC = 0;
     my $hideC = 0;

     ##previous end
     my $centP = 0;
     my $teloP = 0;
     my $otherP = 0;
     my $heterP = 0;
     my $shortP = 0;

     my %begin= ();
     my $noRandom = 0;

     ##for random
     if(!open(AGPR, "<$dir/$chr/chr${chr}_random.agp"))
     {
	 print STDERR "Can't open ${chr}_random.agp .\n";
	 $noRandom = 1;
     }
     else
     {
	 while(my $line = <AGPR>)
	 {
	     chomp($line);
	     my (@eles) = split(/\t/, $line);
	     $baseR = $eles[2];
	     if($eles[4] eq "N")
	     {
		 $gapR = $gapR + $eles[2] - $eles[1] + 1; 
	     }
	 }
     }
     close(AGPR);
     

     #open chr agp
     if(!open(AGP, "<$dir/$chr/chr$chr.agp"))
     {
	 print STDERR "Can't open $chr.agp .\n";
	 if($internal)
	 {
	     if($noRandom == 0)
	     {
		 print "chr$chr\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t",$baseR - $gapR,"\n";
	     }
	     else
	     {
		 print "chr$chr\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\n";
	     }
	 }
	 else
	 {
	      if($noRandom == 0)
	      {
                 print "chr$chr\t-\t-\t-\t-\t-\t-\t-\t",$baseR - $gapR,"\n";
             }
             else
             {
                 print "chr$chr\t-\t-\t-\t-\t-\t-\t-\t-\n";
             }
	  }
	 next;
     }
     else
     {
	 while(my $line = <AGP>)
	 {##read agp
	      chomp($line);
	      my (@eles) = split(/\t/, $line);
	      $base = $eles[2];
	      $begin{$eles[1]} = 1;
	      if($eles[4] eq "N")
	      {
		  if($eles[6] =~ /telo/io)
		  {
		      ## chr? gap size
		      $gap = $gap + $eles[2] - $eles[1] + 1; 
		      ## chr? telo size
		      $telo = $telo + $eles[2] - $eles[1] + 1;
		      
		      ##telo gap count
		      $teloC++ ;
		  }
		  elsif($eles[6] =~ /cent/io)
		  {
		      ## chr? gap size
		      $gap = $gap + $eles[2] - $eles[1] + 1;
		      
		      ## chr? cent size
		      $cent = $cent + $eles[2] - $eles[1] + 1;
		  
		      ##cent count
		      if($centP != $eles[1] -1)
		      {
			  $centC++ ;
			  $centP = $eles[2];
		      }
		  }
		  elsif($eles[6] =~ /heter/io)
		  {
		      ## chr? gap size
		      $gap = $gap + $eles[2] - $eles[1] + 1;
		      
		      ## chr? cent size
		      $heter = $heter + $eles[2] - $eles[1] + 1;
		      
		      ##heter count
		      if($heterP != $eles[1] -1)
		      {
			  $heterC++ ;
			  $heterP = $eles[2];
		      }
		      
		  }
		  elsif($eles[6] =~ /short/io)
		  {
		      ## chr? gap size
		      $gap = $gap + $eles[2] - $eles[1] + 1;
		      
		      ## chr? cent size
		  $short = $short + $eles[2] - $eles[1] + 1;
		      
		      ##frage count
		      $shortC++ ;
		  }
		  elsif($eles[6] =~ /cont/io || $eles[6] =~ /clon/io || $eles[6] =~ /frag/io)
		  {
		      ## chr? gap size
		      $gap = $gap + $eles[2] - $eles[1] + 1;
		      
		      ## chr? cent size
		      $other = $other + $eles[2] - $eles[1] + 1;
		      
		      ##cont count
		      if($otherP != $eles[1] -1)
		      {
			  $otherC++ ;
			  $otherP = $eles[2];
		      }
		  }
	      }
	  }
	 close(AGP);
     }
     

     if(!open(GAP, "<$dir/$chr/chr$chr.gap"))
     {
	 print STDERR "Can't open $chr.gap .\n";
	
     }
     else
     {
	 while(my $line = <GAP>)
	 {##read gap
	      
	      chomp($line);
	      my (@items) = split(/\t/, $line);
	      next if($items[2] < 10 || $items[2] > 10000);
	      next if(defined($begin{$items[0]}));
	      $other +=$items[2];
	      $gap += $items[2];
	      $otherC++;
	      $hide += $items[2];
	      $hideC++;
	  }
     }
	 
     if($internal == 1)
     {
	 if($noRandom == 0)
	 {
	     print "chr$chr\t$base\t", $base-$gap,"\t$gap\t$cent\t$telo\t$teloC\t$short\t$heter\t$heterC\t$other\t$otherC\t$hide\t$hideC\t", $baseR - $gapR, "\n";
	 }
	 else
	 {
	     print "chr$chr\t$base\t", $base-$gap,"\t$gap\t$cent\t$telo\t$teloC\t$short\t$heter\t$heterC\t$other\t$otherC\t$hide\t$hideC\t-\n";
	 }
     }
     else
     {
	  if($noRandom == 0)
	  {
	      print "chr$chr\t$base\t", $base-$gap,"\t$gap\t", $cent + $telo + $short + $heter,"\t", $centC + $teloC + $shortC + $heterC, "\t$other\t$otherC\t", $baseR - $gapR, "\n";
	  }
	  else
	  {
	      print "chr$chr\t$base\t", $base-$gap,"\t$gap\t", $cent + $telo + $short + $heter,"\t", $centC + $teloC + $shortC + $heterC, "\t$other\t$otherC\t-\n";
	      
         }
      }
     $totalBase += $base;
     $totalGap += $gap;
     $totalTelo += $telo;
     $totalCent += $cent;
     $totalHeter += $heter;
     $totalOther += $other;
     $totalShort += $short;
     $totalRandom = $totalRandom + $baseR - $gapR;
     $totalHidden += $hide;

#     $gapCount += $gapC;
     $centCount += $centC;
     $teloCount += $teloC;
     $heterCount += $heterC;
     $shortCount += $shortC;
     $otherCount += $otherC;
     $hiddenCount += $hideC;
}

if($internal == 1)
{
    print "overallChrom\t$totalBase\t", $totalBase-$totalGap, "\t$totalGap\t$totalCent\t$totalTelo\t$teloCount\t$totalShort\t$totalHeter\t$heterCount\t$totalOther\t$otherCount\t$totalHidden\t$hiddenCount\t$totalRandom\n";
}
else
{
    print "overallChrom\t$totalBase\t", $totalBase-$totalGap, "\t$totalGap\t", $totalCent + $totalTelo + $totalShort + $totalHeter, "\t", $centCount + $teloCount + $shortCount + $heterCount, "\t$totalOther\t$otherCount\t$totalRandom\n";
}
