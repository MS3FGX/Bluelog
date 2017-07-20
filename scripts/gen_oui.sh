#!/bin/bash
# Generate OUI list for libmackerel
VER="1.4"

# File to download
DLFILE="http://linuxnet.ca/ieee/oui.txt.gz"

# Location of tmp file
TMPDIR="/tmp"
TMPFILE="/tmp/out.tmp"

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
echo -n "Downloading OUI file from $DLFILE..."
wget --quiet -P $TMPDIR $DLFILE || \
	ErrorMsg ERR "Unable to contact server!"

echo "OK"
}

expand_file ()
{
	echo -n "Decompressing..."
	gunzip $TMPDIR/oui.txt.gz
	cp $TMPDIR/oui.txt $TMPFILE
	echo "OK"
}

format_file ()
{
echo -n "Reformatting..."
# Isolate MAC and manufacturer
grep "(hex)" $TMPFILE | awk '{print $1","$3,$4,$5,$6,$7,$8}' | \
	sed 's/ *$//; /^$/d' > $OUIFILE || \
	ErrorMsg ERR "Unable to reformat file! Is awk/sed installed?"

# Use colon in MAC addresses
sed -i 's/-/:/g' $OUIFILE || \
	ErrorMsg ERR "Unable to format MACs!"

# Remove commas from manufacturer names
sed -i 's/,//g2' $OUIFILE || \
	ErrorMsg ERR "Unable to format manufacturers!"

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
	expand_file
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
		expand_file
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
