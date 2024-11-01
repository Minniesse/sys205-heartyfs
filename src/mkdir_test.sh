#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created and initialized
rm -rf bin
sh script/init_diskfile.sh
make
./bin/heartyfs_init

# Test cases
echo "Test case 1: Create a directory at root"
./bin/heartyfs_mkdir /test_dir
echo

echo "Test case 2: Create a nested directory"
./bin/heartyfs_mkdir /test_dir/nested_dir
echo

echo "Test case 3: Try to create an existing directory"
./bin/heartyfs_mkdir /test_dir
echo

echo "Test case 4: Try to create a directory with an invalid path"
./bin/heartyfs_mkdir /nonexistent/test_dir
echo

# Add more test cases as needed

echo "Test completed."