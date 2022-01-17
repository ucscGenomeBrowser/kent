# expects one argument which is the list of tables
for i in `cat $1`; 
do 
echo  "create table $i like knownGeneV38.$i;" 
echo  "insert into $i select * from knownGeneV38.$i;" 
done
