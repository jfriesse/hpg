#!/bin/bash

openssl s_server -CAfile ../nssdb/qnetd-cacert.crt -cert qnetdcert.pem -Verify 1
