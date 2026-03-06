#!/bin/bash
set -euo pipefail

echo "--- installing ubuntu build deps ---"
sudo apt-get update 
sudo apt-get install -qq -y 
sudo apt install -y -qq -o=Dpkg::Use-Pty=0 wget gpg-agent file eatmydata software-properties-common gettext gettext-base python3-yaml
pip install PyYAML
