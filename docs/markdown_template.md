% A sample Markdown page

[comment]: <> (QA: This is a comment, it will not appear in the HTML)

# Overview of the Genome Browser

The genome browser requires humans:

- sample unnumbered list line 1
- item 2 and `backticks`

# Requirements

A bulleted list:

* There
* Here
* 'with links' map plotting tools <http://www.soest.hawaii.edu/gmt/>
* Redhat/Fedora/CentOS: `yum install libpng12 httpd ghostscript GMT hdf5 R libuuid-devel MySQL-python`

A command:

    rsync -hna --stats rsync://hgdownload.soe.ucsc.edu/gbdb/ | egrep "Number of files:|total size is"

A list and code under it:

1. Topic 1

        XBitHack on
        <Directory /usr/local/apache/htdocs>
        Options +Includes
        </Directory>
 
2. Topic 2

       mysql -u browser -pgenome -e 'show tables;' mysql

    MariaDB can be run from the command line, and
    the tables from the database MariaDB can be displayed.

