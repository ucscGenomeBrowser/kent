if (! -e mix) then
    mkdir mix
endif
foreach k (0 1 2 3 4 5 6 7 8 9)
    parasol -out=mix/echo$k add job /bin/echo $k
    parasol -out=mix/$k add job faSize /scratch/hg/mrna.127/mrna.fa
    parasol add job sleep 10
end
