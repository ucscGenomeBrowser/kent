# UCSC Genome Browser Website Dockerfile using Ubuntu 24 Noble
FROM phusion/baseimage:noble-1.0.2
LABEL org.opencontainers.image.authors="mhaeussl@ucsc.edu"
# The following is required as this is a "fat" docker image with an initd, running daemons like sshd and crond
# See phusion documentation
CMD ["/sbin/my_init"]

# Upgrade the base image using the Ubuntu repos and add wget,rsync
RUN apt-get update && apt-get upgrade -y -o Dpkg::Options::="--force-confold" && apt-get install -yq wget rsync

# Run mysqld and apache via initd, the Ubuntu startup system does not work in containers
ADD https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/apache.service /etc/service/apache/run
# actually mariadb, but the distros have mysqld symlinks, no need to rename
ADD https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/docker/mysql.service /etc/service/mysqld/run
RUN chmod a+x /etc/service/mysqld/run /etc/service/apache/run 

# Install the genome browser
ADD http://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/product/installer/browserSetup.sh /root/browserSetup.sh
RUN chmod a+x /root/browserSetup.sh && /root/browserSetup.sh -b install 

# Phusion documentation suggests this
RUN apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

EXPOSE 80
