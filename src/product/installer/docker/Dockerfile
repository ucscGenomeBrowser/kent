# UCSC Genome Browser Website Dockerfile
FROM phusion/baseimage:latest
MAINTAINER Maximilian Haeussler <max@soe.ucsc.edu>
ADD http://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/browserSetup.sh /root/browserSetup.sh
ADD mysql.service /etc/service/mysqld/run
ADD apache.service /etc/service/apache/run

# Upgrade the base image using the Ubuntu repos
RUN apt-get update && apt-get upgrade -y -o Dpkg::Options::="--force-confold"
# allow the udw command, install the genome browser and clean up
RUN apt-get install -yq wget rsync && sed -i 's/101/0/g' /usr/sbin/policy-rc.d && apt-get install udev || sed -i '2iexit 0' /etc/init.d/udev && service udev start && chmod a+x /root/browserSetup.sh && /root/browserSetup.sh -b install && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* && chmod a+x /etc/service/mysqld/run /etc/service/apache/run
# suppress errors when values are not set and table has no default
RUN sed -i '/^.mysqld.$/a sql_mode=' /etc/mysql/mysql.conf.d/mysqld.cnf
#&& /root/browserSetup.sh addTools
#CMD /etc/init.d/apache2 start && /etc/init.d/mysql start
EXPOSE 80
CMD ["/sbin/my_init"]
