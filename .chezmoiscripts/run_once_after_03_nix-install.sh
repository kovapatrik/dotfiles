#!/bin/bash
set -ex

echo ""
echo "Setting up Nix"
curl --proto '=https' --tlsv1.2 -L https://nixos.org/nix/install | sh -s -- --daemon
echo "Nix is done"
