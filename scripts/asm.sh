# This Script is used to auto-generate .lst/.bin file
# EXAMPLE:
#	$ asm.sh test.asm
#	$ ls test.*
#	test.bin test.lst test.asm


#!/bin/bash

echo "This script only accept one argument."
file_nosuffix=$(echo $1 | awk -F. '{print $1}')
nasm -f bin -l ${file_nosuffix}".lst" -o ${file_nosuffix}".bin" $1 && echo "\`"${file_nosuffix}".lst' and \`"${file_nosuffix}".bin' has been generated."
