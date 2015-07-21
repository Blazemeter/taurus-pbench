import os
import sys
import struct
import subprocess
import unittest

TEST_AMMO = [
    ("1234 f", "GET / HTTP/1.1\r\n\r\n"),
    ("23 login", "GET /login/u1 HTTP/1.1\r\nHost: google.com\r\n\r\n"),
    ("43 abc123", "GET /abc HTTP/1.1\r\n\r\n"),
    ("1233 abc123", "GET /123 HTTP/1.1\r\n\r\n"),
]

CONFIG_TEMPLATE="""
setup_t module_setup = setup_module_t {
    dir = "/usr/lib/phantom"
    list = {
        io_benchmark
        io_benchmark_method_stream
        io_benchmark_method_stream_ipv4
        io_benchmark_method_stream_source_random
        io_benchmark_method_stream_proto_http
        io_stream
        io_stream_ipv4
        io_stream_proto_http
        io_stream_proto_http_handler_null
    }
}

setup_t module_setup = setup_module_t {
    dir = "lib/phantom"
    list = {
        taurus_source
    }
}

scheduler_t main_scheduler = scheduler_simple_t {
    threads = 4
}

io_t stream_io = io_stream_ipv4_t {
    proto_t http_proto = proto_http_t {
        logger_t request_logger = logger_default_t {
            filename = "request.log"
            scheduler = main_scheduler
        }

        loggers = { request_logger }

        handler_t null_handler = handler_null_t {}

        host = {
            "localhost": {
                path = {
                    "/": {
                        handler = null_handler
                    }
                }
            }
        }
    }

    proto = http_proto

    address = 127.0.0.1
    port = 8080
    listen_backlog = 1K

    scheduler = main_scheduler
}

io_t benchmark_io = io_benchmark_t {
    method_t stream_method = method_stream_ipv4_t {
        source_t taurus_source = taurus_source_t {
            ammo = "ammo.stpd"
            schedule = "schedule.sch"
            {source_extra_conf}
        }

        proto_t http_proto = proto_http_t { }

        address = 127.0.0.1
        port = 8080
        timeout = 5s
        source = taurus_source
        proto = http_proto

        logger_t default_logger = logger_default_t {
            filename = "benchmark-default.log"
            level = proto_warning
            scheduler = main_scheduler
        }

        logger_t brief_logger = logger_brief_t {
            filename = "benchmark-brief.log"
            scheduler = main_scheduler
        }

        loggers = { default_logger brief_logger }
    }

    times_t simple_times = times_simple_t {
        max = 3s
        min = 10
        steps = 20
    }

    instances = 1
    method = stream_method
    times = simple_times

    scheduler = main_scheduler
}
"""

PHANTOM_TEST_CONF="phantom_test.conf"

class TaurusSourceTestCase(unittest.TestCase):
    def create_config(self, **kwargs):
        open(PHANTOM_TEST_CONF, "w").write(CONFIG_TEMPLATE.format(**kwargs))

    def start_phantom(self):
        self.phantom = subprocess.Popen(["phantom", "run", PHANTOM_TEST_CONF])

    def parse_access_log(self):
        log = []
        for line in open("test_access.log"):
            pass
        return log

    def test_without_loop(self):
        pass




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
