#!/bin/tcsh
# $Id: synBlastp.csh,v 1.5 2009/09/27 03:32:29 galt Exp $
##########################################################################
#
#  synBlastp.csh - Help filter out unwanted paralogs from xxBlastTab 
#    using chains for synteny  (Galt 2007-01-10)
#
# Background: The xxBlastTab tables are made with a simple blastall 
# (blastp with -b 1) which chooses the best match.  Unfortunately this
# means that if there is no proper match it will still pick something
# even though it's probably not orthologous. This is especially a problem
# in organisms like rat knownGene which has only 30% gene coverage. 
# The strategy here is to filter our xxBlastTab using synteny mappings from
# the chains. This is done by simply taking $db.kg and using /gbdb/$db chains
# and pslMap to lift the genes to the target xx assembly.  Then hgMapToGene
# will find which of those mapped ids have good overlap with xx.knownGene.
# The final mapping is then created by doing an inner join between 
# the traditional xxBlastTab and the mapping table produced above
# and deleting the appropriate records.
#
# We are starting with xxBlastTab tables already built in the usual way with
# blastall/blastp, probably with doHgNearBlastp.pl script.

# check which host we are on
if ( "$HOST" != "hgwdev" ) then
 echo "error: you must run this script on hgwdev!"
 exit 1
endif

# check for required params db and otherDb
if ( "$1" == "" ) then
 echo "error: you must specify assembly db and other db on commandline, e.g."
 echo "synBlastp.csh hg18 rn4"
 exit 1
endif

if ( "$2" == "" ) then
 echo "error: you must specify assembly db and other db on commandline, e.g."
 echo "synBlastp.csh hg18 rn4"
 exit 1
endif

set db = "$1"
set otherDb = "$2"
set geneTable = "knownGene"
set otherGeneTable = "knownGene"

if ( "$3" != "" ) then
    if ( "$4" == "" ) then
	 echo "error: if you specify geneTable, you need to specify otherGeneTable too, e.g."
	 echo "synBlastp.csh hg18 rn4 knownGene rgdGene2"
	 exit 1
    endif
 set geneTable = $3
 set otherGeneTable = $4
endif

# check for required lift file in /gbdb/$db
set otherDbUp1st = `echo "$otherDb" | gawk '{print toupper(substr($1,1,1)) substr($1,2,length($1)-1)}'`
set lift = "/gbdb/$db/liftOver/${db}To${otherDbUp1st}.over.chain.gz"
if ( ! -e $lift ) then
 echo "error: $lift not found. You must specify assembly db and other db on commandline, e.g."
 echo "synBlastp.csh hg18 rn4"
 exit 1
endif


#check for required tables $db.xxBlastTab and db.$geneTable and otherDb.$otherGeneTable

set otherOrg = `echo "$otherDb" | sed 's/[0123456789]*//g'`

set xxBlastTab = "${otherOrg}BlastTab"

# db.xxBlastTab exists?
set sql = "show tables like '$xxBlastTab'"
set result = `hgsql $db -BN -e "$sql"`
if ($result != "$xxBlastTab") then
 echo "error: table $db.$xxBlastTab not found."
 exit 1
endif

# db.$geneTable exists?
set sql = "show tables like '$geneTable'"
set result = `hgsql $db -BN -e "$sql"`
if ($result != "$geneTable") then
 echo "error: table $db.$geneTable not found."
 exit 1
endif

# otherDb.$otherGeneTable exists?
set sql = "show tables like '$otherGeneTable'"
set result = `hgsql $otherDb -BN -e "$sql"`
if ($result != "$otherGeneTable") then
 echo "error: table $otherDb.$otherGeneTable not found."
 exit 1
endif


#debug
echo "db=$db"
echo "geneTable=$geneTable"
echo "otherDb=$otherDb"
echo "otherGeneTable=$otherGeneTable"
#echo "lift=$lift"
#echo "otherDbUp1st=$otherDbUp1st"
#echo "otherOrg=$otherOrg"
#echo "xxBlastTab=$xxBlastTab"

# --- general setup ---

cd ${HOME}
if (-d synBlastpTemp) then
    rm -fr synBlastpTemp
endif
mkdir synBlastpTemp
cd synBlastpTemp

# --- pre-clean old tables from any previous failure ---
hgsql $otherDb -BN -e "drop table temp${db}KgPslMapped if exists" >& /dev/null
hgsql $otherDb -BN -e "drop table temp${otherDb}kgTo${db}kg if exists" >& /dev/null
hgsql $db -BN -e "drop table ${xxBlastTab}Temp if exists" >& /dev/null


# --- filter $db.xxBlastTab based on pslMapped $db.kg hgMapToGene'd over to $otherDb.kg ---

echo "genePredToFakePsl:"
genePredToFakePsl $db $geneTable $db.kg.psl $db.kg.cds

echo "pslMap via $lift :"
zcat $lift | pslMap -chainMapFile -swapMap $db.kg.psl stdin stdout \
|  sort -k 14,14 -k 16,16n > $db.$otherDb.kg.psl

echo "hgLoadPsl:"
hgLoadPsl -clientLoad $otherDb $db.$otherDb.kg.psl -table=temp${db}KgPslMapped

echo "hgMapToGene:"
hgMapToGene -all $otherDb -type=psl -verbose=0 temp${db}KgPslMapped $otherGeneTable temp${otherDb}kgTo${db}kg

echo "$db.${xxBlastTab}:"

# original for comparison:
set sql = "select count(distinct query) from ${xxBlastTab}"; echo "old number of unique query values:"; hgsql $db -BN -e "$sql"
set sql = "select count(distinct target) from ${xxBlastTab}"; echo "old number of unique target values"; hgsql $db -BN -e "$sql"

# drop rows that do not have a match in the psl-mapped otherDb table.
hgsql $db -BN -e "delete a from ${xxBlastTab} a left join ${otherDb}.temp${otherDb}kgTo${db}kg b on (a.query = b.value and a.target = b.name) where b.value is NULL"

set sql = "select count(distinct query) from ${xxBlastTab}"; echo "new number of unique query values:"; hgsql $db -BN -e "$sql"
set sql = "select count(distinct target) from ${xxBlastTab}"; echo "new number of unique target values"; hgsql $db -BN -e "$sql"

#cleanup:

hgsql $otherDb -BN -e "drop table temp${db}KgPslMapped"
hgsql $otherDb -BN -e "drop table temp${otherDb}kgTo${db}kg"

rm $db.kg.psl $db.kg.cds $db.$otherDb.kg.psl
cd ..
rmdir synBlastpTemp


