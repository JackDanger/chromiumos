#!/bin/sh

export USER=chronos
export LOGNAME=${USER}
export SHELL=/bin/bash
export HOME=/home/${USER}
export DISPLAY=:0.0
export PATH=/bin:/usr/bin:/usr/local/bin:/usr/bin/X11
export XAUTHORITY=${HOME}/.Xauthority

/usr/bin/xauth -q -f $HOME/.Xauthority add :0 . $1
exec /etc/X11/Xsession
