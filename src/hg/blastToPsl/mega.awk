/^Database:/ {
       # save 2 database lines in variables
     db=$0 ; getline db2 
   }
/^Query=/ {
       # output 2 query lines, a blank line and then database lines.
      getline q2; print $0"\n",q2"\n\n"db"\n"db2
   }  
!/^Query=/{print $0}
