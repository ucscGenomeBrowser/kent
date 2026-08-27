NOTE: cirmdcm.soe.ucsc.edu, the host this gateway ran on, was retired in 2026 and
no longer answers on port 80 or 443. Nothing described here is deployed anywhere
now. These files are kept as a record of how the gateway was built, since the
same setup would be needed again if a private server ever comes back.

This directory contains the Apache Reverse Proxy configuration that sets up a HTTPS gateway 
for the data warehouse. An https proxy or a VPN is required by HHS/HIPPA rules. The HTTPS
gateway is an easy alternative for users to access the machine, much simpler than the VPN.

- first, deactivate all sites on the host with a2dissite. 
- copy the file proxy-host.conf into /etc/apache2/sites-available/ 
- enable it with via a2ensite proxy-host
- edit the config file and adapt the target host to forward to
- mkdir /etc/cirm and copy over the SSL certificates
- create the users that are allowed in with 'sudo htpasswd -c /etc/apachePasswords max'
- restart apache with 'sudo /etc/init.d/apache2 restart'
