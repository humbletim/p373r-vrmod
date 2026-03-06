#!/bin/bash
set -euo pipefail

echo "--- installing ubuntu xvfb/wine deps ---"
sudo apt-get update 
sudo DEBIAN_FRONTEND=noninteractive apt install -y -qq -o=Dpkg::Use-Pty=0 wine-stable xvfb fluxbox imagemagick x11-apps

which wine
