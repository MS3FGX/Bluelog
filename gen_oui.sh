#!/bin/bash
# Generate OUI list for libmackerel
VER="1.2"

# Location of tmp file
TMPFILE="/tmp/oui.tmp"

# File to write
OUIFILE="oui.txt"
#------------------------------------------------------------------------------#
ErrorMsg ()
{
[[ $(expr substr "$2" 1 1) == "n" ]] && echo
if [ "$1" == "ERR" ]; then
	echo "  ERROR: ${2#n}"
	exit 1
elif [ "$1" == "WRN" ]; then
	echo "  WARNING: ${2#n}"
fi
exit 1
}

get_oui ()
{
echo -n "Downloading OUI file from IEEE..."
wget --quiet -O $TMPFILE http://standards.ieee.org/develop/regauth/oui/oui.txt || \
	ErrorMsg ERR "Unable to contact IEEE server!"

echo "OK"
}

format_file ()
{
echo -n "Reformatting..."
grep "(hex)" $TMPFILE | awk '{print $1","$3,$4,$5,$6,$7,$8}' | \
	sed 's/ *$//; /^$/d' > $OUIFILE || \
	ErrorMsg ERR "Unable to reformat file! Is awk/sed installed?"

sed -i 's/-/:/g' $OUIFILE || \
	ErrorMsg ERR "Unable to run sed!"
echo "OK"
}

clean_all ()
{
echo -n "Removing files..."
rm -f $TMPFILE
rm -f $OUIFILE
echo "OK"
}

# Execution Start
case $1 in
'force')
	clean_all
	get_oui
	format_file
;;
'clean')
	clean_all
;;
'check')
	# If file doesn't exist
	if [ ! -f $OUIFILE ];
	then
		get_oui
		format_file
		echo "Done."
		exit 0
	fi
	
	# If we get here, it does
	echo "OUI file exists, skipping."
;;
*)
	echo "gen_oui.sh Version: $VER"
	echo "usage: $0 check|force|clean"
	echo
	echo "This script will download a copy of the OUI database from"
	echo "the IEEE and format it for libmackerel."
	echo
	echo "The available arguments as of version $VER are as follows:"
	echo "check      - Check for and create OUI file"
	echo "force      - Force an update of OUI file"
	echo "clean      - Remove all created files"
esac
#EOF
