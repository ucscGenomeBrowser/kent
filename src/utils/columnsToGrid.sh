#!/usr/bin/env bash

#################################################################################
# Purpose
#################################################################################
# In bioinformatics we deal with the lowest common denominator format. We want something that is easily readable by all computer programs: usually this means the data is stored as plain text (ASCII) organized into rows and columns, often with tabs delimiting the columns. While this is very easy for a computer to parse, sometimes it's a bit confusing for us to read and interpret. 
# 
# Tag storms are one way of overcoming this challenge: they are easy for computers to parse, reduce the redundancy of a tab-separated file, and they are easily human readable. However, in other aspects of our work we are still largely stuck with text files organized into rows and columns.
#
# We stare at these files all day, so visually organizing them for a quick look has been a huge time saver for me (and prevented a lot of simple mistakes). Without tools like this, we'd have to perform a tedious series of cut commands or traverse our screen doing a lot of counting of columns, trying to figure out how blocks of text map. 
#
# This program allows you to view data as a grid, and allows you to define which delimiter you'd like to use (tab is used by default).

#################################################################################
# Usage
#################################################################################
# Run program with no arguments for usage. 

#################################################################################
# Limitations/to do
#################################################################################
# I want to truncate columns, probably using something like sed 's/\(.\{1,50\}\).*/\1/' this way there's no wrapping around the terminal (in which case, columns is a better program)
# Some users of older shells report issues. Detect if user is running bash/zsh, otherwise exit.

#################################################################################
# Configuration
#################################################################################
    reset=$(echo -en "\e[0m")
    # Some of my commonly-used background colors:
    bg25=$(echo -en "\e[48;5;25m")
    bg107=$(echo -en "\e[48;5;107m")
    bg117=$(echo -en "\e[48;5;117m")
    bg196=$(echo -en "\e[48;5;196m")
    bg201=$(echo -en "\e[48;5;201m")
    bg202=$(echo -en "\e[48;5;202m")
    bg240=$(echo -en "\e[48;5;240m")
    # Some of my commonly-used foreground colors:
    color25=$(echo -en "\e[38;5;25m")
    color107=$(echo -en "\e[38;5;107m")
    color117=$(echo -en "\e[38;5;117m")
    color196=$(echo -en "\e[38;5;196m")
    color201=$(echo -en "\e[38;5;201m")
    color202=$(echo -en "\e[38;5;202m")
    color240=$(echo -en "\e[38;5;240m")

