#	Procedure for analysis of the Repeat Masker rmsk tables
#	 $Id: README.txt,v 1.1 2005/04/28 00:01:22 hiram Exp $

#	On a scratch disk location on hgwdev, extract bed
#	files from the rmsk tables:

ssh hgwdev
mkdir /scratch/$USER/rmsk
cd /scratch/$USER/rmsk

cat << '_EOF_' > rmskToBed.sh
#!/bin/sh

usage()
{
echo "usage: rmskToBed.sh <db>"
exit 255
}

if [ $# -lt 1 ]; then
    usage
fi

DB=$1


if [ -d "${DB}" ]; then
    mv "${DB}" "${DB}.0"
    rm -fr "${DB}.0" &
fi

mkdir "${DB}"

for T in `hgsql -N "${DB}" -e 'show tables like "%rmsk";'`
do
    chr=${T/_rmsk/}
    if [ "${T}" = "rmsk" ]; then
        chr="chr"
    else
        chr=${chr/rmsk/}
    fi
    echo "${chr}"
    hgsql -N "${DB}" -e 'select
genoName,genoStart,genoEnd,repClass,swScore,strand from '"${T}"';' > \
    "${DB}/${chr}.bed"
done
'_EOF_'

chmod +x rmskToBed.sh

#	This creates a directory called ce2 with a bed file
#	for each chromosome with the rmsk elements in the bed files
#	and the name column being the repeat masker class name
./rmskToBed.sh ce2

cd ce2

#	Now split those per-chrom bed files into bed files of each repeat
#	masker class element:
cat << '_EOF_' > splitByName.sh
#!/bin/sh

usage()
{
echo "usage: splitByName <bed file>"
echo "splits the bed file into directories by the name column"
exit 255
}

if [ $# -lt 1 ]; then
    usage
fi

F=$1

#       There is a class called DNA? - change that name to DNAx
#               to avoid shell problems of such a name
if [ -s "${F}" ]; then
    awk '{print $4}' "${F}" | sort -u | while read repC
    do
        dirName=`echo "${repC}" | sed -e "s/\?/x/;"`
        pat=`echo "${repC}" | sed -e "s/\?/\\?/;"`
        echo "'$repC' '$dirName' '$pat'"
        rm -fr "${dirName}"
        mkdir  "${dirName}"
        perl -ane \
            "my \$pat=\"$pat\";print if (\$F[3] eq \$pat)" \
                "${F}" > "${dirName}/file.bed"
        dirName=""
        pat=""
    done
else
    echo "ERROR: can not find file: ${F}"
    usage
fi
'_EOF_'

cat << '_EOF_' > runAllSplits.sh
#!/bin/sh

TOP=`pwd`
export TOP
SPL="${TOP}/splitByName.sh"
export SPL

for F in *.bed
do
    C=${F/.bed/}
    echo ${F} ${C}
    mkdir "${C}"
    cd "${C}"
    ${SPL} ../${F}
    cd "${TOP}"
done
'_EOF_'

chmod +x runAllSplits.sh

#	This creates a directory for each chromosome, and each of those
#	directories contains directories for each of the repeat masker class
#	elements.
./runAllSplits.sh

#	With that hierarchy of directories, can not create the analysis html
#	page:

~/kent/src/hg/makeDb/hgLoadOut/rmskFeatBits.pl -verbose=1 -html -db=ce2 \
	chr*/*/file.bed > ce2.summary.html 2> ce2.stderr

====================================================================
This file last updated: $Date: 2005/04/28 00:01:22 $

