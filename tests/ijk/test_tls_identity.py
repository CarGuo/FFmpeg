#!/usr/bin/env python3
"""Exercise FFmpeg's real OpenSSL HTTPS backend against isolated loopback fixtures.

No public server, existing certificate, or production credential is used. Keys and
certificates are generated in a temporary directory and deleted after the run.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import time


def command(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def certificate(root, name, san, signer="ca", cn="localhost", usage="serverAuth"):
    key, csr, cert, ext = [str(root / (name + suffix)) for suffix in (".key", ".csr", ".crt", ".ext")]
    command("openssl", "req", "-new", "-newkey", "rsa:2048", "-nodes", "-keyout", key,
            "-out", csr, "-subj", "/CN=" + cn)
    Path(ext).write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=" + usage + "\nsubjectAltName=" + san + "\n")
    command("openssl", "x509", "-req", "-in", csr, "-CA", str(root / (signer + ".crt")),
            "-CAkey", str(root / (signer + ".key")), "-CAcreateserial", "-days", "2", "-sha256",
            "-extfile", ext, "-out", cert)


def sanitizer_clean(stderr):
    return not any(marker in stderr for marker in
                   ("AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer", "runtime error:"))


def run_case(binary, root, case):
    name, cert, connect, verify, ca, host, expected, expected_sni, fault = case
    family = socket.AF_INET6 if connect == "::1" else socket.AF_INET
    server = socket.socket(family, socket.SOCK_STREAM)
    server.bind(("::1" if family == socket.AF_INET6 else "127.0.0.1", 0))
    server.listen(1)
    server.settimeout(4)
    port = server.getsockname()[1]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(root / (cert + ".crt")), str(root / (cert + ".key")))
    observed = {"sni": None, "application_request": False}
    context.set_servername_callback(lambda conn, name, ctx: observed.update(sni=name))

    def serve():
        try:
            sock, _ = server.accept()
            sock.settimeout(3)
            with context.wrap_socket(sock, server_side=True) as peer:
                request = peer.recv(8192)
                observed["application_request"] = bool(request)
                if request:
                    peer.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\ntls-ok")
        except (OSError, ssl.SSLError) as exc:
            observed["server_error"] = str(exc)
        finally:
            server.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    urlhost = "[::1]" if connect == "::1" else connect
    capath = "none" if ca in ("none", "default-untrusted") else str(root / ca)
    env = os.environ.copy()
    for key in ("http_proxy", "https_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "all_proxy"):
        env.pop(key, None)
    env["no_proxy"] = "*"
    env["SSL_CERT_FILE"] = str(root / ("other-ca.crt" if ca == "default-untrusted" else "ca.crt"))
    env["SSL_CERT_DIR"] = str(root / "empty-ca-dir")
    env.pop("TLS_TEST_FAIL", None)
    if fault:
        env["TLS_TEST_FAIL"] = fault
    proc = subprocess.run([binary, f"https://{urlhost}:{port}/fixture", verify, capath, host],
                          env=env, text=True, capture_output=True, timeout=8)
    thread.join(5)
    actual = proc.returncode == 0
    passed = actual == expected and proc.returncode in (0, 1) and sanitizer_clean(proc.stderr)
    if expected:
        passed = passed and observed["application_request"] and observed["sni"] == expected_sni
    else:
        passed = passed and not observed["application_request"]
    return {"name": name, "passed": passed, "expected_success": expected,
            "actual_success": actual, "returncode": proc.returncode, "observed": observed,
            "stderr": proc.stderr.strip(), "stdout": proc.stdout.strip()}


def run_listen_case(binary, root, name, client_cert, expected):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    env = os.environ.copy()
    for key in ("http_proxy", "https_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "all_proxy", "TLS_TEST_FAIL"):
        env.pop(key, None)
    args = [binary, f"tls://127.0.0.1:{port}?listen=1", "1", str(root / "ca.crt"),
            "unrelated-client-name.invalid", str(root / "dns.crt"), str(root / "dns.key")]
    proc = subprocess.Popen(args, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    peer_error = None
    try:
        deadline = time.monotonic() + 3
        while True:
            try:
                connection = socket.create_connection(("127.0.0.1", port), timeout=1)
                break
            except ConnectionRefusedError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.01)
        context = ssl.create_default_context(cafile=str(root / "ca.crt"))
        if client_cert:
            context.load_cert_chain(str(root / (client_cert + ".crt")), str(root / (client_cert + ".key")))
        with context.wrap_socket(connection, server_hostname="localhost") as peer:
            peer.sendall(b"tls-ok")
            peer.unwrap().close()
    except (OSError, ssl.SSLError) as exc:
        peer_error = str(exc)
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        raise
    actual = proc.returncode == 0
    return {"name": name, "passed": actual == expected and proc.returncode in (0, 1) and sanitizer_clean(stderr),
            "expected_success": expected, "actual_success": actual, "returncode": proc.returncode,
            "peer_error": peer_error, "stderr": stderr.strip(), "stdout": stdout.strip()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--case-filter", default="")
    parser.add_argument("--ipv4-only", action="store_true", help="exclude IPv6 transport cases for the IPv4-only getaddrinfo fallback")
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())
    # name, server certificate, URL host, verify, CA, verifyhost, success, SNI, fault
    cases = [
        ("valid-dns", "dns", "localhost", "1", "ca.crt", "none", True, "localhost", None),
        ("wrong-host-right-ca", "wrong", "localhost", "1", "ca.crt", "none", False, None, None),
        ("right-host-wrong-ca", "dns", "localhost", "1", "other-ca.crt", "none", False, None, None),
        ("san-overrides-matching-cn", "san-conflict", "localhost", "1", "ca.crt", "none", False, None, None),
        ("valid-ipv4-san", "ip", "127.0.0.1", "1", "ca.crt", "none", True, None, None),
        ("wrong-ipv4-san", "wrong-ip", "127.0.0.1", "1", "ca.crt", "none", False, None, None),
        ("dns-numeric-not-ip-san", "dns-ip", "127.0.0.1", "1", "ca.crt", "none", False, None, None),
        ("dns-override-ip-connection", "dns", "127.0.0.1", "1", "ca.crt", "localhost", True, "localhost", None),
        ("wrong-dns-override", "dns", "127.0.0.1", "1", "ca.crt", "other.invalid", False, None, None),
        ("ip-override-dns-connection", "ip", "localhost", "1", "ca.crt", "127.0.0.1", True, None, None),
        ("wrong-ip-override", "ip", "localhost", "1", "ca.crt", "127.0.0.2", False, None, None),
        ("valid-ipv6-san", "ip", "::1", "1", "ca.crt", "none", True, None, None),
        ("wrong-ipv6-san", "wrong-ip", "::1", "1", "ca.crt", "none", False, None, None),
        ("empty-verifyhost-fails-closed", "dns", "127.0.0.1", "1", "ca.crt", "", False, None, None),
        ("missing-ca-fails", "dns", "localhost", "1", "missing.crt", "none", False, None, None),
        ("malformed-ca-fails", "dns", "localhost", "1", "invalid.crt", "none", False, None, None),
        ("default-ca-untrusted-fails", "dns", "localhost", "1", "default-untrusted", "none", False, None, None),
        ("default-ca-path", "dns", "localhost", "1", "none", "none", True, "localhost", None),
        ("verify0-empty-verifyhost-compatible", "wrong", "127.0.0.1", "0", "ca.crt", "", True, None, None),
        ("verify0-wrong-host", "wrong", "localhost", "0", "ca.crt", "none", True, "localhost", None),
        ("verify0-wrong-ca", "dns", "localhost", "0", "other-ca.crt", "none", True, "localhost", None),
        ("verify0-missing-ca-compatible", "wrong", "localhost", "0", "missing.crt", "none", True, "localhost", None),
        ("default-remains-verify0", "wrong", "localhost", "default", "other-ca.crt", "none", True, "localhost", None),
        ("valid-full-wildcard", "wildcard", "127.0.0.1", "1", "ca.crt", "valid.fixture.invalid", True, "valid.fixture.invalid", None),
        ("wildcard-extra-label-rejected", "wildcard", "127.0.0.1", "1", "ca.crt", "deep.valid.fixture.invalid", False, None, None),
        ("partial-wildcard-rejected", "partial-wildcard", "127.0.0.1", "1", "ca.crt", "valid.fixture.invalid", False, None, None),
        ("fault-sni-configuration", "dns", "localhost", "1", "ca.crt", "none", False, None, "sni"),
        ("fault-host-configuration", "dns", "localhost", "1", "ca.crt", "none", False, None, "host"),
        ("fault-ip-configuration", "ip", "127.0.0.1", "1", "ca.crt", "none", False, None, "ip"),
        ("fault-default-ca-configuration", "dns", "localhost", "1", "none", "none", False, None, "default-ca"),
        ("fault-bio-method-allocation", "dns", "localhost", "1", "ca.crt", "none", False, None, "bio-method"),
        ("fault-bio-allocation", "dns", "localhost", "1", "ca.crt", "none", False, None, "bio"),
    ]
    with tempfile.TemporaryDirectory(prefix="ffmpeg-tls-") as tmp:
        root = Path(tmp)
        (root / "empty-ca-dir").mkdir()
        (root / "invalid.crt").write_text("not a certificate\n")
        for ca in ("ca", "other-ca"):
            command("openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-sha256",
                    "-keyout", str(root / (ca + ".key")), "-out", str(root / (ca + ".crt")),
                    "-subj", "/CN=Local TLS fixture " + ca, "-addext", "basicConstraints=critical,CA:TRUE")
        certificate(root, "dns", "DNS:localhost")
        certificate(root, "wrong", "DNS:other.invalid", cn="other.invalid")
        certificate(root, "san-conflict", "DNS:other.invalid")
        certificate(root, "ip", "IP:127.0.0.1,IP:::1")
        certificate(root, "wrong-ip", "IP:127.0.0.2,IP:::2")
        certificate(root, "dns-ip", "DNS:127.0.0.1", cn="127.0.0.1")
        certificate(root, "wildcard", "DNS:*.fixture.invalid")
        certificate(root, "partial-wildcard", "DNS:v*.fixture.invalid")
        certificate(root, "client", "DNS:client.invalid", usage="clientAuth")
        certificate(root, "untrusted-client", "DNS:client.invalid", signer="other-ca", usage="clientAuth")
        results = [run_case(binary, root, case) for case in cases
                   if args.case_filter in case[0] and not (args.ipv4_only and case[2] == "::1")]
        listen_cases = [("listen-trusted-client-unrelated-name", "client", True),
                        ("listen-untrusted-client", "untrusted-client", False),
                        ("listen-missing-client-certificate", None, False)]
        results += [run_listen_case(binary, root, *case) for case in listen_cases if args.case_filter in case[0]]
    if not results:
        parser.error("no test cases match --case-filter")
    report = {"binary": binary, "openssl_fixture_version": ssl.OPENSSL_VERSION,
              "total": len(results), "passed": sum(r["passed"] for r in results), "results": results}
    Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
    for result in results:
        print(("PASS" if result["passed"] else "FAIL") + " " + result["name"])
    print(f"{report['passed']}/{report['total']} passed")
    raise SystemExit(0 if report["passed"] == report["total"] else 1)


if __name__ == "__main__":
    main()
