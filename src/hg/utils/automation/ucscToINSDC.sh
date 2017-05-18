#!/bin/sh

export chrUN=""
export chrMT=""

if [ $# -lt 1 ]; then
  echo "usage: ucscToINSDC.sh <priAsmPath> [chrMT_accession]" 1>&2
  echo "  <priAsmPath> is something like ../../genbank/GCA_*assembly_structure/Primary_Assembly" 1>&2
  echo "the [chrMT_accession] is optional, use when there is one." 1>&2
  exit 255
fi

export priAsmPath=$1
if [ $# -eq 2 ]; then
  chrMT=$2
fi

runAll() {


if [ -s ${priAsmPath}/assembled_chromosomes/chr2acc ]; then
  grep -v "^#" ${priAsmPath}/assembled_chromosomes/chr2acc \
     | sed -e 's/^/chr/'
fi

unCount=`grep chrUn_ ../../chrom.sizes | wc -l`
if [ $unCount -gt 0 ]; then
   chrUN="chrUn_"
fi

if [ -s ${priAsmPath}/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz ]; then
  zcat ${priAsmPath}/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz | grep -v "^#" | cut -f1 | sort -u \
     | sed -e 's/^\([A-Za-z0-9_]*\).\([0-9]*\)/'${chrUN}'\1v\2\t\1.\2/;'
fi

if [ -s ${priAsmPath}/unlocalized_scaffolds/unlocalized.chr2scaf ]; then
grep -v "^#" \
  ${priAsmPath}/unlocalized_scaffolds/unlocalized.chr2scaf \
    | sed -e 's/^\([A-Za-z0-9]*\)\t\([A-Za-z0-9_]*\).\([0-9]*$\)/chr\1_\2v\3_random\t\2.\3/;'

fi

if [ -s ${priAsmPath}/nuclear/assembled_chromosomes/chr2acc ]; then
   AC=`grep "^MT" ${priAsmPath}/nuclear/assembled_chromosomes/chr2acc | cut -f2`
   echo -e "chrM\t$AC"
else
  mCount=`grep chrM ../../chrom.sizes | wc -l`
  if [ $mCount -gt 0 ]; then
     if [ $mCount -eq 1 ]; then
        if [ "x${chrMT}y" != "xy" ]; then
          echo -e "chrM\t${chrMT}"
        else
          echo "need to find chrM accessions" 1>&2
        fi
     else
        if [ -s ${priAsmPath}/nuclear/unlocalized_scaffolds/unlocalized.chr2scaf ]; then
          grep -v "^#" ${priAsmPath}/nuclear/unlocalized_scaffolds/unlocalized.chr2scaf \
           | cut -f2 | sed -e 's/\([A-Za-z0-9]*\).\([0-9]*$\)/chrM_\1_random\t\1.\2/'
        else
           echo "need to find multiple chrM accessions" 1>&2
        fi
     fi
  fi
fi

}

runAll $* | sort > ucscToINSDC.txt
