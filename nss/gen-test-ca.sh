#!/bin/bash

create_new_pwd_file() {
    local pwd_file="$1"

    if [ ! -e "$pwd_file" ];then
        echo "Creating new $2 file $pwd_file"
        (ps -elf; date; w) | sha1sum | (read sha_sum rest; echo $sha_sum) > "$pwd_file"
        chown root:root "$pwd_file"
        chmod 400 "$pwd_file"
    else
        echo "Using existing $2 file $pwd_file"
    fi
}

get_serial_no() {
    local serial_no

    if ! [ -f "$SERIAL_NO_FILE" ];then
        echo "100" > $SERIAL_NO_FILE
    fi
    serial_no=`cat $SERIAL_NO_FILE`
    serial_no=$((serial_no+1))
    echo "$serial_no" > $SERIAL_NO_FILE
    echo "$serial_no"
}

DB_DIR=nssdb

if [ "$1" != "" ];then
    DB_DIR="$1"
fi

# Validity of certificate (months)
CRT_VALIDITY=120
CA_NICKNAME="QNet CA"
SERVER_NICKNAME="QNetd Cert"
CA_SUBJECT="CN=QNet CA"
SERVER_SUBJECT="CN=Qnetd Server"
PWD_FILE="$DB_DIR/pwdfile.txt"
NOISE_FILE="$DB_DIR/noise.txt"
SERIAL_NO_FILE="$DB_DIR/serial.txt"
CA_EXPORT_FILE="$DB_DIR/cacert.crt"

create_ca=false
create_server_crt=false

if [ -f "$DB_DIR/cert8.db" ];then
    # Database exists. Take a look for CA cert
    if ! certutil -L -d "$DB_DIR" -n "$CA_NICKNAME" 2>/dev/null >&2; then
        echo "Creating new CA"
        create_ca=true
    else
        echo "Using existing CA"
    fi
    if ! certutil -L -d "$DB_DIR" -n "$SERVER_NICKNAME" 2>/dev/null >&2; then
        echo "Creating new server certificate"
        create_server_crt=true
    else
        echo "Using existing server certificate"
    fi
else
    create_ca=true
    create_server_crt=true
fi

# Create password and noise file if doesn't exists yet
create_new_pwd_file "$PWD_FILE" "password"
create_new_pwd_file "$NOISE_FILE" "noise"

if ! [ -f "$DB_DIR/cert8.db" ];then
    echo "Creating new key and cert db"
    certutil -N -d "$DB_DIR" -f "$PWD_FILE"
    chown root:root "$DB_DIR/key3.db" "$DB_DIR/cert8.db" "$DB_DIR/secmod.db"
    chmod 600 "$DB_DIR/key3.db" "$DB_DIR/cert8.db" "$DB_DIR/secmod.db"
fi

if $create_ca;then
    echo "Creating new CA"
    # Create self-signed certificate (CA). Asks 3 questions (is this CA, lifetime and critical extension
    echo -e "y\n0\ny\n" | certutil -S -n "$CA_NICKNAME" -s "$CA_SUBJECT" -x \
        -t "CT,," -m `get_serial_no` -v $CRT_VALIDITY -d "$DB_DIR" \
        -z "$NOISE_FILE" -f "$PWD_FILE" -2
    # Export CA certificate in ascii
    certutil -L -d "$DB_DIR" -n "$CA_NICKNAME" > "$CA_EXPORT_FILE"
    certutil -L -d "$DB_DIR" -n "$CA_NICKNAME" -a >> "$CA_EXPORT_FILE"
fi

if $create_server_crt;then
    certutil -S -n "$SERVER_NICKNAME" -s "$SERVER_SUBJECT" -c "$CA_NICKNAME" -t "u,u,u" -m `get_serial_no` \
        -v $CRT_VALIDITY -d "$DB_DIR" -z "$NOISE_FILE" -f "$PWD_FILE"
fi
