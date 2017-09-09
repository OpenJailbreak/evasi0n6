#!/bin/bash

ID=`echo "$1probably sha1 of their name + secret nonce, and then put that in the binary and also use that to decrypt some other piece of info." | openssl sha1`
echo $ID
