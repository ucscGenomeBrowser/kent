echo adding fast jobs
foreach h (0 1 2 3 4 5 6 7 8 9)
    foreach i (0 1 2 3 4 5 6 7 8 9)
       foreach j (0 1 2 3 4 5 6 7 8 9)
	  foreach k (0 1 2 3 4 5 6 7 8 9)
	      parasol -out=res/$h$i$j$k add job echo $h $i $j $k
	      parasol add job sleep 1
	  end
          echo -n .
      end
   end
end
echo ""
