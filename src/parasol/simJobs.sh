if (! -e res) then
    mkdir res
endif
echo adding sleepy jobs
foreach i (0 1 2 3 4 5 6 7 8 9)
    parasol add job sleep 100
end

echo adding fast jobs
foreach h (0 1 2 3)
    foreach i (0 1 2 3 4 5 6 7 8 9)
       foreach j (0 1 2 3 4 5 6 7 8 9)
	  foreach k (0 1 2 3 4 5 6 7 8 9)
	      parasol -out=res/$h$i$j$k add job echo $h $i $j $k
	      echo -n .
	 end
      end
   end
end
echo ""
