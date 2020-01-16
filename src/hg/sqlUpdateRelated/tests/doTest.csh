#!/bin/tcsh -efx
set db = kentXDjTstWhyYDropMe
hgsqladmin --force drop $db
hgsqladmin create $db
hgsql $db < inDb.sql
sqlUpdateRelated -uncsv $db hcat_contributor.tsv hcat_lab.tsv hcat_publication.tsv hcat_organ.tsv hcat_organpart.tsv hcat_assaytype.tsv hcat_disease.tsv hcat_project.tsv 
hgsqldump $db | grep -v "Dump completed" >outDb.sql
diff expectedDb.sql outDb.sql
