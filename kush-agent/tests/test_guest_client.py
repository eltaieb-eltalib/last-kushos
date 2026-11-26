import base64
import json
import socket
import threading

from kush_agent.tools.computer import Computer
from kush_agent.vm.guest import GuestClient, GuestError


def fake_guest(path, log):
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(path)
    srv.listen(1)

    def serve():
        conn, _ = srv.accept()
        conn.sendall(b"ReactOS boot noise\r\n")
        buf = b""
        while True:
            data = conn.recv(65536)
            if not data:
                return
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                req = json.loads(line)
                log.append(req)
                m, p = req["method"], req["params"]
                if m == "ping":
                    res = {"result": {"ok": True, "agent": "0.1.0"}}
                elif m == "read_file":
                    res = {"result": {"size": 3, "data": base64.b64encode(b"abc").decode()}}
                elif m == "type_unicode":
                    res = {"result": {"ok": True}}
                else:
                    res = {"error": "unknown method"}
                conn.sendall(json.dumps({"id": req["id"], **res}).encode() + b"\n")

    threading.Thread(target=serve, daemon=True).start()
    return srv


def test_guest_client(tmp_path):
    path = str(tmp_path / "ga.sock")
    log = []
    fake_guest(path, log)
    g = GuestClient(path, timeout=5).connect()
    assert g.ping()["ok"]
    assert g.read_file("C:\\x.txt") == b"abc"
    try:
        g.call("nope")
        raise AssertionError("expected error")
    except GuestError as e:
        assert "unknown method" in str(e)

    class NoQMP:
        def input_events(self, ev):
            raise AssertionError("QMP must not be used for Arabic text")

    comp = Computer(NoQMP(), guest=g, settle=0)
    assert not comp.run("type", {"text": "مرحبا يا كوش"}).is_error
    sent = [r for r in log if r["method"] == "type_unicode"][0]
    assert sent["params"]["text"] == "مرحبا يا كوش"
