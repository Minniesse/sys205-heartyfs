#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created
sh script/init_diskfile.sh

# Compile and run heartyfs_init
make
./bin/heartyfs_init

# Compile and run the test
gcc -o bin/test_init src/test_init.c
./bin/test_init

# Clean up
rm bin/test_init

echo "Test completed."