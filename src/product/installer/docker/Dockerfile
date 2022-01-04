# UCSC Genome Browser Website Dockerfile
FROM phusion/baseimage:focal-1.0.0
MAINTAINER Maximilian Haeussler <max@soe.ucsc.edu>
ADD http://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/browserSetup.sh /root/browserSetup.sh

# for debugging, it's usually best to uncomment these lines and comment out the next two lines
#ADD mysql.service /etc/service/mysqld/run
#ADD apache.service /etc/service/apache/run
ADD https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/apache.service /etc/service/apache/run
ADD https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/mysql.service /etc/service/mysqld/run

# Upgrade the base image using the Ubuntu repos
RUN apt-get update && apt-get upgrade -y -o Dpkg::Options::="--force-confold"
# allow the udw command, install the genome browser and clean up
RUN apt-get install -yq wget rsync && sed -i 's/101/0/g' /usr/sbin/policy-rc.d && apt-get install udev || sed -i '2iexit 0' /etc/init.d/udev && service udev start && chmod a+x /root/browserSetup.sh && /root/browserSetup.sh -b install && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* && chmod a+x /etc/service/mysqld/run /etc/service/apache/run
# switch off STRICT_TRANS_TABLES, which is default now. The genome browser has a few places where it does not specify all
# values in an insert statement, which is not allowed in strict mode
RUN sed -i '/^.mysqld.$/a sql_mode=' /etc/mysql/mariadb.conf.d/50-server.cnf 
#&& /root/browserSetup.sh addTools
#CMD /etc/init.d/apache2 start && /etc/init.d/mysql start
EXPOSE 80
CMD ["/sbin/my_init"]
