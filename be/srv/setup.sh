#!/bin/sh -x

rm -rf /tmp
ln -s /dev/shm /tmp

cd /var
rm -rf tmp
ln -s /tmp

cd /home
rm -rf golf
ln -s /tmp golf

chmod 755 /var/lock