#################################################################################
# Handle stdin
#################################################################################
if [ -t 0 ]; then

    #################################################################################
    # If no file is found, display help			    #
    #################################################################################
    if [ -z "$1" ] || [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
	echo
	echo "Usage:"
	echo
	echo "	${color117}columnsToGrid.sh$color107 file.txt $color25'delimiter'$reset"
	echo
	echo "	${color240}cat ${color107}file.txt | ${color117}columnsToGrid.sh$color107 stdin $color25'delimiter'$reset"
	echo
	echo "$color240 (If no delimiter is specified, defaults to tab)$reset"
	echo
	echo "	Particularly useful for data wrangling to see how fields will be cut apart, for example:"
	echo
	echo "	    ${color240}find directory/ -type f | grep fast.gz | ${color117}columnsToGrid.sh$color107 stdin$color25 \$'\/'$reset"
	echo
	exit 0;
    else
	#########################################################################
	# Otherwise, a file was set.			    #
	#########################################################################   
	FILE=$1
	if [ -a "$FILE" ]; then
	    printf ""
	else
	    #################################################################################
	    # Handle files that aren't found with grace.
	    #################################################################################
		printf "\n"
		echo " ╔════════════════════════════════════════════════════════════════════════════════╗"
		echo "	 The file ${color202}$FILE$reset doesn't appear to exist."
		echo ""

			caseSensitive=$(shopt nocaseglob)
			if [ "$caseSensitive" = "nocaseglob	    off" ]; then
				shopt -s nocaseglob; { sleep 3 && shopt -u nocaseglob & };
			fi

			similarfiles=$( ls -d ${FILE:0:1}* | sort | wc -l )
			echo "	Perhaps you intended to look at one of the following $bg25$similarfiles$reset files? "
		echo "	(setting case insensitive for an instant)" 
			echo ""
			echo "		      $ ls -d ${FILE:0:1}*$color25"
			# some directories have a ton of files that may match, like srr*, so let's split this up to avoid a flood.
			if [ "$similarfiles" -lt 20 ]; then
				ls -d ${FILE:0:1}* | sort | sed "s/^/		     /"
			else
				ls -d ${FILE:0:1}* | sort | sed "s/^/		     /" | head -15
				echo ""
				echo "		      $white[ ... ]$color25"
				echo ""
				ls -d ${FILE:0:1}* | sort | sed "s/^/		     /" | tail -15
			fi
			echo ""
			echo "$reset"

		echo " ╚════════════════════════════════════════════════════════════════════════════════╝"
		printf "\n"
		exit 0
	fi  
    fi
else
    FILE=stdin
    while IFS= read -r line; do
		 LINES="$LINES 
$line"; done

fi


    echo 
	#########################################################################
	# Make a custom border				#
	#########################################################################
	wall() {
	    border=1
	    WALL=
	    WINDOW=$(tput cols)
	    while [ "$border" -lt "$WINDOW" ]; do
		WALL="=$WALL";
		((border++))
	    done
	    export WALL="$reset$color240$WALL$reset"
	}

	arrange() {
	    wall
	    if [ -z "$2" ] || [ "$2" == "tab" ]; then 
		delimiter=$'\t'
		aligningOn="tab"
	    else
		delimiter=$2
		aligningOn=$2
	    fi
	    echo "$reset${color240}Aligned with $reset$bg25 $aligningOn $reset ${color240}as delimiter.$reset"
	    echo $WALL
	    if [ "$FILE" == "stdin" ]; then
		while read line; do 
		 sed "s/$delimiter/${delimiter}CUTMEOUT/g" | sed "s/$delimiter$delimiter/$delimiter.$delimiter/g" | sed "s/.$delimiter$delimiter/.$delimiter.$delimiter/g" | sed "s/$delimiter$/$delimiter./g" | sed "s/.$delimiter$/$delimiter./g" | sed "s/^$delimiter/.$delimiter/g" | column -ts $"$delimiter"
		done
	    else
		cat $1 | sed "s/$delimiter/${delimiter}CUTMEOUT/g" | sed "s/$delimiter$delimiter/$delimiter.$delimiter/g" | sed "s/.$delimiter$delimiter/.$delimiter.$delimiter/g" | sed "s/$delimiter$/$delimiter./g" | sed "s/.$delimiter$/$delimiter./g" | sed "s/^$delimiter/.$delimiter/g" | column -ts $"$delimiter"
	    fi
	}
	makeGrid() {
	#   This would give us far more colors, but some of them are difficult to read.
	#   BASECOLORS="117 202 106 196 25 201 240 99 22 210 81 203 105"; FULLCOLORS=$(shuf -i 17-240 ); array=( $(echo "$BASECOLORS" | sed -r 's/(.[^;]*;)/ \1 /g' | tr " " "\n" | shuf | tr -d " "; echo " $FULLCOLORS" | tr '\n' ' ' | sed -r 's/(.[^;]*;)/ \1 /g' | tr " " "\n" | shuf | tr -d " "	) )
	    BASECOLORS="117 202 106 196 25 201 240 99 22 210 81 203 105"; array=( $(echo "$BASECOLORS" | sed -r 's/(.[^;]*;)/ \1 /g' | tr " " "\n" | shuf | tr -d " "; ) )
	    if [ -z "$1" ]; then fgbg=38; else fgbg=48; fi
	    while read line; do 
		color=${array[z]}
		lastColor=$( echo ${array[${#array[@]}-1]} )
		if [ "$lastColor" == "$color" ]; then 
		    array=( $(echo "$BASECOLORS" | sed -r 's/(.[^;]*;)/ \1 /g' | tr " " "\n" | shuf | tr -d " "; ) )
		    printf ""
		    z=0
		fi
		if [ -z "$alternate" ]; then 
		    alternate=0; 
		else 
		((alternate++)); 
		fi; 
		if [ $((alternate%2)) -eq 0 ]; then 
		    alternateRow=$(echo -en "\e[${fgbg};5;${color}m"); 
		    COLOR=`echo -e "\e[48;5;${color}m"`
		    NORMAL=`echo -e '\033[0m'`
		    griddedLine=$(echo "$line" | sed "s/CUTMEOUT/$NORMAL $COLOR/g" ) # sed  "s/\([[:blank:]]\+\)\(.*\)/\1\\\e[48;5;${color}m\2/g" )
		    ((z++)); 
		else 
		    alternateRow= #$(echo -en "\e[38;5;255m") ; 
		    griddedLine=$(echo "$line" | sed "s/CUTMEOUT/ /g")
		fi
		echo "$reset$alternateRow$griddedLine$reset";
	    done
	    echo "$WALL"
	}

    if [ "$FILE" == "stdin" ]; then
	echo "$LINES" | arrange $FILE $2 | makeGrid x
    else
	arrange $FILE $2 | makeGrid x
    fi
    
