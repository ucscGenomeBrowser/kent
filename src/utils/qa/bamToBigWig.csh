#!/bin/tcsh

###############################################
# 
#  05-22-2014
#  Robert Kuhn
#
#  convert a BAM file to bigWig
# 
###############################################

onintr cleanup

set db=""
set bamFile=""
set bwFile=""


if ( $#argv == 0 || $#argv > 4 ) then
  # no command line args
  echo
  echo "  convert a BAM file into a bigWig"
  echo "  requires bedtools"
  echo
  echo "    usage:  db  in.bam  out.bw  [splitByStrand]"
  echo "      where splitByStrand makes two output files"
  echo
  exit
endif

set db=$argv[1]
set bamFile=$argv[2]
set bigWigFile=$argv[3]

# if bigwig file has .bw on the end, remove so can add plus/minus before .bw
set bigWigFile=`echo $bigWigFile | sed s/.bw\$//`

fetchChromSizes $db > $db.chromsizes$$

if ( $#argv == 4 ) then
  if ( $argv[4] == "splitByStrand" ) then
    bamToBedGraph.csh $db $bamFile $bamFile.bedgraph splitByStrand
    bedGraphToBigWig $bamFile.bedgraph.plus  $db.chromsizes$$ $bigWigFile.plus.bw
    bedGraphToBigWig $bamFile.bedgraph.minus $db.chromsizes$$ $bigWigFile.minus.bw
  else
    echo
    echo '  4th argument can only be:  "splitByStrand"'
    $0
    exit
  endif
else
  bamToBedGraph.csh $db $bamFile $bamFile.bedgraph
  bedGraphToBigWig $bamFile.bedgraph $db.chromsizes$$ $bigWigFile.bw
endif

cleanup:
rm -f $db.chromsizes$$
rm -f $bamFile.bedgraph.plus
rm -f $bamFile.bedgraph.minus
rm -f $bamFile.bedgraph
