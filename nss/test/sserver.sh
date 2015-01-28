#!/bin/bash

openssl s_server -CAfile ../nssdb/cacert.crt -cert qnetdcert.pem
