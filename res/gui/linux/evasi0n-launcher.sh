#!/bin/sh

DIR=`dirname $0`
PLAT=`uname -m`
case $PLAT in
	x86_64)
	${DIR}/evasi0n.x86_64
	;;
	i386|i586|i686)
	${DIR}/evasi0n.x86
	;;
	*)
	echo Sorry, platform $PLAT is not supported.
	;;
esac
