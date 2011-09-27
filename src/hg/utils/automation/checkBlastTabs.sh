#!/bin/sh
#
# this script demonstrates how to check all of the blast tab tables
#	in all genome assemblies for proper date/time update status given
#       the requirement that each blast table in two corresponding assemblies
#	have update table times that are newer than either of the two
#	gene tables in question.
#
#	When run in this manner:
#	./t.sh 2>&1 | grep -w vs
# a bunch of numbers are output.  Look for any negative numbers for problems.
# Currently on hgwdev, there are negative numbers as concerns anything
# related to rn4 - this is because the rn4 gene table has a newer update
# time, but it is actually the exact same contents as what it was when
# it was properly older.  It has been reloaded since it was done.
#	But reloaded with its original contents it was at the time the
#	tables were computed.  It is OK.
#
############################################################################
# return the newest julian date from the three status dates:
#       CREATE  UPDATE  CHECK
dbJday() {
D=$1
T=$2
echo "${D}.${T}" 1>&2
hgsql -N -e "show table status like \"${T}\";" ${D} \
        | cut -f12-14 | tr '[\t]' '[\n]' | sort -u 1>&2
hgsql -N -e "show table status like \"${T}\";" ${D} \
        | cut -f12-14 | tr '[\t]' '[\n]' | sort -u | while read DT
do
    jday -d ${DT} | sed -e "s/  *//"
done | sort -rn | head -1
}

############################################################################
# MAIN
if [ "$1" != "go" ]; then
    echo "usage: checkBlastTabs.sh go 2>&1 | grep -w vs" 1>&2
    echo "script runs on hgwdev to verify all BlastTabs are properly" 1>&2
    echo "up to date.  Watch for negative numbers in the output." 1>&2
    echo "Those negative numbers indicate potential date/time problems." 1>&2
    echo "To see those specifically:" 1>&2
    echo "./checkBlastTabs.sh go 2>&1 | grep -w vs | grep '\-'"
    exit 255
fi

############################################################################
# Outside loop runs through all assemblies with their corresponding
#	gene table names and BlastTab table prefix names
for CM in hg19_knownGene_hg danRer7_ensGene_dr sacCer3_sgdGene_sc \
        mm9_knownGene_mm rn4_knownGene_rn dm3_flyBaseGene_dm ce6_sangerGene_ce
do
    checkMe=`echo $CM | awk -F_ '{print $1}'`
    checkGene=`echo $CM | awk -F_ '{print $2}'`
    checkMeP=`echo $CM | awk -F_ '{print $3}'`
echo "${CM}" 1>&2

# Inner loop runs through the same set of assemblies again so that
# everything against everything can be verified.  Special cases of:
#	1. don't verify same with same
#	2. there are no BlastTab tables on danRer7
#
echo hg19_knownGene_hg mm9_knownGene_mm rn4_knownGene_rn danRer7_ensGene_dr \
        sacCer3_sgdGene_sc dm3_flyBaseGene_dm ce6_sangerGene_ce \
        | sed -e "s/${CM}//" | tr '[ ]' '[\n]' | sed -e "/^$/d" | while read DT
do
echo "DT: ${DT}" 1>&2
    D=`echo $DT | awk -F_ '{print $1}'`
    T=`echo $DT | awk -F_ '{print $2}'`
    P=`echo $DT | awk -F_ '{print $3}'`
otherGene=`dbJday ${D} ${T}`
checkMeGene=`dbJday ${checkMe} ${checkGene}`
checkMeTab=`dbJday ${D} ${checkMeP}BlastTab`
checkMeOtherTab=`dbJday ${checkMe} ${P}BlastTab`
pos1=`echo $otherGene $checkMeTab | awk '{printf "%.6f", $2-$1}'`
pos2=`echo $otherGene $checkMeOtherTab | awk '{printf "%.6f", $2-$1}'`
pos3=`echo $checkMeGene $checkMeTab | awk '{printf "%.6f", $2-$1}'`
pos4=`echo $checkMeGene $checkMeOtherTab | awk '{printf "%.6f", $2-$1}'`
echo "${D} $checkMe genes: $otherGene - $checkMeGene"
echo "${D} $checkMe blastTabs: $checkMeTab - $checkMeOtherTab"
case "${D}" in
    danRer7)
    echo "${D} $checkMe ${P}BlastTab vs. ${D}.${T} $pos2 ${checkMe}.${checkGene} $pos4"
        ;;
    *)
        case "${checkMe}" in
                danRer7)
    echo "${D} $checkMe ${checkMeP}BlastTab vs. ${D}.${T} $pos1 ${checkMe}.${checkGene} $pos3"
                ;;
                *)
    echo "${D} $checkMe ${checkMeP}BlastTab vs. ${D}.${T} $pos1 ${checkMe}.${checkGene} $pos3"
    echo "${D} $checkMe ${P}BlastTab vs. ${D}.${T} $pos2 ${checkMe}.${checkGene} $pos4"
                ;;
        esac
        ;;
esac
done
done
