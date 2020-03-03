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
svtype=$5

split($4,a,"_")
name=a[3] "_" a[4] "_" a[5]
color=""

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

print chrom, start, end, name, 0, ".", start, end, color, svtype
}
