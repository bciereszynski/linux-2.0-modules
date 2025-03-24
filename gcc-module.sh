#!/bin/bash

file="$1"

gcc -D__KERNEL__ -DMODULE -O2 -c "$file"
