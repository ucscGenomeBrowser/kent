mkdir manySize
echo adding fast jobs
foreach h (0 1 2 3 4)
    foreach i (0 1 2 3 4 5 6 7 8 9)
       foreach j (0 1 2 3 4 5 6 7 8 9)
	  foreach k (0 1 2 3 4 5 6 7 8 9)
	      parasol -out=manySize/$h$i$j$k add job faSize /scratch/hg/mrna.127/mrna.fa
	  end
          echo -n .
      end
   end
end
echo ""
