#!/bin/tcsh -ef
source `which qaConfig.csh`

set db=""
set sql=~/kent/src/hg/lib/cytoBandIdeo.sql

if ( $#argv == 0 || $#argv > 2 ) then
  # no command line args
  echo
  echo "  make a cytoBandIdeo table for navigation if no real cytology available."
  echo "  checks for existing cytoBandIdeo table and optionally overwrites."
  echo "  note: the script may break on load step if chromNames are too long."
  echo
  echo "    usage:  database [overwrite]"
  echo
  exit
else
  set db=$argv[1]
endif

if ( $#argv == 2 ) then
  if ( $argv[2] == "overwrite"  ) then
    hgsql -e "DROP TABLE cytoBandIdeo" $db
  else
    echo
    echo 'second argument must be "overwrite"'
    echo 'which will overwrite existing cytoBandIdeo table'
    echo
    exit
  endif
endif

if ( `hgsql -N -e "SHOW TABLES LIKE 'cytoBandIdeo'" $db` == "cytoBandIdeo" ) then
  set ideoCount=`hgsql -N -e "SELECT COUNT(*) FROM cytoBandIdeo" $db` 
  echo
  echo "$db cytoBandIdeo table has $ideoCount rows"
  echo 'run program with "overwrite" argument to continue'
  $0
  exit
endif 

# get chroms and sizes
hgsql -N -e 'SELECT chrom, size FROM chromInfo' $db > $db.chroms
# find all chromnames with centromeres and the cen coords
hgsql -N -e 'SELECT chrom, chromStart, chromEnd FROM gap WHERE type = "centromere"' $db \
  | sort > $db.cens

if ( -s $db.cens ) then # process cens
  # get the names only
  cat $db.cens | awk '{print $1}' > $db.cenNames

  # process cens into pieces
  rm -f $db.splitChroms
  foreach cenchrom (`cat $db.cenNames`)
    set   pEnd=`cat $db.cens   | grep -w $cenchrom | awk '{print $2}'`
    set qStart=`cat $db.cens   | grep -w $cenchrom | awk '{print $3}'`
    set   qEnd=`cat $db.chroms | grep -w $cenchrom | awk '{print $2}'`
    # make pArm
    if ( $pEnd != 0 ) then # no pArm if acrocentric
      echo $cenchrom 0 $pEnd p gneg | awk '{print $1"\t"$2"\t"$3"\t"$4"\t"$5}' \
        >> $db.splitChroms
    endif
    # make qArm
    if ( $qStart != $qEnd ) then # no qArm if acrocentric (yes, it happens: see ornAna1 chrX2)
      echo $cenchrom $qStart $qEnd q gneg | awk '{print $1"\t"$2"\t"$3"\t"$4"\t"$5}' \
        >> $db.splitChroms
    endif
  
    # split the centromere into halves for making triangles in ideogram
    set cen1start=$pEnd
    set   cen2end=$qStart
    @ censize = $cen2end - $cen1start
    @ cen1end = $cen1start + $censize / 2
    @ cen2start = $cen1end
    echo $cenchrom $cen1start $cen1end cen acen | awk '{print $1"\t"$2"\t"$3"\t"$4"\t"$5}' \
      >> $db.splitChroms
    echo $cenchrom $cen2start $cen2end cen acen | awk '{print $1"\t"$2"\t"$3"\t"$4"\t"$5}' \
      >> $db.splitChroms
  end
  cat $db.splitChroms > $db.cytoBand
  # remove cen chroms from full list of chroms and format for cytoBandIdeo table
  cat $db.chroms | grep -w -v -f $db.cenNames | sort > $db.chromsNoCens
  cat $db.chromsNoCens | awk '{print $1"\t"0"\t"$2"\t""\t""gneg"}' >> $db.cytoBand
else
  cat $db.chroms | awk '{print $1"\t"0"\t"$2"\t""\t""gneg"}' > $db.cytoBand
endif
bedSort $db.cytoBand $db.cytoBand

hgLoadSqlTab $db cytoBandIdeo $sql $db.cytoBand
if ( $status ) then
  set char=`hgsql -N -e "SELECT MAX(LENGTH(chrom)) FROM chromInfo" $db`
  echo
  echo "Load failed.  Possibly due to long chrom names."
  echo "Edit kent/src/hg/lib/cytoBandIdeo.sql to increase chrom KEY size:"
  grep  PRIMARY ~/kent/src/hg/lib/cytoBandIdeo.sql
  grep  UNIQUE  ~/kent/src/hg/lib/cytoBandIdeo.sql
  echo "Your longest chromName is $char long"
  echo 
endif

