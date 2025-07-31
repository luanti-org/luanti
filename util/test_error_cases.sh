#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
gameid=${gameid:-devtest}
executable=$dir/../bin/luanti
testspath=$dir/../tests
conf_server=$testspath/server.conf
worldpath=$testspath/world

[ -e "$executable" ] || { echo "executable $executable missing"; exit 1; }

write_config () {
	printf '%s\n' >"$conf_server" \
		helper_mode=error mg_name=singlenode "$@"
}

run () {
	timeout 10 "$@"
	r=$?
	echo "Exit status: $r"
	[ $r -eq 124 ] && echo "(timed out)"
	if [ $r -ne 1 ]; then
		echo "-> Test failed"
		exit 1
	fi
}

rm -rf "$worldpath"
mkdir -p "$worldpath/worldmods"

ln -s "$dir/helper_mod" "$worldpath/worldmods/"

args=(--server --config "$conf_server" --world "$worldpath" --gameid $gameid)

# make sure we can tell apart sanitizer and luanti errors
export ASAN_OPTIONS="exitcode=42"
export MSAN_OPTIONS="exitcode=42"

# see helper_mod/init.lua for the different types
for n in $(seq 1 6); do
	write_config error_type=$n
	run "$executable" "${args[@]}"
	echo "---------------"
done

echo "All done."
exit 0
