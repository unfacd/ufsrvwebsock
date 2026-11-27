#!/usr/bin/env bash
#
# ufsrv_console_gen_client_cert.sh
#
# Generate a self-signed client certificate + key for the ufsrv command console
# and print its SPKI (SubjectPublicKeyInfo) SHA-256 hash — the value the server's
# ssl_command_console_allow_list_* fields pin.
#
# Usage:
#   ufsrv_console_gen_client_cert.sh <role> [-o OUTDIR] [-f]
#
#   <role>      client role name (god, browser, admin, ...). Files are named
#               ufsrv_console_client_<role>.{key,pem}; CN = ufsrv-console-<role>.
#   -o OUTDIR   directory for the .key/.pem (default: current directory).
#   -f, --force overwrite an existing key/cert pair (default: refuse).
#
# The printed hash is the SHA-256 of the DER SubjectPublicKeyInfo — exactly what
# OpenSSL's X509_pubkey_digest(peer, EVP_sha256(), ...) computes server-side.
# It is NOT the cert fingerprint (X509_digest) and not the raw key bytes.

set -euo pipefail

usage() {
  cat <<'EOF'
usage: ufsrv_console_gen_client_cert.sh <role> [-o OUTDIR] [-f]

  <role>      client role name (god, browser, admin, ...)
  -o OUTDIR   output directory (default: .)
  -f, --force overwrite an existing key/cert pair
EOF
}

ROLE=""
OUTDIR="."
FORCE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -f|--force) FORCE=1 ;;
    -o|--outdir) OUTDIR="${2:?error: missing value for $1}"; shift ;;
    *)
      if [[ -z "$ROLE" ]]; then
        ROLE="$1"
      else
        echo "error: unexpected argument '$1'" >&2
        usage >&2
        exit 2
      fi
      ;;
  esac
  shift
done

if [[ -z "$ROLE" ]]; then
  usage >&2
  exit 2
fi

# Keep the derived names safe for a filename and a CN.
if [[ "$ROLE" =~ [^a-zA-Z0-9_-] ]]; then
  echo "error: role must contain only [a-zA-Z0-9_-] (got '$ROLE')" >&2
  exit 2
fi

BASE="ufsrv_console_client_${ROLE}"
CN="ufsrv-console-${ROLE}"
KEY="${OUTDIR}/${BASE}.key"
CERT="${OUTDIR}/${BASE}.pem"

if [[ ( -e "$KEY" || -e "$CERT" ) && "$FORCE" -ne 1 ]]; then
  echo "error: '${BASE}' already exists — re-run with --force to overwrite." >&2
  exit 1
fi

mkdir -p "$OUTDIR"

# 1. Self-signed client cert + key (same style as the server console cert).
openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -keyout "$KEY" \
  -out    "$CERT" \
  -subj   "/CN=${CN}" >/dev/null

chmod 600 "$KEY"
chmod 644 "$CERT"

# 2. SPKI SHA-256 hash — the allow-list entry (== X509_pubkey_digest, EVP_sha256).
HASH="$(
  openssl x509 -in "$CERT" -pubkey -noout \
  | openssl pkey -pubin -outform DER \
  | openssl dgst -sha256 \
  | awk '{print $NF}'
)"

# 3. Report.
cat <<EOF

=== ufsrv command console — client identity ===
  role  : ${ROLE}
  CN    : ${CN}
  key   : ${KEY}   (0600 — keep private)
  cert  : ${CERT}  (0644)

  allow-list entry (SPKI SHA-256):
  ${HASH}
==============================================
EOF
