#!/usr/bin/env python3
# LayerNX Second Screen: Wi-Fi-Display-(Miracast-)Empfaenger, RTSP-Steuerkanal.
#   layernx-wfd-rtsp.py QUELL-IP [PORT]
# Bild/Ton zeigt layernx-wfd-anzeige (C) an.
# Das Tablet (Sink) verbindet sich zur Quelle (Windows) auf Port 7236 und handelt M1..M7 aus.
import os, re, signal, socket, subprocess, sys, time

SRC = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 7236
RTP_PORT = 1028
LOG = "/run/layernx-second-screen/rtsp.log"

def log(t):
    with open(LOG, "a") as f:
        f.write(time.strftime("%T ") + t + "\n")

# 1280x800p30 (VESA-Index 8) als native Aufloesung, zusaetzlich Pflichtmodus 640x480p60 (CEA 0).
# native = (8 << 3) | 1 (VESA) = 0x41; Profil CBP+CHP (03), Level 4.2 (10)
VIDEO = "41 00 03 10 00000001 00000100 00000000 00 0000 0000 00 none none"
AUDIO = "none"   # kein Ton: Windows behaelt den Ton am PC
ANTWORTEN = {
    "wfd_video_formats": VIDEO,
    "wfd_audio_codecs": AUDIO,
    "wfd_client_rtp_ports": "RTP/AVP/UDP;unicast %d 0 mode=play" % RTP_PORT,
    "wfd_content_protection": "none",
    "wfd_display_edid": "none",
    "wfd_coupled_sink": "none",
    "wfd_uibc_capability": "none",
    "wfd_standby_resume_capability": "none",
    "wfd_connector_type": "05",
    "microsoft_cursor": "none",
    "microsoft_rtcp_capability": "none",
    "microsoft_latency_management_capability": "none",
    "microsoft_format_change_capability": "none",
    "microsoft_diagnostics_capability": "none",
    "intel_friendly_name": socket.gethostname(),
    "intel_sink_manufacturer_name": "LayerNX",
    "intel_sink_model_name": "LayerNX-Tablet-Linux",
    "intel_sink_device_URL": "none",
    "intel_sink_version": "none",
}

class Rtsp:
    def __init__(self, s):
        self.s, self.buf, self.cseq = s, b"", 1
    def lesen(self):
        while b"\r\n\r\n" not in self.buf:
            d = self.s.recv(65536)
            if not d:
                raise EOFError("Verbindung zu")
            self.buf += d
        kopf, rest = self.buf.split(b"\r\n\r\n", 1)
        zeilen = kopf.decode(errors="replace").split("\r\n")
        h = {}
        for z in zeilen[1:]:
            if ":" in z:
                k, v = z.split(":", 1)
                h[k.strip().lower()] = v.strip()
        n = int(h.get("content-length", "0"))
        while len(rest) < n:
            d = self.s.recv(65536)
            if not d:
                raise EOFError("Verbindung zu")
            rest += d
        body, self.buf = rest[:n].decode(errors="replace"), rest[n:]
        log("<< " + zeilen[0] + (" | " + body.replace("\r\n", " / ")[:300] if body else ""))
        return zeilen[0], h, body
    def antwort(self, h, body="", extra=""):
        m = "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s" % (h.get("cseq", "0"), extra)
        if body:
            m += "Content-Type: text/parameters\r\nContent-Length: %d\r\n" % len(body)
        m += "\r\n" + body
        self.s.sendall(m.encode())
        log(">> 200 OK cseq=%s %s" % (h.get("cseq"), body.replace("\r\n", " / ")[:200]))
    def anfrage(self, methode, url, extra=""):
        self.cseq += 1
        m = "%s %s RTSP/1.0\r\nCSeq: %d\r\n%s\r\n" % (methode, url, self.cseq, extra)
        self.s.sendall(m.encode())
        log(">> " + methode + " " + url)
        return self.cseq

def verbinden():
    for i in range(60):
        try:
            s = socket.create_connection((SRC, PORT), timeout=3)
            s.settimeout(None)
            log("verbunden mit %s:%d" % (SRC, PORT))
            return s
        except OSError as e:
            time.sleep(0.5)
    raise SystemExit("keine Verbindung zu %s:%d" % (SRC, PORT))

def main():
    gst = None
    r = Rtsp(verbinden())
    url, session = None, None
    try:
        while True:
            start, h, body = r.lesen()
            if start.startswith("RTSP/1.0"):            # Antwort auf eigene Anfrage
                if "session" in h and not session:
                    session = h["session"].split(";")[0]
                    r.anfrage("PLAY", url, "Session: %s\r\n" % session)
                continue
            methode = start.split()[0]
            if methode == "OPTIONS":                      # M1
                r.antwort(h, extra="Public: org.wfa.wfd1.0, GET_PARAMETER, SET_PARAMETER\r\n")
                r.anfrage("OPTIONS", "*", "Require: org.wfa.wfd1.0\r\n")   # M2
            elif methode == "GET_PARAMETER":              # M3 / Keepalive (M16)
                namen = [z.strip() for z in body.split("\r\n") if z.strip()]
                antw = "".join("%s: %s\r\n" % (n, ANTWORTEN.get(n, "none")) for n in namen)
                r.antwort(h, antw)
            elif methode == "SET_PARAMETER":              # M4 / M5 (Trigger)
                r.antwort(h)
                m = re.search(r"wfd_presentation_URL:\s*(\S+)", body)
                if m:
                    url = m.group(1)
                if "wfd_trigger_method: SETUP" in body:
                    if gst is None:
                        gst = subprocess.Popen(["layernx-wfd-anzeige", str(RTP_PORT)],
                                               stdout=open("/run/layernx-second-screen/anzeige.log", "a"), stderr=subprocess.STDOUT)
                    r.anfrage("SETUP", url or "rtsp://%s/wfd1.0/streamid=0" % SRC,
                              "Transport: RTP/AVP/UDP;unicast;client_port=%d\r\n" % RTP_PORT)   # M6
                elif "wfd_trigger_method: TEARDOWN" in body:
                    r.anfrage("TEARDOWN", url or "*", "Session: %s\r\n" % session if session else "")
                    break
            else:
                r.antwort(h)
    except (EOFError, ConnectionError) as e:
        log("Ende: %s" % e)
    finally:
        if gst:
            gst.send_signal(signal.SIGTERM)
            try:
                gst.wait(3)
            except subprocess.TimeoutExpired:
                gst.kill()
        log("beendet")

main()
