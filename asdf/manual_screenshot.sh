#!/bin/bash
test -f /tmp/.X99-lock && { echo "/tmp/.X99-lock exists..." ; exit 21; }
base_name=${base_name:-$(cat env/base_name)}
SNAPSHOT_DIR=${SNAPSHOT_DIR:-"../_snapshot/${base_name}"}

HERE=$PWD

export WINE_EXE=${WINE_EXE:-wine}
export EXE=${EXE:-$PWD/${base_name}.exe}

which Xvfb || { echo 'no Xvfb' ; exit 3; }
which $WINE_EXE || { echo "nox WINE_EXE '$WINE_EXE'" ; exit 3; }
which $EXE || { echo "no EXE '$EXE'" ; exit 3; }

export WINE_EXE_OPTS=${WINE_EXE_OPTS:-}
export EXE_OPTS=""
export SCREENSHOT_OPTS="$@" #${SCREENSHOT_OPTS:-"--grid http://grid.kitely.com:8002 --login Test User password"}
export WINEPREFIX=$PWD/steamuser/.wine
export WINEPATH=${WINEPATH:-$SNAPSHOT_DIR/runtime}
export USER=steamuser
export HOME=$PWD/steamuser
mkdir -p $HOME
export XDG_DATA_HOME=$HOME/.local/share
export XDG_CONFIG_HOME=$HOME/.config
export XDG_CACHE_HOME=$HOME/.cache
export WINEARCH=win64
export WINEDEBUG=${WINEDEBUG:-"-all"}
export WINEDLLOVERRIDES=${WINEDLLOVERRIDES:-"winedbg.exe=d"}


XVFB_PID=
VIEWER_PID=
shutdown() {
    trap '' SIGINT SIGTERM EXIT
    echo "shutdown $1..."
    if [[ -n $VIEWER_PID ]] ; then
        echo "kill VIEWER_PID $VIEWER_PID..."
        kill $VIEWER_PID || true
    fi
    if [[ -n $XVFB_PID ]] ; then
        echo "kill XVFB_PID $XVFB_PID..."
        kill $XVFB_PID || true
    fi
    exit 16
}
trap 'shutdown "SIGINT (Ctrl+C)"' SIGINT
trap 'shutdown "EXIT"' EXIT


export DISPLAY=:99
Xvfb :99 -screen 0 1024x768x24 -fbdir /tmp -nolisten tcp &
XVFB_PID=$!
echo "Xvfb started with PID $XVFB_PID"
sleep 2

fluxbox 2>&1  > /dev/null &
echo "Fluxbox started"


echo "cd $WINEPATH and running $EXE"
cd $WINEPATH #$SNAPSHOT_DIR/runtime

bash -xc 'exec $WINE_EXE $WINE_EXE_OPTS "$EXE" $EXE_OPTS $SCREENSHOT_OPTS 2>&1' 2>&1 > $HERE/jules_wine.log &
VIEWER_PID=$!
echo "Viewer started with PID $VIEWER_PID"
TIMEOUT=${TIMEOUT:-90}
echo "Waiting ${TIMEOUT} seconds..."
sleep ${TIMEOUT}

echo "Taking screenshot..."
import -window root $HERE/screenshot-region.jpg
ls -l $HERE/screenshot-region.jpg

echo "Killing viewer..."
kill $VIEWER_PID
sleep 5
kill $XVFB_PID
echo "Done."
