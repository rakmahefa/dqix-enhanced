#!/usr/bin/env python3
"""A stand-in relay for tests/relay_bench/run.sh: the same room protocol as
tools/relay/worker.js over a plain WebSocket, and a count of every message it
receives, per role. The real relay's daily allowance is spent per message it
gets, so that count is what the benchmark measures.

usage: server.py port seconds
Prints "ready" once listening, then one summary line per second and a final
total: "TOTAL friend_data=N host_data=N host_multi=N keep=N other=N".
"""
import base64, hashlib, json, socket, struct, sys, threading, time

port, seconds = int(sys.argv[1]), float(sys.argv[2])

# RM_OPEN=0 RM_DATA=1 RM_CLOSE=2 RM_OK=3 RM_ERROR=4 RM_KEEP=5 RM_MULTI=6
lock = threading.Lock()
counts = {"friend_data": 0, "host_data": 0, "host_multi": 0, "keep": 0, "other": 0}
rows = {}                      # id -> socket of each friend in the room
host = None                    # the room's host socket


def frame(payload):
    n = len(payload)
    if n < 126:
        return b"\x82" + bytes([n]) + payload
    if n < 65536:
        return b"\x82\x7e" + struct.pack(">H", n) + payload
    return b"\x82\x7f" + struct.pack(">Q", n) + payload


def readn(c, n):
    b = b""
    while len(b) < n:
        d = c.recv(n - len(b))
        if not d:
            raise EOFError
        b += d
    return b


def send(c, payload):
    with lock:
        try:
            c.sendall(frame(payload))
        except OSError:
            pass


def read_msg(c):
    """one whole client message (binary, masked) and its opcode"""
    h = readn(c, 2)
    op = h[0] & 15
    n = h[1] & 127
    if n == 126:
        n = struct.unpack(">H", readn(c, 2))[0]
    elif n == 127:
        n = struct.unpack(">Q", readn(c, 8))[0]
    mask = readn(c, 4) if h[1] & 128 else b"\0\0\0\0"
    p = bytes(b ^ mask[i & 3] for i, b in enumerate(readn(c, n)))
    return op, p


def count(kind):
    with lock:
        counts[kind] += 1


def forward_friend(p):
    """a friend's message: RM_DATA [1, id, bytes] goes to the host"""
    if len(p) >= 2 and p[0] == 1:
        count("friend_data")
        if host is not None:
            send(host, p)
    elif len(p) >= 2 and p[0] == 5:
        count("keep")
    else:
        count("other")


def forward_host(p):
    """the host's message: RM_DATA [1, id, bytes] to that friend, RM_MULTI
    [6, 0, (id, len lo, len hi, bytes)...] to each of them"""
    if not p:
        return
    if p[0] == 1:
        count("host_data")
        c = rows.get(p[1])
        if c is not None:
            send(c, p)
    elif p[0] == 6:
        count("host_multi")
        i = 2
        while i + 3 <= len(p):
            rid, n = p[i], p[i + 1] | (p[i + 2] << 8)
            body = p[i + 3:i + 3 + n]
            i += 3 + n
            c = rows.get(rid)
            if c is not None:
                send(c, bytes([1, rid]) + body)
    elif p[0] == 5:
        count("keep")
    else:
        count("other")


def serve(raw, head, role, rid):
    global host
    lines = head.decode(errors="replace").split("\r\n")
    key = [l.split(": ", 1)[1] for l in lines if l.lower().startswith("sec-websocket-key")][0]
    acc = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
    raw.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                 "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n" % acc).encode())
    raw.sendall(frame(bytes([3, rid, 1])))          # RM_OK, id, protocol version 1 (RM_MULTI)
    if role == "host":
        host = raw
        for fid in list(rows):
            send(raw, bytes([0, fid]))              # RM_OPEN for those already here
    else:
        rows[rid] = raw
        if host is not None:
            send(host, bytes([0, rid]))
    try:
        while True:
            op, p = read_msg(raw)
            if op == 8:
                break
            if op == 2:
                (forward_host if role == "host" else forward_friend)(p)
    except (EOFError, OSError):
        pass
    finally:
        if role == "host":
            host = None
        else:
            rows.pop(rid, None)


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(8)
print("ready", flush=True)


def acceptor():
    nxt = 1
    while True:
        raw, _ = srv.accept()
        req = b""
        while not req.endswith(b"\r\n\r\n"):
            d = raw.recv(1)
            if not d:
                break
            req += d
        first = req.split(b"\r\n", 1)[0].decode(errors="replace")
        role = "host" if "role=host" in first else "join"
        rid = 0 if role == "host" else nxt
        if role != "host":
            nxt += 1
        threading.Thread(target=serve, args=(raw, req, role, rid), daemon=True).start()


threading.Thread(target=acceptor, daemon=True).start()

start = time.time()
while True:
    time.sleep(1.0)
    with lock:
        line = dict(counts)
    per_s = sum(line.values())
    print(json.dumps({"t": round(time.time() - start, 1), "messages": per_s, **line}), flush=True)
    if time.time() - start >= seconds:
        break
with lock:
    print("TOTAL " + " ".join("%s=%d" % (k, counts[k]) for k in
                              ("friend_data", "host_data", "host_multi", "keep", "other")), flush=True)
