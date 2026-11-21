#!/usr/bin/env python3
# LayerNX Second Screen: eigene P2P-Gruppe (Tablet = Gruppenleiter) mit Wi-Fi-Display-Kennung.
# Windows (Win+K) tritt per WPS-Push-Button bei (ohne gemeinsames WLAN). Der ERSTE Rechner wird
# gemerkt (/etc/layernx/second-screen-pc) und immer ohne Nachfrage angenommen, alle anderen
# werden abgelehnt, bis er in den Einstellungen vergessen wird.
import os, socket, time, subprocess, glob, struct, threading
CTRL_DIR = "/run/layernx-second-screen/wpa"
RUN = "/run/layernx-second-screen"
PAIR = "/etc/layernx/second-screen-pc"   # "p2p_dev_addr name"
def log(t):
    with open(RUN + "/p2p.log", "a") as f: f.write(time.strftime("%T ") + t + "\n")

class Ctrl:
    def __init__(self, ifn, attach=False):
        self.me = RUN + "/ctl-%d-%s" % (os.getpid(), ifn)
        try: os.unlink(self.me)
        except FileNotFoundError: pass
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.s.bind(self.me); self.s.connect(os.path.join(CTRL_DIR, ifn)); self.s.settimeout(5)
        if attach: self.cmd("ATTACH")
    def cmd(self, c):
        self.s.send(c.encode())
        while True:
            r = self.s.recv(8192).decode(errors="replace")
            if not r.startswith("<"): return r.strip()
            on_event(r.strip())

pending = []
def on_event(r): pending.append(r)

def wlan_freq():
    try:
        o = subprocess.run(["iw", "dev", "wlan0", "link"], capture_output=True, text=True).stdout
        for l in o.splitlines():
            if "freq:" in l: return int(float(l.split("freq:")[1].split()[0]))
    except Exception: pass
    return 2437

def lease_ip(mac):
    # udhcpd-Leases: 8 Byte Zeit, dann je 36 Byte: Laufzeit(4) IP(4) MAC(6) Name(20) Fuell(2)
    try: d = open(RUN + "/dhcp.leases", "rb").read()
    except Exception: return None
    m = bytes(int(x, 16) for x in mac.split(":"))
    for i in range(8, len(d) - 35, 36):
        if d[i+8:i+14] == m: return socket.inet_ntoa(d[i+4:i+8])
    return None

sink = None
def sink_starten(mac):
    global sink
    ip = None
    for i in range(40):
        subprocess.call(["pkill", "-USR1", "-f", "second-screen/dhcp[.]conf"])
        ip = lease_ip(mac)
        if ip: break
        time.sleep(0.5)
    ip = ip or "192.168.173.80"
    log("quelle %s -> wfdsink" % ip)
    if sink and sink.poll() is None: sink.kill()
    sink = subprocess.Popen(["python3", "-u", "/usr/libexec/layernx/layernx-wfd-rtsp.py", ip], stdout=open(RUN + "/rtsp.out", "a"), stderr=subprocess.STDOUT)

def gepaart():
    try: return open(PAIR).read().split()[0].lower()
    except Exception: return None

def modus():
    try:
        for l in open("/etc/conf.d/layernx-second-screen"):
            if l.startswith("MODUS="): return l.split("=", 1)[1].strip().strip('"') or "client"
    except Exception: pass
    return "client"

def pairing_ok(r, peer):
    name = r.split("name='", 1)[1].split("'", 1)[0] if "name='" in r else "?"
    p = gepaart()
    if p and p != peer:
        log("abgelehnt: %s (%s), gepaart ist %s" % (peer, name, p)); return False
    if not p:
        os.makedirs(os.path.dirname(PAIR), exist_ok=True)
        open(PAIR, "w").write("%s %s\n" % (peer, name)); log("gemerkt: %s %s" % (peer, name))
    return True

UDHCPC = RUN + "/udhcpc.sh"
open(UDHCPC, "w").write("""#!/bin/sh
# nur Adresse setzen, KEINE Standardroute (das normale WLAN bleibt Internet-Weg)
case "$1" in
  bound|renew) ip addr flush dev "$interface"; ip addr add "$ip/${mask:-24}" dev "$interface"
               echo "${router%%%% *}" > %s/quelle ;;
  deconfig) ip addr flush dev "$interface" ;;
esac
""" % RUN)
os.chmod(UDHCPC, 0o755)

