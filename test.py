import os
import sys
import struct
import subprocess
import BaseHTTPServer

TEST_AMMO = [
    ("", "GET / HTTP/1.1\r\n\r\n"),
    ("login", "GET /login/u1 HTTP/1.1\r\nHost: google.com\r\n\r\n"),
    ("abc123", "GET /abc HTTP/1.1\r\n\r\n"),
    ("abc123", "GET /123 HTTP/1.1\r\n\r\n"),
]

def test_phantom():
    offsets = []

    with open("ammo.stpd", "w") as ammo:
        for tag, payload in TEST_AMMO:
            plen = len(tag) + len(payload) + 2
            ammo.write("%d " % plen)
            pstart = ammo.tell()
            ammo.write("%s\r\n%s\r\n" % (tag, payload))

            print "schedule:", pstart, len(tag), len(payload)

            offsets.append((pstart, plen))

    with open("schedule.sch", "w") as schedule:
        for i, (pstart, plen) in enumerate(offsets):
            t = 500
            if i == 0:
                t = t | (1 << 24)

            schedule.write(struct.pack("IIL", t, plen, pstart))

    phantom = subprocess.Popen(["phantom", "run", "example.conf"])

    assert phantom.wait() == 0
