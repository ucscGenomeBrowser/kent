#!/bin/tcsh

###############################################
# 
#  08-25-04
#  Robert Kuhn
# 
#  checks the metadata for a new assembly.
# 
###############################################

set db=""
set dbTrunc=""


if ($#argv == 1 || $#argv == 3) then
  set db=$argv[1]
  set dbTrunc=`echo $db | sed -e "s/[0-9]*//"`
else
  # no command line args
  echo
  echo "  checks the metadata for a new assembly."
  echo
  echo "    usage:  database, [machine1], [machine2]"
  echo '        (defaults to dev and beta, use "RR" for production machines, not hgw#)'
  echo
  exit
endif

# echo "db = $db"
# echo "dbTrunc = $dbTrunc"

# set defaults
set centdb1="hgcentraltest"
set centdb2="hgcentralbeta"
set mach1=""
set mach2="-h hgwbeta"

if ($#argv == 3) then
  set centdb1=$argv[2]
  set centdb2=$argv[3]
endif

# set machines to dev where needed
if ($centdb1 == "dev" || $centdb1 == "hgwdev") then
  set centdb1="hgcentraltest"
  set mach1=""
endif

if ($centdb2 == "dev" || $centdb2 == "hgwdev") then
  set centdb2="hgcentraltest"
  set mach2=""
endif

# set machines to beta where needed
if ($centdb1 == "beta" || $centdb1 == "hgwbeta") then
  set centdb1="hgcentralbeta"
endif

if ($centdb2 == "beta" || $centdb2 == "hgwbeta") then
  set centdb2="hgcentralbeta"
endif

# set machines to RR where needed
if ($centdb1 == "RR" || $centdb1 == "rr") then
  set centdb1="hgcentral"
  set mach1="-h genome-centdb"
endif

if ($centdb2 == "RR" || $centdb2 == "rr") then
  set centdb2="hgcentral"
  set mach2="-h genome-centdb"
endif


# echo
# echo "mach1 = $mach1"
# echo "centdb1= $centdb1"
# echo
# echo "mach2 = $mach2"
# echo "centdb2= $centdb2"
echo

set out1=`echo $centdb1 | sed -e "s/-h //"i`
set out2=`echo $centdb2 | sed -e "s/-h //"i`

# echo "out1 = $out1"
# echo "out2 = $out2"



# ----------------------------------------------------
# compare metadata

echo "database = $db"
echo

hgsql $mach1 -e 'SELECT * FROM dbDb WHERE name = "'$db'"' $centdb1 > dbDb.$out1 
hgsql $mach2 -e 'SELECT * FROM dbDb WHERE name = "'$db'"' $centdb2  > dbDb.$out2
comm -23 dbDb.$out1 dbDb.$out2 > dbDb.${out1}Only
comm -13 dbDb.$out1 dbDb.$out2 > dbDb.${out2}Only
comm -12 dbDb.$out1 dbDb.$out2 > dbDb.common
wc -l dbDb.${out1}Only dbDb.${out2}Only dbDb.common | grep -v "total"
echo


hgsql $mach1 -e 'SELECT * FROM blatServers WHERE db = "'$db'"' $centdb1 | sort > blat.$out1 
hgsql $mach2 -e 'SELECT * FROM blatServers WHERE db = "'$db'"' $centdb2 | sort > blat.$out2
comm -23 blat.$out1 blat.$out2 > blat.${out1}Only
comm -13 blat.$out1 blat.$out2 > blat.${out2}Only
comm -12 blat.$out1 blat.$out2 > blat.common
wc -l blat.${out1}Only blat.${out2}Only blat.common | grep -v "total"
echo


hgsql $mach1 -e 'SELECT * FROM defaultDb WHERE name LIKE "'$dbTrunc'%"' \
   $centdb1 > defaultDb.$out1 
hgsql $mach2 -e 'SELECT * FROM defaultDb WHERE name LIKE "'$dbTrunc'%"' \
   $centdb2 > defaultDb.$out2
comm -23 defaultDb.$out1 defaultDb.$out2 > defaultDb.${out1}Only
comm -13 defaultDb.$out1 defaultDb.$out2 > defaultDb.${out2}Only
comm -12 defaultDb.$out1 defaultDb.$out2 > defaultDb.common
wc -l defaultDb.${out1}Only defaultDb.${out2}Only defaultDb.common | grep -v "total"
echo


hgsql $mach1 -e 'SELECT * FROM gdbPdb WHERE genomeDb = "'$db'"' $centdb1 > gdbPdb.$out1 
hgsql $mach2 -e 'SELECT * FROM gdbPdb WHERE genomeDb = "'$db'"' $centdb2 > gdbPdb.$out2
comm -23 gdbPdb.$out1 gdbPdb.$out2 > gdbPdb.${out1}Only
comm -13 gdbPdb.$out1 gdbPdb.$out2 > gdbPdb.${out2}Only
comm -12 gdbPdb.$out1 gdbPdb.$out2 > gdbPdb.common
wc -l gdbPdb.${out1}Only gdbPdb.${out2}Only gdbPdb.common | grep -v "total"
echo

