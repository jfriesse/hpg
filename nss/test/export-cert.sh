#!/bin/bash

pk12util -d ../nssdb -o qnetdcert.pk12 -W "" -n "QNetd Cert"
openssl pkcs12  -nodes -clcerts -in qnetdcert.pk12  -out qnetdcert.pem -password 'pass:'
