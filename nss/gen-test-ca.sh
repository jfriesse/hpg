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
CA_SUBJECT="CN=QNet CA"
PWD_FILE="$DB_DIR/pwdfile.txt"
NOISE_FILE="$DB_DIR/noise.txt"
SERIAL_NO_FILE="$DB_DIR/serial.txt"

create_ca=false

if [ -f "$DB_DIR/cert8.db" ];then
    # Database exists. Take a look for CA cert
    if ! certutil -L -d "$DB_DIR" -n "$CA_NICKNAME" 2>/dev/null >&2; then
        echo "Creating new CA"
        create_ca=true
    else
        echo "Using existing CA"
    fi
else
    create_ca=true
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
# IS THIS NEEDED?
#    certutil -G -d "$DB_DIR" -z "$NOISE_FILE" -f "$PWD_FILE"
    # Create self-signed certificate (CA). Asks 3 questions (is this CA, lifetime and critical extension
    echo -e "y\n0\ny\n" | certutil -S -n "$CA_NICKNAME" -s "$CA_SUBJECT" -x \
        -t "CT,," -m `get_serial_no` -v $CRT_VALIDITY -d "$DB_DIR" \
        -z "$NOISE_FILE" -f "$PWD_FILE" -2
fi

exit 1

# 5. Generate the encryption key:
echo "Creating encryption key for CA"
#certutil -G $prefixarg -d $secdir -z $secdir/noise.txt -f $secdir/pwdfile.txt
# 6. Generate the self-signed certificate:
echo "Creating self-signed CA certificate"
# note - the basic constraints flag (-2) is required to generate a real CA cert
# it asks 3 questions that cannot be supplied on the command line
serialno=`getserialno`
( echo y ; echo ; echo y ) | certutil -S -n "$CA_NICKNAME" -s "cn=CAcert" -x -t "CT,," -m `get_serial_no` -v 120 -d $secdir -z $secdir/noise.txt -f $secdir/pwdfile.txt -2
# export the CA cert for use with other apps
echo Exporting the CA certificate to cacert.asc
certutil -L $prefixarg -d $secdir -n "CA certificate" -a > $secdir/cacert.asc
fi

exit 1

if [ -n "$needCA" -o -n "$needServerCert" -o -n "$needASCert" ] ; then
if [ -f $secdir/pwdfile.txt ] ; then
echo "Using existing $secdir/pwdfile.txt"
else
echo "Creating password file for security token"
(ps -ef ; w ) | sha1sum | awk '{print $1}' > $secdir/pwdfile.txt
if test -n "$isroot" ; then
chown $uid:$gid $secdir/pwdfile.txt
fi
chmod 400 $secdir/pwdfile.txt
fi

CRT_SERIAL=0
# Validity of certs


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

# Generate noise file
dd if=/dev/urandom of="$NOISE_FILE" bs=64 count=1 2>/dev/null

$CERTUTIL -R -d $DBDIR \
            -s "$SERVER_CERTDN" \
                        -o $DEST/tmpcertreq \
                                    -g $KEYSIZE \
                                                -z $DEST/noise \
                                                            -f $DEST/pw.txt