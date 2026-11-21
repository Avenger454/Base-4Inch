"""Decode the checked-in GIF without third-party build dependencies."""
import base64
import hashlib
import pathlib
import sys

source, destination = map(pathlib.Path, sys.argv[1:])
data = base64.b64decode(b''.join(source.read_bytes().split()), validate=True)
if not data.startswith(b'GIF89a'):
    raise SystemExit('Invalid boot GIF header')
if hashlib.sha256(data).hexdigest() != '585218958ce27cb82c79e98c5dd329e1fa837a80148f5c8cbe4451b2316d26c4':
    raise SystemExit('Boot GIF checksum mismatch')
destination.write_bytes(data)
