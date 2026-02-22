#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Disconnect VPN
# @raycast.mode silent

# Optional parameters:
# @raycast.icon 🤖
# @raycast.packageName openfortivpn


function isnt_connected () {
    ! ps aux | grep -q '[o]penfortivpn'
}

if isnt_connected; then
    echo "VPN is not connected"
    exit 1
else
    sudo pkill openfortivpn
    echo "Disconnected from VPN"
fi
