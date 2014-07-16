# UCSC Browserbox install helper script
display dialog "UCSC Browserbox network config helper: This program adds an entry 'ucsc.local' to your network configuration (/etc/hosts) and directs port 1234 to it. To do this, you will need the adminstrator password"
do shell script "if grep -q ucsc.local /etc/hosts ; then true; else echo 127.0.0.1 genome.ucsc.local; fi >> /etc/hosts" with administrator privileges
# flush the DNS cache to activate the localhost entry
do shell script "dscacheutil -flushcache"

# setup a plist file to create firewall rule on every reboot that redirects local host 1234 to port 80
set plistStr to "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC -//Apple Computer//DTD PLIST 1.0//EN http://www.apple.com/DTDs/PropertyList-1.0.dtd >
<plist version=\"1.0\">
<dict>
    <key>Label</key>
    <string>edu.ucsc.genome.portRedir</string>
    <key>ProgramArguments</key>
    <array>
        <string>/sbin/ipfw</string>
        <string>add</string>
	 <string>100</string>
      <string>fwd</string>
	<string>127.0.0.1,1234</string>
	<string>tcp</string>
	<string>from</string>
	<string>any</string>
    <string>to</string>
    <string>me</string>
    <string>80</string>
	</array>
    <key>RunAtLoad</key>
    <true/>
    <key>Nice</key>
    <integer>10</integer>
    <key>KeepAlive</key>
    <false/>
    <key>AbandonProcessGroup</key>
    <true/>
</dict>
</plist>
"
# create the plist file for the firewall rule
do shell script "echo  " & quoted form of plistStr & " >  /System/Library/LaunchDaemons/edu.ucsc.genome.portRedir.plist" with administrator privileges

# activate the plist file now
do shell script "launchctl load /System/Library/LaunchDaemons/edu.ucsc.genome.portRedir.plist" with administrator privileges

#do shell script "ipfw add 100 fwd 127.0.0.1,1234 tcp from any to me 80" with administrator privileges
display dialog "Added entry for genome.ucsc.local to hosts file. Redirected port 80 to port 1234, by creating the file /System/Library/LaunchDaemons/edu.ucsc.genome.portRedir.plist which is run on every reboot" buttons "OK" default button "OK"
