#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: testDynBlat.sh asmId > result.json\n" 1>&2
  exit 255
fi

export blatHost="dynablat-01"
export blatPort="4040"

export asmId=$1
export gcX=${asmId:0:3}
export d0=${asmId:4:3}
export d1=${asmId:7:3}
export d2=${asmId:10:3}
export dataDir="$gcX/$d0/$d1/$d2/$asmId"
export srcDir="/hive/data/genomes/asmHubs/$dataDir"
export faaFile="/cluster/home/hiram/kent/src/hg/makeDb/doc/asmHubs/rtp1.ace2.faa.gz"

time (gfClient -genome=$asmId -genomeDataDir=$dataDir -t=dnax -q=prot \
  $blatHost $blatPort $srcDir $faaFile stdout > $asmId.rtp1.ace2.psl) 2> $asmId.dynBlat.log

export realTime=`grep -w real $asmId.dynBlat.log | awk '{print $NF}'`
export resultLines=`pslScore $asmId.rtp1.ace2.psl | wc -l`
printf "%s\t%d\t%s\n" "${asmId}" "${resultLines}" "${realTime}"
rm -f $asmId.rtp1.ace2.psl $asmId.dynBlat.log

exit $?

export hubUrl="https://hgdownload-test.gi.ucsc.edu/hubs/$gcX/$d0/$d1/$d2/$asmId/hub.txt"

curl -I "$hubUrl"

curl -o "$asmId.json" \
  --data-urlencode "hubUrl=https://hgdownload-test.gi.ucsc.edu/hubs/$gcX/$d0/$d1/$d2/$asmId/hub.txt" \
  --data-urlencode "userSeq=>hg38.RTP1.prot
MRIFRPWRLRCPALHLPSLSVFSLRWKLPSLTTDETMCKSVTTDEWKKVFYEKMEEAKPA
DSWDLIIDPNLKHNVLSPGWKQYLELHASGRFHCSWCWHTWQSPYVVILFHMFLDRAQRA
GSVRMRVFKQLCYECGTARLDESSMLEENIEGLVDNLITSLREQCYGERGGQYRIHVASR
QDNRRHRGEFCEACQEGIVHWKPSEKLLEEEATTYTFSRAPSPTKSQDQTGSGWNFCSIP
WCLFWATVLLLIIYLQFSFRSSV
>hg38.ACE2.prot
MSSSSWLLLSLVAVTAAQSTIEEQAKTFLDKFNHEAEDLFYQSSLASWNYNTNITEENVQ
NMNNAGDKWSAFLKEQSTLAQMYPLQEIQNLTVKLQLQALQQNGSSVLSEDKSKRLNTIL
NTMSTIYSTGKVCNPDNPQECLLLEPGLNEIMANSLDYNERLWAWESWRSEVGKQLRPLY
EEYVVLKNEMARANHYEDYGDYWRGDYEVNGVDGYDYSRGQLIEDVEHTFEEIKPLYEHL
HAYVRAKLMNAYPSYISPIGCLPAHLLGDMWGRFWTNLYSLTVPFGQKPNIDVTDAMVDQ
AWDAQRIFKEAEKFFVSVGLPNMTQGFWENSMLTDPGNVQKAVCHPTAWDLGKGDFRILM
CTKVTMDDFLTAHHEMGHIQYDMAYAAQPFLLRNGANEGFHEAVGEIMSLSAATPKHLKS
IGLLSPDFQEDNETEINFLLKQALTIVGTLPFTYMLEKWRWMVFKGEIPKDQWMKKWWEM
KREIVGVVEPVPHDETYCDPASLFHVSNDYSFIRYYTRTLYQFQFQEALCQAAKHEGPLH
KCDISNSTEAGQKLFNMLRLGKSEPWTLALENVVGAKNMNVRPLLNYFEPLFTWLKDQNK
NSFVGWSTDWSPYADQSIKVRISLKSALGDKAYEWNDNEMYLFRSSVAYAMRQYFLKVKN
QMILFGEEDVRVANLKPRISFNFFVTAPKNVSDIIPRTEVEKAIRMSRSRINDAFRLNDN
SLEFLGIQPTLGPPNQPPVSIWLIVFGVVMGVIVVGIVILIFTGIRDRKKKNKARSGENP
YASIDISKGENNPGFQNTDDVQTSF" \
  --data-urlencode "db=${asmId}" \
  --data-urlencode "type=protein" \
  --data-urlencode "output=json" \
    https://genome-test.gi.ucsc.edu/cgi-bin/hgBlat

cat $asmId.json | python -mjson.tool > $asmId.json.txt
