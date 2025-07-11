#!/bin/sh

# Update/create luanti po files

# an auxiliary function to abort processing with an optional error
# message
abort() {
	test -n "$1" && echo >&2 "$1"
	exit 1
}

# The po/ directory is assumed to be parallel to the directory where
# this script is. Relative paths are fine for us so we can just
# use the following trick (works both for manual invocations and for
# script found from PATH)
scriptisin="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# The script is executed from the parent of po/, which is also the
# parent of the script directory and of the src/ directory.
# We go through $scriptisin so that it can be executed from whatever
# directory and still work correctly
cd "$scriptisin/.."

test -e po || abort "po/ directory not found"
test -d po || abort "po/ is not a directory!"

# Get a list of the languages we have to update/create

cd po || abort "couldn't change directory to po!"

# This assumes that we won't have dirnames with space, which is
# the case for language codes, which are the only subdirs we expect to
# find in po/ anyway. If you put anything else there, you need to suffer
# the consequences of your actions, so we don't do sanity checks
langs=""

for lang in * ; do
	if test ! -d $lang; then
		continue
	fi
	langs="$langs $lang"
done

# go back
cd ..

# First thing first, update the .pot template. We place it in the po/
# directory at the top level. You a recent enough xgettext that supports
# --package-name
potfile=po/luanti.pot
echo "updating pot"
xgettext --package-name=luanti \
	--add-comments='~' \
	--sort-by-file \
	--add-location=file \
	--keyword=N_ \
	--keyword=wgettext \
	--keyword=fwgettext \
	--keyword=fgettext \
	--keyword=fgettext_ne \
	--keyword=hgettext \
	--keyword=strgettext \
	--keyword=wstrgettext \
	--keyword=core.gettext \
	--keyword=showTranslatedStatusText \
	--keyword=fmtgettext \
	--output $potfile \
	--from-code=utf-8 \
	`find src/ -name '*.cpp' -o -name '*.h'` \
	`find builtin/ -name '*.lua'`

# Gettext collects a huge amount of bogus comments for the string
# "Available commands: ", and this not once but twice!
# I couldn't figure out how to avoid that so get rid of them afterwards:
for i in 1 2; do
	sed '/^#\. ~= 0\.3$/,/^#: /{ /^#: /!d; }' -i $potfile
done

# Now iterate on all languages and create the po file if missing, or update it
# if it exists already
for lang in $langs ; do # note the missing quotes around $langs
	pofile=po/$lang/luanti.po
	if test -e $pofile; then
		echo "[$lang]: updating strings"
		# Drop old strings *before* updating such that they can be re-used
		# until this script is run again.
		msgattrib --output-file=$pofile --no-obsolete $pofile
		msgmerge --update --backup=none --sort-by-file $pofile $potfile
	else
		# This will ask for the translator identity
		echo "[$lang]: NEW strings"
		msginit --locale=$lang --output-file=$pofile --input=$potfile
	fi

done
