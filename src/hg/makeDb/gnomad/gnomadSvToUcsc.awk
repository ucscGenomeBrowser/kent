#!/usr/bin/awk -f
# turn a gnomad SV file to ucsc bigBed 9+

BEGIN {
    FS="\t";
    OFS="\t";
}

{
chrom=$1
start=$2
end=$3
origName=$4

PROTEIN_CODING__COPY_GAIN=$5
PROTEIN_CODING__DUP_LOF=$6
PROTEIN_CODING__DUP_PARTIAL=$7
PROTEIN_CODING__INTERGENIC=$8
PROTEIN_CODING__INTRONIC=$9
PROTEIN_CODING__INV_SPAN=$10
PROTEIN_CODING__LOF=$11
PROTEIN_CODING__MSV_EXON_OVR=$12
PROTEIN_CODING__NEAREST_TSS=$13
PROTEIN_CODING__PROMOTER=$14
PROTEIN_CODING__UTR=$15

# size of NA when variant is a breakend or something else
if ($16 > 0)
    svlen=$16
else
    svlen = "NA"
svtype=$17
an=$18
ac=""
af=""
nhet=$21
nhomalt=$22

split($19,acArray,",")
for (i = 1; i < length(acArray) - 1; i++)
    {
    ac = ac "" acArray[i] ", "
    }
ac = ac "" acArray[length(acArray)]

split($20,afArray,",")
for (i = 1; i< length(afArray) - 1; i++)
    {
    af = af "" sprintf("%0.2g, ", afArray[i])
    }
af = af "" sprintf("%0.2g", afArray[length(afArray)])

split($4,a,"_")
name=a[3] "_" a[4] "_" a[5]
color=""
mouseOver = "Position: " chrom ":" start+1 "-" end ", Size: " svlen ", Class: " svtype ", Allele Count: " ac ", Allele Number: " an ", Allele Frequency: " af

switch(svtype) {
    case "BND":
        color = "154,182,160"
        break
    case "CPX":
        color = "182,239,195"
        break
    case "CTX":
        color = "154,182,160"
        break
    case "DEL":
        color = "231,154,144"
        break
    case "DUP":
        color = "143,184,214"
        break
    case "INS":
        color = "231,183,237"
        break
    case "INV":
        color = "250,199,140"
        break
    case "MCNV":
        color = "183,170,214"
        break
    default:
        color = "154,182,160"
        break
}

print chrom, start, end, name, 0, ".", start, end, color, svlen, svtype, ac, an, af, nhet, nhomalt, PROTEIN_CODING__COPY_GAIN, PROTEIN_CODING__DUP_LOF, PROTEIN_CODING__DUP_PARTIAL, PROTEIN_CODING__INTERGENIC, PROTEIN_CODING__INTRONIC, PROTEIN_CODING__INV_SPAN, PROTEIN_CODING__LOF, PROTEIN_CODING__MSV_EXON_OVR, PROTEIN_CODING__NEAREST_TSS, PROTEIN_CODING__PROMOTER, PROTEIN_CODING__UTR, mouseOver
}
