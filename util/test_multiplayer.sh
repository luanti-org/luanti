#!/bin/bash
# Runs a multiplayer server and connects a headless client, devtest unittests are executed.

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
gameid=${gameid:-devtest}
executable=$dir/../bin/luanti
testspath=$dir/../tests
conf_client1=$testspath/client1.conf
conf_server=$testspath/server.conf
worldpath=$testspath/world

waitfor () {
	n=30
	while [ $n -gt 0 ]; do
		[ -f "$1" ] && return 0
		sleep 0.5
		((n-=1))
	done
	echo "Waiting for ${1##*/} timed out"
	pkill -P $$
	exit 1
}

[ -e "$executable" ] || { echo "executable $executable missing"; exit 1; }

rm -f "$testspath/log.txt"
rm -rf "$worldpath"
mkdir -p "$worldpath/worldmods"

printf '%s\n' >"$testspath/client1.conf" \
	video_driver=null name=client1 viewing_range=10 \
	enable_{sound,minimap,post_processing}=false

printf '%s\n' >"$testspath/server.conf" \
	max_block_send_distance=1 active_block_range=1 \
	devtest_unittests_autostart=true helper_mode=devtest \
	"${serverconf:-}"

ln -s "$dir/helper_mod" "$worldpath/worldmods/"

echo "Starting server"
"$executable" --debugger --server --config "$conf_server" --world "$worldpath" --gameid $gameid 2>&1 \
	| sed -u 's/^/(server) /' | tee -a "$testspath/log.txt" &
waitfor "$worldpath/startup"

echo "Starting client"
"$executable" --debugger --config "$conf_client1" --go --address 127.0.0.1 2>&1 \
	| sed -u 's/^/(client) /' | tee -a "$testspath/log.txt" &
waitfor "$worldpath/done"

echo "Waiting for client and server to exit"
wait

if [ -f "$worldpath/test_failure" ]; then
	echo "There were test failures."
	exit 1
fi
# gdb|lldb
if grep -Eq "(Thread .* received signal|thread .* stop reason =)" "$testspath/log.txt"; then
	echo "Debugger reported error."
	exit 1
fi
echo "Success"
exit 0