def client_gruppe(ifn):
    try: os.unlink(RUN + "/quelle")
    except FileNotFoundError: pass
    subprocess.call(["busybox", "udhcpc", "-i", ifn, "-n", "-q", "-t", "15", "-T", "1", "-s", UDHCPC])
    try: ip = open(RUN + "/quelle").read().strip()
    except Exception: ip = ""
    if not ip:
        log("keine Adresse von Windows erhalten"); return
    log("quelle %s -> rtsp" % ip)
    global sink
    if sink and sink.poll() is None: sink.kill()
    sink = subprocess.Popen(["python3", "-u", "/usr/libexec/layernx/layernx-wfd-rtsp.py", ip], stdout=open(RUN + "/rtsp.out", "a"), stderr=subprocess.STDOUT)

MODUS = modus()
dev = Ctrl("p2p0", attach=True)
dev.cmd("SET wifi_display 1")
# WFD-Geraeteinfo: primaerer Sink, Sitzung verfuegbar, RTSP-Port 7236, max. 200 Mbit/s
dev.cmd("WFD_SUBELEM_SET 0 000600111c4400c8")
log("attach, modus=%s, gepaart=%s" % (MODUS, gepaart()))
grp, grp_if = None, None
if MODUS == "go":
    f = wlan_freq()
    log("P2P_GROUP_ADD freq=%d: %s" % (f, dev.cmd("P2P_GROUP_ADD freq=%d" % f)))
else:
    log("P2P_LISTEN: %s" % dev.cmd("P2P_LISTEN"))
dev.s.settimeout(None)
autorisiert = 0
while True:
    r = pending.pop(0) if pending else dev.s.recv(8192).decode(errors="replace").strip()
    log(r[:300])
    w = r.split(">", 1)[-1].split()
    if not w: continue
    if MODUS != "go":
        # --- Windows ist Gruppenleiter (eigener Kanal, kein Kanalspringen am Laptop) ---
        if w[0] in ("P2P-PROV-DISC-PBC-REQ", "P2P-GO-NEG-REQUEST"):
            peer = w[1].lower()
            if not pairing_ok(r, peer): continue
            if w[0] == "P2P-PROV-DISC-PBC-REQ":
                if time.time() - autorisiert < 5: continue
                autorisiert = time.time()
                log("-> P2P_CONNECT auth: %s" % dev.cmd("P2P_CONNECT %s pbc auth go_intent=0" % peer))
                dev.cmd("P2P_LISTEN")
            elif time.time() - autorisiert > 30:
                log("-> P2P_CONNECT: %s" % dev.cmd("P2P_CONNECT %s pbc go_intent=0" % peer))
        elif w[0] == "P2P-GROUP-STARTED":
            grp_if = w[1]
            threading.Thread(target=client_gruppe, args=(grp_if,), daemon=True).start()
        elif w[0] in ("P2P-GROUP-REMOVED", "P2P-GO-NEG-FAILURE", "P2P-GROUP-FORMATION-FAILURE"):
            if sink and sink.poll() is None: sink.terminate()
            autorisiert = 0
            time.sleep(1); log("P2P_LISTEN: %s" % dev.cmd("P2P_LISTEN"))
        continue
    # --- Tablet ist Gruppenleiter ---
    if w[0] == "P2P-GROUP-STARTED":
        grp_if = w[1]
        subprocess.call(["ip", "addr", "add", "192.168.173.1/24", "dev", grp_if])
        open(RUN + "/dhcp.conf", "w").write("start 192.168.173.80\nend 192.168.173.90\ninterface %s\nlease_file %s/dhcp.leases\noption subnet 255.255.255.0\nauto_time 1\n" % (grp_if, RUN))
        open(RUN + "/dhcp.leases", "a").close()
        subprocess.Popen(["busybox-extras", "udhcpd", "-f", RUN + "/dhcp.conf"])
        time.sleep(1)
        grp = Ctrl(grp_if)
        log("gruppe %s bereit" % grp_if)
    elif w[0] == "P2P-PROV-DISC-PBC-REQ" and grp:
        peer = w[1].lower()
        if not pairing_ok(r, peer): continue
        log("-> WPS_PBC %s: %s" % (peer, grp.cmd("WPS_PBC")))
    elif w[0] == "AP-STA-CONNECTED":
        threading.Thread(target=sink_starten, args=(w[1],), daemon=True).start()
    elif w[0] == "AP-STA-DISCONNECTED":
        if sink and sink.poll() is None: sink.terminate()
    elif w[0] == "P2P-GROUP-REMOVED":
        subprocess.call(["pkill", "-f", "second-screen/dhcp[.]conf"]); grp = None
        time.sleep(2)
        log("P2P_GROUP_ADD neu: %s" % dev.cmd("P2P_GROUP_ADD freq=%d" % wlan_freq()))
