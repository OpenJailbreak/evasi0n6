#!/bin/bash

for x in postrm extrainst_; do  
    clang -o $x -isysroot /Developer/iPhoneOS6.0.sdk -arch armv7 -miphoneos-version-min=6.0 -framework CoreFoundation -framework Foundation Cydia.mm $x.mm
done
