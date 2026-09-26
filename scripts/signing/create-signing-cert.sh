#!/usr/bin/env bash
# Run once, by hand, to create the self-signed code-signing certificate that
# .github/workflows/release.yml uses. Works in Git Bash, macOS and Linux.
#
#   scripts/signing/create-signing-cert.sh [output-dir]
#
# Env overrides: PFX_PASSWORD (else prompted), CERT_CN, CERT_DAYS (default 1095).
#
# Outputs (the directory is created with owner-only permissions):
#   key.pem, cert.pem          private key and certificate. Keep offline and back them up.
#   signing.pfx                key and cert bundle for signtool, codesign and openssl
#   signing.pfx.base64         -> GitHub secret SIGNING_CERT_PFX_BASE64
#   game-face-codesign.cer     public certificate for end users to trust. Not secret.
set -euo pipefail

out="${1:-signing-cert}"
cn="${CERT_CN:-game-face Code Signing (self-signed)}"
days="${CERT_DAYS:-1095}"

# Stop Git Bash (MSYS) from rewriting "/CN=..." into a Windows path.
export MSYS2_ARG_CONV_EXCL='*'

if [ -e "$out/signing.pfx" ]; then
    echo "error: $out/signing.pfx already exists; refusing to overwrite." >&2
    exit 1
fi

if [ -z "${PFX_PASSWORD:-}" ]; then
    read -rsp "PFX password: " PFX_PASSWORD; echo
    read -rsp "Repeat: " again; echo
    [ "$PFX_PASSWORD" = "$again" ] || { echo "error: passwords differ." >&2; exit 1; }
fi
[ -n "$PFX_PASSWORD" ] || { echo "error: empty password." >&2; exit 1; }
export PFX_PASSWORD

umask 077
mkdir -p "$out"

# Code signing only (EKU), so trusting this cert can't make it valid for TLS or email.
openssl req -x509 -newkey rsa:3072 -sha256 -days "$days" -nodes \
    -keyout "$out/key.pem" -out "$out/cert.pem" \
    -subj "/CN=$cn" \
    -addext "basicConstraints=critical,CA:FALSE" \
    -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=critical,codeSigning" \
    -addext "subjectKeyIdentifier=hash"

# 3DES/SHA1 PKCS#12 encryption: macOS `security import` rejects OpenSSL 3's AES default.
openssl pkcs12 -export \
    -inkey "$out/key.pem" -in "$out/cert.pem" -name "$cn" \
    -keypbe PBE-SHA1-3DES -certpbe PBE-SHA1-3DES -macalg sha1 \
    -passout env:PFX_PASSWORD -out "$out/signing.pfx"

openssl base64 -A -in "$out/signing.pfx" -out "$out/signing.pfx.base64"
openssl x509 -in "$out/cert.pem" -outform DER -out "$out/game-face-codesign.cer"

echo
openssl x509 -in "$out/cert.pem" -noout -subject -enddate -fingerprint -sha256
cat <<EOF

Next:
  1. Add GitHub repository secrets:
       SIGNING_CERT_PFX_BASE64  = contents of $out/signing.pfx.base64
       SIGNING_CERT_PASSWORD    = the password you just entered
  2. Store $out/ somewhere safe and offline, then delete it from this machine.
     Anyone with signing.pfx can sign code your users' machines will trust.
EOF
