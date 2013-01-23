foreach f (wgEncode*.ra)
    set c = $f:r
    echo "      $c"
    ~kate/bin/x86_64/mdbPrint hg19 -composite=$c > new.ra
    diff new.ra $f
end
