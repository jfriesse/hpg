#!/bin/bash

DB_DIR=nssdb
NOISE_FILE="$DB_DIR/noise"
CRT_SERIAL=0
CA_NICKNAME="qnet-ca"
CA_SUBJECT="CN=QNet CA"
# Validity of certs
CRT_VALIDITY=120

rm -f "$DB_DIR/cert8.db" "$DB_DIR/key3.db" "$DB_DIR/secmod.db"
# Generate database. Use empty password
certutil -N -d "$DB_DIR" -f /dev/zero
# Generate noise file
dd if=/dev/urandom of="$NOISE_FILE" bs=64 count=1 2>/dev/null

certutil -S -s "CN=My Issuer" -n myissuer -x -t "C,C,C" -1 -2 -5 -m 1234 -f password-file -d certdir 

CRT_SERIAL=$((CRT_SERIAL+1))
# Create self-signed CA cert and import it to database

# 5 9 n - Cert signing key
# y 0 y - path lenght contstaints
# 5 9 n -> SSL
echo -e "5\n9\nn\n" \
"y\n0\ny\n" \
"5\n9\nn\n" | \
certutil -S -d "$DB_DIR" -n "$CA_NICKNAME" -s "$CA_SUBJECT" -x -t "C,," -m $CRT_SERIAL -v $CRT_VALIDITY \
  -f /dev/zero -z "$NOISE_FILE" -1 -2 -5

