#!/bin/csh

foreach chrom ( 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y )

    echo
    echo starting $chrom on `date`

    gunzip ds_ch$chrom.xml.gz
    echo   ds_ch$chrom.xml.gz unzipped on `date`

    getObsHet < ds_ch$chrom.xml > chN.observed/ch$chrom.obs
    echo wrote ch$chrom.obs on `date`

    gzip ds_ch$chrom.xml
    mv   ds_ch$chrom.xml.gz done

    echo finished $chrom at `date`

end

cat chN.observed/*.obs > chN.observed/observed

echo starting dbSnp at `date`
dbSnp hg13 chN.observed/observed   dbSnpOutputHg13
echo finished dbSnp at `date`
