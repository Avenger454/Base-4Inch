"""Decode and verify the transport-safe native boot animation asset."""
import base64
import hashlib
import pathlib
import sys

EXPECTED_SHA256 = "b45adaafbdb69789fdc6c1d939ee70399e1092e06ea20b7f55c33ad4c9b72b52"

source, destination = map(pathlib.Path, sys.argv[1:])
data = base64.b64decode(b"".join(source.read_bytes().split()), validate=True)
if not data.startswith(b"VPB1"):
    raise SystemExit("Invalid native boot animation header")
if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
    raise SystemExit("Boot animation checksum mismatch")
destination.write_bytes(data)
