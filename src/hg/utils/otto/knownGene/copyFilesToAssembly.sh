# expects one argument which is the list of tables
for i in `cat $1`; 
do 
echo  "create table $i like knownGeneVM27.$i;" 
echo  "insert into $i select * from knownGeneVM27.$i;" 
done
