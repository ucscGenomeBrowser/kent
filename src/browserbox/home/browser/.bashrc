# ~/.bashrc: executed by bash(1) for non-login shells.
# see /usr/share/doc/bash/examples/startup-files (in the package bash-doc)
# for examples

# If not running interactively, don't do anything
case $- in
    *i*) ;;
      *) return;;
esac

# don't put duplicate lines or lines starting with space in the history.
# See bash(1) for more options
HISTCONTROL=ignoreboth

# append to the history file, don't overwrite it
shopt -s histappend

# for setting history length see HISTSIZE and HISTFILESIZE in bash(1)
HISTSIZE=1000
HISTFILESIZE=2000

# check the window size after each command and, if necessary,
# update the values of LINES and COLUMNS.
shopt -s checkwinsize

# If set, the pattern "**" used in a pathname expansion context will
# match all files and zero or more directories and subdirectories.
#shopt -s globstar

# make less more friendly for non-text input files, see lesspipe(1)
[ -x /usr/bin/lesspipe ] && eval "$(SHELL=/bin/sh lesspipe)"

# set variable identifying the chroot you work in (used in the prompt below)
if [ -z "${debian_chroot:-}" ] && [ -r /etc/debian_chroot ]; then
    debian_chroot=$(cat /etc/debian_chroot)
fi

# set a fancy prompt (non-color, unless we know we "want" color)
case "$TERM" in
    xterm-color) color_prompt=yes;;
esac

# uncomment for a colored prompt, if the terminal has the capability; turned
# off by default to not distract the user: the focus in a terminal window
# should be on the output of commands, not on the prompt
#force_color_prompt=yes

if [ -n "$force_color_prompt" ]; then
    if [ -x /usr/bin/tput ] && tput setaf 1 >&/dev/null; then
	# We have color support; assume it's compliant with Ecma-48
	# (ISO/IEC-6429). (Lack of such support is extremely rare, and such
	# a case would tend to support setf rather than setaf.)
	color_prompt=yes
    else
	color_prompt=
    fi
fi

if [ "$color_prompt" = yes ]; then
    PS1='${debian_chroot:+($debian_chroot)}\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ '
else
    PS1='${debian_chroot:+($debian_chroot)}\u@\h:\w\$ '
fi
unset color_prompt force_color_prompt

# If this is an xterm set the title to user@host:dir
case "$TERM" in
xterm*|rxvt*)
    PS1="\[\e]0;${debian_chroot:+($debian_chroot)}\u@\h: \w\a\]$PS1"
    ;;
*)
    ;;
esac

# enable color support of ls and also add handy aliases
if [ -x /usr/bin/dircolors ]; then
    test -r ~/.dircolors && eval "$(dircolors -b ~/.dircolors)" || eval "$(dircolors -b)"
    alias ls='ls --color=auto'
    #alias dir='dir --color=auto'
    #alias vdir='vdir --color=auto'

    alias grep='grep --color=auto'
    alias fgrep='fgrep --color=auto'
    alias egrep='egrep --color=auto'
fi

# some more ls aliases
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'

# Add an "alert" alias for long running commands.  Use like so:
#   sleep 10; alert
alias alert='notify-send --urgency=low -i "$([ $? = 0 ] && echo terminal || echo error)" "$(history|tail -n1|sed -e '\''s/^\s*[0-9]\+\s*//;s/[;&|]\s*alert$//'\'')"'

# Alias definitions.
# You may want to put all your additions into a separate file like
# ~/.bash_aliases, instead of adding them here directly.
# See /usr/share/doc/bash-doc/examples in the bash-doc package.

if [ -f ~/.bash_aliases ]; then
    . ~/.bash_aliases
fi

# enable programmable completion features (you don't need to enable
# this, if it's already enabled in /etc/bash.bashrc and /etc/profile
# sources /etc/bash.bashrc).
if ! shopt -oq posix; then
  if [ -f /usr/share/bash-completion/bash_completion ]; then
    . /usr/share/bash-completion/bash_completion
  elif [ -f /etc/bash_completion ]; then
    . /etc/bash_completion
  fi
fi

# CHANGES IN BROWSERBOX

export PATH=~/bin:~/bin/blat:$PATH:.

# for a VM this is very useful
if [ `tty` = '/dev/tty1' ]; then
	setterm -powersave off -blank 0;
fi

