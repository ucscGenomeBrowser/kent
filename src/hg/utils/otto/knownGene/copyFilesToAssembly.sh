# expects one argument which is the list of tables
for i in `cat $1`; 
do 
echo  "create table $i like $2.$i;" 
echo  "insert into $i select * from $2.$i;" 
done
