#!/bin/sh

./cf_main cf_mount
rm -frv cf_mount/*
cd cf_mount
tar -zxvf /usr/portage/distfiles/fuse-2.3.0.tar.gz
cd fuse-2.3.0
#./configure
