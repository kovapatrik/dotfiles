#!/bin/bash

set -ex

# This script installs everything from scratch. It is meant to be used through a curl to bash command.

# Install XCode Command Line Tools if necessary
xcode-select --install || echo "XCode already installed"

if [[ -x /opt/homebrew/bin/brew ]]; then
    if which -s brew; then
        echo 'Homebrew is already installed'
    else
        echo 'Homebrew is already installed for another user. Please use an alias for brew.'
        exit 1
    fi
else
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    eval "$(/opt/homebrew/bin/brew shellenv)"
fi

brew install chezmoi
chezmoi init kovapatrik
chezmoi apply
