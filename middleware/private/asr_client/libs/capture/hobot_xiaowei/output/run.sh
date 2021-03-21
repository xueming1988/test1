#!/bin/bash
rm hobotspeechdemo.zip
zip -r hobotspeechdemo.zip hobotspeechdemo
rm -rf /mnt/hgfs/D/output/*
cp ./hobotspeechdemo.zip /mnt/hgfs/D/output/
