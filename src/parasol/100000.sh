echo adding fast jobs
foreach d (0 1 2 3 4 5 6 7 8 9)
mkdir res$d
foreach h (0 1 2 3 4 5 6 7 8 9)
    foreach i (0 1 2 3 4 5 6 7 8 9)
       foreach j (0 1 2 3 4 5 6 7 8 9)
	  foreach k (0 1 2 3 4 5 6 7 8 9)
	      parasol -out=res$d/$h$i$j$k add job echo $h $i $j $k
	      parasol add job sleep 10
	  end
          echo -n .
      end
   end
end
end
echo ""
