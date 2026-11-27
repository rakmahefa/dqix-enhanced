#!/bin/sh
# What a co-op session costs the relay, measured at the relay.
#
# tools/relay/worker.js counts every message it receives, and the free plan's
# daily allowance is spent per message, so "messages per second of play" is the
# number to shrink. This runs four players (one host, three friends) against a
# stand-in relay that speaks the same room protocol and counts what it gets
# (server.py), each player sending one small message per frame, as the session
# does. docs/PERF.md's rank-1 entry is proved with this: same command before
# and after a change, compare the TOTAL lines.
#
#   tests/relay_bench/run.sh [seconds] [fps] [coalesce_ms]
# coalesce_ms defaults to the build's own COALESCE_MS (30): a wider window
# bundles more frames per message and costs that much extra input queueing.
set -e
cd "$(dirname "$0")"; R=../../runtime
SECONDS_ARG=${1:-20}; FPS=${2:-59.83}; COALESCE=${3:-}
COALESCE_DEF=${COALESCE:+-DCOALESCE_MS=$COALESCE}
W=${TMPDIR:-/tmp}/relay_bench.$$; mkdir -p $W
trap 'kill $SRV $HP $F1 $F2 $F3 2>/dev/null || true; rm -rf $W' EXIT
gcc -O1 -w -I$R $COALESCE_DEF bench.c $R/relay.c $R/relay_tls.c -o $W/bench -lpthread
PORT=$((20000 + $$ % 20000))
python3 server.py $PORT $SECONDS_ARG > $W/srv.log 2>&1 & SRV=$!
while ! grep -q ready $W/srv.log 2>/dev/null; do sleep 0.1; done
export DQIX_RELAY=http://127.0.0.1:$PORT
$W/bench host ABCD2345 $((PORT + 1)) 3 $SECONDS_ARG $FPS > $W/host.log 2>&1 & HP=$!
sleep 0.3
$W/bench friend ABCD2345 $SECONDS_ARG $FPS > $W/f1.log 2>&1 & F1=$!
$W/bench friend ABCD2345 $SECONDS_ARG $FPS > $W/f2.log 2>&1 & F2=$!
$W/bench friend ABCD2345 $SECONDS_ARG $FPS > $W/f3.log 2>&1 & F3=$!
wait $HP $F1 $F2 $F3 || true
wait $SRV || true
grep TOTAL $W/srv.log
# messages per second of play, four players; the relay counts every message from every player
TOTAL=$(sed -n 's/^TOTAL //p' $W/srv.log | tail -1)
python3 - "$TOTAL" "$SECONDS_ARG" <<'EOF'
import sys
total = dict(kv.split("=") for kv in sys.argv[1].split())
n = sum(int(v) for v in total.values())
print("messages: %d in %s s = %.1f/s for four players (%.1f per player-second)"
      % (n, sys.argv[2], n / float(sys.argv[2]), n / float(sys.argv[2]) / 4))
EOF
