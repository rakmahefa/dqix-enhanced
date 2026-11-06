#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
export LD_LIBRARY_PATH="$HERE$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"
exec ./dqix data/dqix.nds data/code 0 1 dqix.sav
