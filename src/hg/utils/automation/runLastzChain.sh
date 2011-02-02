#!/bin/sh

echo "example script to run lastz and chaining on two genomes in 2bit files"
echo "adjust this script for your local example, you have a couple of choices"
echo "for parameter sets.  You will need a parasol cluster computer system"
echo "to run the large number of lastz instances."
echo "requires companion script constructLiftFile.pl and"
echo "partitionSequence.pl"
echo 
echo "The point is to illustrate the steps of:"
echo "1. partitioning the two genomes into:"
echo "   a. 10,000,000 overlapping 10,000 chunks for the target sequence"
echo "   b. 20,000,000 no overlap chunks for the query sequence"
echo "2. setup cluster run target.list query.list lastz run script"
echo "3. chaining the psl results from the lastz procedure"

exit 255

# typical axtChain and lastz parameter sets:
export chainNear="-minScore=5000 -linearGap=medium"
export chainMedium="-minScore=3000 -linearGap=medium"
export chainFar="-minScore=5000 -linearGap=loose"
export lastzNear="B=0 C=0 E=150 H=0 K=4500 L=3000 M=254 O=600 Q=/scratch/data/blastz/human_chimp.v2.q T=2 Y=15000"
export lastzMedium="B=0 C=0 E=30 H=0 K=3000 L=3000 M=50 O=400 T=1 Y=9400"
export lastzFar="B=0 C=0 E=30 H=2000 K=2200 L=6000 M=50 O=400 Q=/scratch/data/blastz/HoxD55.q T=2 Y=3400"

# select one of three different parameter sets
# Near == genomes close to each other
# Medium == genomes at middle distance from each other
# Far == genomes distant from each other

export chainParams="$chainNear"
export lastzParams="$lastzNear"

#  WRKDIR is where your 2bit files are and where you want this to work
export WRKDIR="/full/path/to/testLastz"
cd ${WRKDIR}
export TNAME=ce9
export QNAME=cb3
export TARGET=${WRKDIR}/${TNAME}.2bit
export QUERY=${WRKDIR}/${QNAME}.2bit

ls -ld $TARGET $QUERY

if [ ! -s ${TNAME}.chrom.sizes ]; then
twoBitInfo ${TARGET} stdout | sort -k2nr > ${TNAME}.chrom.sizes
rm -fr ${TNAME}PartList ${TNAME}.part.list
mkdir ${TNAME}PartList
fi
if [ ! -s ${QNAME}.chrom.sizes ]; then
twoBitInfo ${QUERY} stdout | sort -k2nr > ${QNAME}.chrom.sizes
rm -fr ${QNAME}PartList ${QNAME}.part.list
mkdir ${QNAME}PartList
fi

if [ ! -s ${TNAME}.part.list ]; then
partitionSequence.pl 10000000 10000 ${TARGET} ${TNAME}.chrom.sizes 1 \
	-lstDir ${TNAME}PartList > ${TNAME}.part.list
fi
if [ ! -s ${QNAME}.part.list ]; then
partitionSequence.pl 20000000 0 ${QUERY} ${QNAME}.chrom.sizes 1 \
	-lstDir ${QNAME}PartList > ${QNAME}.part.list
fi

grep -v PartList ${TNAME}.part.list > target.list
for F in ${TNAME}PartList/*.lst
do
    cat ${F}
done >> target.list

grep -v PartList ${QNAME}.part.list > query.list
for F in ${QNAME}PartList/*.lst
do
    cat ${F}
done >> query.list

./constructLiftFile.pl ${TNAME}.chrom.sizes target.list > target.lift
./constructLiftFile.pl ${QNAME}.chrom.sizes query.list > query.lift

echo "#LOOP" > template
echo 'runLastz $(path1) $(path2) $(file1) $(file2) {check out exists+ psl/$(file1).$(file2).psl.gz}' >> template
echo "#ENDLOOP" >> template

cat <<_EOF_ > runLastz
#!/bin/csh -fe
set T = \$1
set Q = \$2
set FT = \$3
set FQ = \$4
set tmpDir = /scratch/tmp/\${FT}
mkdir -p raw psl \${tmpDir}
twoBitToFa \${T} \${tmpDir}/\${FT}.fa
twoBitToFa \${Q} \${tmpDir}/\${FQ}.fa
/cluster/bin/penn/lastz-distrib-1.02.00/bin/lastz \${tmpDir}/\${FT}.fa \
    \${tmpDir}/\${FQ}.fa \
    ${lastzParams} \
    > raw/\${FT}.\${FQ}.lav
lavToPsl raw/\${FT}.\${FQ}.lav stdout \
    | liftUp -type=.psl stdout target.lift error stdin \
    | liftUp -nohead -pslQ -type=.psl stdout query.lift error stdin \
	| gzip -c > psl/\${FT}.\${FQ}.psl.gz
rm -f \${tmpDir}/\${FT}.fa \${tmpDir}/\${FQ}.fa
rmdir --ignore-fail-on-non-empty \${tmpDir}
_EOF_

echo "ready to run lastz kluster job:"
echo "gensub2 target.list query.list template jobList"
echo "para make jobList"

echo "when finished, run the commands in chainJobs.csh to perform the chaining"

mkdir -p chain
echo "#!/bin/csh -fe" > chainJobs.csh
for T in `cat target.list | sed -e "s#${WRKDIR}/##"`
do
echo "zcat psl/${T}.*.psl.gz \\"
echo "    | axtChain -psl -verbose=0 ${chainParams} \\"
echo -e "\tstdin ${TARGET} ${QUERY} stdout \\"
echo "   | chainAntiRepeat ${TARGET} ${QUERY} stdin chain/${T}.chain"
done >> chainJobs.csh

echo "find ./chain -name \"*.chain\" | chainMergeSort -inputList=stdin | gzip -c > ${TNAME}.${QNAME}.all.chain.gz" >> chainJobs.csh
