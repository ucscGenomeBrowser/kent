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
