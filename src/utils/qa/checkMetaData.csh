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
set metatables="dbDb blatServers defaultDb gdbPdb genomeClade liftOverChain"

if ( $#argv < 1 || $#argv > 3 ) then
  # no command line args
  echo
  echo "  checks the metadata for a new assembly."
  echo
  echo "    usage:  database [machine1 machine2]"
  echo '        (defaults to dev and beta, or use "RR".)'
  echo
  exit
else
  set db=$argv[1]
  set dbTrunc=`echo $db | sed -e "s/[0-9]*//"`
endif

# set defaults
set mach1="hgwdev"
set mach2="hgwbeta"
set centdb1="hgcentraltest"
set centdb2="hgcentralbeta"
set host1=""
set host2="-h hgwbeta"

# check if asssembly database exits on dev.
set orgCheck=`hgsql -N -e 'SELECT COUNT(*) FROM dbDb WHERE name = "'$db'"' \
  hgcentraltest`
if ( $orgCheck == 0 ) then
  echo
  echo "  $db is not a valid genome database."
  echo
  exit
endif

# set machines, if given on command line.
if ( $#argv == 2 ) then
  echo "\n  please use two machine names.\n"
  echo "${0}:"
  $0
  exit 1
endif

if ( $#argv == 3 ) then
  set mach1=$argv[2]
  set mach2=$argv[3]
endif

# set machines to dev where needed
if ( $mach1 == "hgwdev" ) then
  set centdb1="hgcentraltest"
  set host1=""
endif

if ( $mach2 == "hgwdev" ) then
  set centdb2="hgcentraltest"
  set host2=""
endif

# set machines to beta where needed
if ( $mach1 == "hgwbeta" ) then
  set centdb1="hgcentralbeta"
  set host1="-h hgwbeta"
endif

if ( $mach2 == "hgwbeta" ) then
  set centdb2="hgcentralbeta"
  set host2="-h hgwbeta"
endif


if ( $mach1 == "RR" || $mach1 == "rr" ) then
  set centdb1="hgcentral"
  set host1="-h genome-centdb"
endif

if ( $mach2 == "RR" || $mach2 == "rr" ) then
  set centdb2="hgcentral"
  set host2="-h genome-centdb"
endif

# set machines to RR where needed if hgw# format used
set covered="hgwdev hgwbeta rr RR"

echo $covered | grep -wq "$mach1"
if ( $status ) then
  checkMachineName.csh $mach1
  if ( $status ) then
    exit 1
  else
    set centdb1="hgcentral"
    set host1="-h genome-centdb"
  endif
endif
 
echo $covered | grep -wq "$mach2" 
if ( $status ) then
  checkMachineName.csh $mach2
  if ( $status ) then
    exit 1
  else
    set centdb2="hgcentral"
    set host2="-h genome-centdb"
  endif
endif

# echo
# echo "host1 = $host1"
# echo "centdb1= $centdb1"
# echo
# echo "host2 = $host2"
# echo "centdb2= $centdb2"
echo

# make file extention for output
set out1=`echo $centdb1 | sed -e "s/-h //"`
set out2=`echo $centdb2 | sed -e "s/-h //"`

# echo "out1 = $out1"
# echo "out2 = $out2"

# ----------------------------------------------------
# compare metadata

echo "database = $db"
echo

# check dbDb
set metatable="dbDb"

hgsql $host1 -Ne 'SELECT * FROM dbDb WHERE name = "'$db'"' $centdb1 \
  > $metatable.$db.$out1 
hgsql $host2 -Ne 'SELECT * FROM dbDb WHERE name = "'$db'"' $centdb2  \
  > $metatable.$db.$out2


# check blatServers
set metatable="blatServers"

hgsql $host1 -Ne 'SELECT * FROM blatServers WHERE db = "'$db'"' $centdb1 | sort \
  > $metatable.$db.$out1 
hgsql $host2 -Ne 'SELECT * FROM blatServers WHERE db = "'$db'"' $centdb2 | sort \
  > $metatable.$db.$out2 


# check defaultDb 
set metatable="defaultDb"

hgsql $host1 -Ne 'SELECT * FROM defaultDb WHERE name LIKE "'$dbTrunc'%"' \
   $centdb1 > $metatable.$db.$out1 
hgsql $host2 -Ne 'SELECT * FROM defaultDb WHERE name LIKE "'$dbTrunc'%"' \
   $centdb2 > $metatable.$db.$out2 


# check gdbPdb
set metatable="gdbPdb"

hgsql $host1 -Ne 'SELECT * FROM gdbPdb WHERE genomeDb = "'$db'"' $centdb1 \
  > $metatable.$db.$out1 
hgsql $host2 -Ne 'SELECT * FROM gdbPdb WHERE genomeDb = "'$db'"' $centdb2 \
  > $metatable.$db.$out2 


# check liftOverChain
set metatable="liftOverChain"

hgsql $host1 -Ne 'SELECT * FROM liftOverChain WHERE fromDb = "'$db'" \
  or toDb = "'$db'"' $centdb1 | sort \
  > $metatable.$db.$out1
hgsql $host2 -Ne 'SELECT * FROM liftOverChain WHERE fromDb = "'$db'" \
  or toDb = "'$db'"' $centdb2 | sort \
  > $metatable.$db.$out2


# check genomeClade
# get genome name for the assembly to query genomeClade table.

set genome=`hgsql -N -e 'SELECT genome FROM dbDb WHERE name = "'$db'"' \
  hgcentraltest`

# pull out last word of the find, if in the format "G. species" 
#    and use LIKE to query genomeClade.
set secondWord=`echo $genome | gawk -F" " '{print $2}'`
if ( $secondWord != "" ) then
  set genome=$secondWord
endif

set metatable="genomeClade"

# get lookup for clade check
# filter out "/" when it appears in genome name - to avoid e.g, Dog/Human
hgsql $host1 -Ne 'SELECT * FROM genomeClade WHERE genome LIKE "%'$genome'"' \
  $centdb1 | grep -v "/" > $metatable.$db.$out1 
hgsql $host2 -Ne 'SELECT * FROM genomeClade WHERE genome LIKE  "%'$genome'"' \
  $centdb2 | grep -v "/" > $metatable.$db.$out2
 
set metatable=""

# compare and  print results
foreach table ( `echo $metatables` )
  comm -23 $table.$db.$out1 $table.$db.$out2 > $table.$db.${out1}Only
  comm -13 $table.$db.$out1 $table.$db.$out2 > $table.$db.${out2}Only
  comm -12 $table.$db.$out1 $table.$db.$out2 > $table.$db.common
  wc -l $table.$db.${out1}Only $table.$db.${out2}Only $table.$db.common \
    | gawk '{ printf("%3d %-45s\n", $1, $2) }' \
    | grep -v "total"
  echo
end

