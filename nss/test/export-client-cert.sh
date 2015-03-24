#!/bin/bash

openssl pkcs12  -nodes -clcerts -in ../node/nssdb/qnetd-node.p12  -out qnetd-node.pem -password 'pass:'
