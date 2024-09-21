#!/bin/bash
set -ex

echo ""
echo "Setting up rustup"
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain stable --profile default -y --no-modify-path
echo "Done"
