set outdir = $1
set dir = /cluster/home/kate/kent/src/hg/lib
foreach f ($outdir/gtexEqtlTissue*.bed)
    if (-z $f) then
        echo "empty file $f"
        continue;
    endif
    set track = $f:t:r
    echo $track
    hgLoadBed hg19 \
        -tab -allowStartEqualEnd \
        -type=bed9+7 -as=$dir/gtexEqtl.as -sqlTable=$dir/gtexEqtl.sql -renameSqlTable \
            $track $outdir/$track.bed
end