# useful aliases when working with the gbib
alias sqlUcsc='mysql -h genome-mysql.soe.ucsc.edu -u genomep --password=password -A hg19'
alias gbibSlowNet='sudo tc qdisc add dev eth0 root netem delay 100ms'
alias gbibFastNet='sudo tc qdisc del dev eth0 root netem delay 100ms'
alias mail='alpine'
alias pine='alpine'
# switch auto updates on/off
alias gbibAutoUpdateOff='sudo touch /root/noAutoUpdate; echo auto updates are off'
alias autoUpdateOff=gbibAutoUpdateOff
alias gbibAutoUpdateOn='sudo rm -f /root/noAutoUpdate; echo auto updates are on'
alias autoUpdateOn=gbibAutoUpdateOn
# rm and zero the disk
alias rmIt='sudo shred -n 1 -zu'
# update the update script
alias gbibCoreUpdate='sudo wget http://genome-test.soe.ucsc.edu:/browserbox/updateBrowser.sh -O /root/updateBrowser.sh; sudo chmod a+x /root/updateBrowser.sh'
alias gbibCoreUpdateBeta='sudo wget http://hgwdev.soe.ucsc.edu:/gbib/updateBrowser.sh -O /root/updateBrowser.sh; sudo chmod a+x /root/updateBrowser.sh'

# repair all tables, first while mysql is running, then with a full server stop
alias gbibFixMysql1='sudo mysqlcheck --all-databases --auto-repair --fast'
alias gbibFixMysql2='sudo service mysql stop; sudo myisamchk --force --silent --update-state /data/mysql/*/*.MYI; sudo service mysql start'

alias gbibResetNetwork='sudo ifup --force eth0'

# show tables that had to be loaded from UCSC since last reboot
alias gbibUcscTablesLog='sudo cat /var/log/apache2/error.log | grep -- "->" | grep -o "from [^ ]*" | cut -d" " -f2 | sort | uniq -c | sort -nr | less'
alias gbibUcscTablesReset='sudo echo | sudo dd of=/var/log/apache2/error.log; sudo service apache2 reload'
# show gbdb files that had to be loaded from UCSC since last reboot
alias gbibUcscGbdbLog='sudo find /data/trash/udcCache/http/hgdownload.soe.ucsc.edu/gbdb/ -type f | egrep -v "bitmap"  | sed -e "s/\/sparseData//" | sed -e "s#/data/trash/udcCache/http/hgdownload.soe.ucsc.edu/##"'
alias gbibUcscGbdbReset='rm -rf /data/trash/udcCache/http/hgdownload.soe.ucsc.edu/*'

# remove or add the ucsc mysql server for testing
alias gbibOffline='sudo sed -i "s/^slow-db/#slow-db/" /usr/local/apache/cgi-bin/hg.conf; sudo sed -i "s/^showTableCache/#showTableCache/" /usr/local/apache/cgi-bin/hg.conf; gbibAutoUpdateOff; echo remote access to UCSC off.'
alias gbibOnline='sudo sed -i "s/^#slow-db/slow-db/" /usr/local/apache/cgi-bin/hg.conf; sudo sed -i "s/^#showTableCache/showTableCache/" /usr/local/apache/cgi-bin/hg.conf; gbibAutoUpdateOn; echo remote access to UCSC on.'
alias boxOffline=gbibOffline
alias boxOnline=gbibOnline

# enable or disable hgMirror support on this VM
alias gbibMirrorTracksOff='sudo sed -i /allowHgMirror/d /usr/local/apache/cgi-bin/hg.conf.local; sudo bash -c "echo allowHgMirror=false >> /usr/local/apache/cgi-bin/hg.conf.local"'
alias gbibMirrorTracksOn='sudo sed -i /allowHgMirror/d /usr/local/apache/cgi-bin/hg.conf.local; sudo bash -c "echo allowHgMirror=true >> /usr/local/apache/cgi-bin/hg.conf.local"'

# for consistency
alias gbibUpdate='/home/browser/updateBrowser'
alias sshNewKey='shred -n 1 -zu /home/browser/.ssh/dsa*; cat /dev/zero | ssh-keygen -t dsa -N ""'
alias gbibUcscLog='sudo tail -f /var/log/apache2/error.log'
alias og='ls -ogrt'

# get the kent tools
alias gbibAddTools='sudo mkdir -p /data/tools; sudo rsync -avP hgdownload.soe.ucsc.edu::genome/admin/exe/linux.x86_64/ /data/tools/ && ln -s /data/tools ~/bin'

function trackSize() { rsync -hvn  hgdownload.soe.ucsc.edu::mysql/hg19/$1.* ./ ; }

if [ `tty` == "/dev/tty1" -a ! -e /root/noAutoUpdate ]; then
    sudo /root/updateBrowser.sh
fi

alias og='ls -ogrt'
