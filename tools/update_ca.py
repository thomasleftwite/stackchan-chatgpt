"""Build a small public-root PEM bundle from Mozilla certifi data (no private keys)."""
from pathlib import Path
import re
import ssl
import tempfile
import certifi

ROOT = Path(__file__).resolve().parents[1]
PREFIXES = ("GTS Root ", "ISRG Root ", "DigiCert Global Root ", "Amazon Root CA ")
selected = []
names = []
source = Path(certifi.where()).read_text(encoding="utf-8")
for pem in re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", source, re.S):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".pem", dir=ROOT / "tools", delete=False) as tmp:
        tmp.write(pem + "\n")
        path = Path(tmp.name)
    try:
        cert = ssl._ssl._test_decode_cert(str(path))
    finally:
        path.unlink()
    cn = next((v for rdn in cert["subject"] for k, v in rdn if k == "commonName"), "")
    if cn.startswith(PREFIXES):
        selected.append(pem + "\n")
        names.append(cn)
if not any(n == "GTS Root R4" for n in names):
    raise SystemExit("Required root missing; bundle was not changed")
(ROOT / "data" / "ca.pem").write_text("\n".join(selected), encoding="ascii")
print("Included public CA roots:", ", ".join(names))
