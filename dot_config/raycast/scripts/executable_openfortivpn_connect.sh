#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Connect VPN
# @raycast.mode silent

# Optional parameters:
# @raycast.icon 🤖
# @raycast.packageName openfortivpn


function isnt_connected () {
    grep -q 'Tunnel is up and running.' ~/openfortivpn.log
}

function poll_until_connected () {
    let loops=0 || true
    let max_loops=200 # 200 * 0.1 is 20 seconds. Bash doesn't support floats

    while isnt_connected "$VPN"; do
        sleep 0.1 # can't use a variable here, bash doesn't have floats
        let loops=$loops+1
        [ $loops -gt $max_loops ] && break
    done

    [ $loops -le $max_loops ]
}

(sudo openfortivpn > ~/openfortivpn.log 2>&1 < /dev/null &)

if poll_until_connected; then
    echo "Connected to VPN"
else
    echo "Couldn't connect to VPN, check ~/openfortivpn.log for details"
    exit 1;
fi
