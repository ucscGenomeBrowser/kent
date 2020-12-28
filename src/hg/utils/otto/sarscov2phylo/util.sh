#!/bin/bash

# Define some handy functions for other bash scripts in this directory

xcat () {
    fasta=$1
    if [ "${fasta##*.}" == "xz" ]; then
        xzcat $fasta
    elif [ "${fasta##*.}" == "gz" ]; then
        zcat $fasta
    else
        cat $fasta
    fi
}
export -f xcat

fastaNames () {
    xcat $1 \
    | grep ^\> | sed -re 's/^>//;'
}
export -f fastaNames

fastaSeqCount () {
    xcat $1 \
    | grep ^\> | wc -l
}
export -f fastaSeqCount

cleanGenbank () {
    sed -re "s@Severe acute respiratory syndrome coronavirus 2 isolate SARS[ -]Co[Vv]-2/(human|homo ?sapiens)/@@;
             s@Severe acute respiratory syndrome coronavirus 2 SARS-CoV-2/@@;
             s@Mutant Severe acute respiratory syndrome coronavirus 2 clone SARS-CoV-2[_-]@@;
             s@Severe acute respiratory syndrome coronavirus 2( isolate)?( 2019_nCoV)?@@;
             s@[A-Za-z0-9]+ [a-z]*protein.*@@;
             s@(( genomic)? RNA)?, ((nearly )?complete|partial) genome\$@@;
             s@genome assembly(, complete genome)?: monopartite\$@@;
             s@ (1 |nasopharyngeal )?genome assembly, chromosome: .*\$@@;
             s@, complete sequence@@;
             s@hCo[vV]-19/@@;
             s@SARS?-CoV-?2/([Hh]umai?ns?|[Hh]o[mw]o ?sapiens?)/@@;
             s@SARS-CoV-2/(environment|ENV)/@env/@;
             s@SARS-CoV-2/Felis catus/@cat/@;
             s@SARS-CoV-2/Panthera leo/@lion/@;
             s@SARS-CoV-2/Panthera tigris/@tiger/@;
             s@SARS-CoV-2/@@;
             s@BetaCoV/@@;
             s@Homo sapines/@@;
             s@ \| @ \|@; s@ \$@@; s@  @ @;
             s@ \|@\t@;"
# Got rid of this:   s/ ([^|])/_\1/g;
}
export -f cleanGenbank

cleanCncb () {
    sed -re "s@^BetaCoV/@@;
             s@^hCoV-19/@@;
             s@^SARS-CoV-2/@@;
             s@^human/@@;
             s@ *\| *@\t@;"
}
export -f cleanCncb

