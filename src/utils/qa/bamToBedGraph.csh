#!/bin/tcsh

###############################################
# 
#  05-22-2014
#  Robert Kuhn
#
#  convert a BAM file to bedGraph
# 
###############################################

onintr cleanup

set db=""
set bamFile=""
set bedgraphFile=""


if ( $#argv == 0 || $#argv > 4 ) then
  # no command line args
  echo
  echo "  convert a BAM file into a bedGraph to assess coverage"
  echo "  requires bedtools"
  echo
  echo "    usage:  db in.bam out.bedGraph [splitByStrand]"
  echo "      where splitByStrand makes two output files"
  echo "    output is sorted for input into bedGraphToBigWig"
  echo
  exit
endif

set db=$argv[1]
set bamFile=$argv[2]
set bedgraphFile=$argv[3]

bedtools bamtobed -split -i $bamFile \
  | awk '{print "chr"$1, "\t"$2, "\t"$3, "\t"$4, "\t"$5, "\t"$6}' \
  | sed s/chrMT/chrM/ | sort -k1,1 -k2,2n > $bamFile.bed
if ( $#argv == 4 ) then
  if ( $argv[4] == "splitByStrand" ) then
   awk '{if ($6 == "+") print $1, "\t"$2, "\t"$3, "\t"$4, "\t"$5, "\t"$6}' $bamFile.bed \
     | bedItemOverlapCount $db stdin | sort -k1,1 -k2,2n > $bedgraphFile.plus
   awk '{if ($6 == "-") print $1, "\t"$2, "\t"$3, "\t"$4, "\t"$5, "\t"$6}' $bamFile.bed \
     | bedItemOverlapCount $db stdin | sort -k1,1 -k2,2n > $bedgraphFile.minus
  else
    echo
    echo '  4th argument can only be:  "splitByStrand"'
    $0
    exit
  endif
else
  cat $bamFile.bed | bedItemOverlapCount $db stdin | sort -k1,1 -k2,2n > $bedgraphFile
endif

rm -f $bamFile.bed 

cleanup:
rm -f $bamFile.bed 
