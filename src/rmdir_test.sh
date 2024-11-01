#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

rm -rf bin
# Ensure the disk file is created and initialized
bash script/init_diskfile.sh
make
./bin/heartyfs_init

# Set up test directories
./bin/heartyfs_mkdir /test_dir
./bin/heartyfs_mkdir /test_dir/nested_dir
./bin/heartyfs_mkdir /empty_dir

# Test cases
echo "Test case 1: Remove an empty directory"
./bin/heartyfs_rmdir /empty_dir
echo

echo "Test case 2: Try to remove a non-empty directory"
./bin/heartyfs_rmdir /test_dir
echo

echo "Test case 3: Remove a nested empty directory"
./bin/heartyfs_rmdir /test_dir/nested_dir
echo

echo "Test case 4: Try to remove a non-existent directory"
./bin/heartyfs_rmdir /nonexistent_dir
echo

echo "Test case 5: Remove the now-empty test_dir"
./bin/heartyfs_rmdir /test_dir
echo

# Add more test cases as needed

echo "Test completed."