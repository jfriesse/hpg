#!/bin/bash

openssl s_client -CAfile ../nssdb/cacert.crt -host 127.0.0.1
