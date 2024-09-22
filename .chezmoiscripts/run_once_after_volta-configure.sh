#!/bin/bash
set -ex

echo ""
echo "Setting up Volta, with Node and pnpm installed"

curl https://get.volta.sh | bash -s -- --skip-setup

VOLTA_HOME="$HOME/.volta"
PATH="$VOLTA_HOME/bin:$PATH"

volta install node@22
volta install pnpm

echo "Done"
