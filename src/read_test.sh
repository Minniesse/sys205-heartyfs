#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created and initialized
rm -rf bin
sh script/init_diskfile.sh
make
./bin/heartyfs_init

# Create test files and directories
./bin/heartyfs_mkdir /test_dir
./bin/heartyfs_creat /test_file.txt
./bin/heartyfs_creat /test_dir/nested_file.txt

# Create test content
echo "This is a test file for heartyfs_read." > external_file.txt
echo "This is a larger test file for heartyfs_read. It contains multiple lines of text to test reading larger amounts of data." > external_file_large.txt

# Write content to heartyfs files
./bin/heartyfs_write /test_file.txt external_file.txt
./bin/heartyfs_write /test_dir/nested_file.txt external_file_large.txt

# Test cases
echo "Test case 1: Read a file at root"
./bin/heartyfs_read /test_file.txt
echo

echo "Test case 2: Read a file in a directory"
./bin/heartyfs_read /test_dir/nested_file.txt
echo

echo "Test case 3: Try to read a non-existent file"
./bin/heartyfs_read /nonexistent_file.txt
echo

echo "Test case 4: Try to read a directory (should fail)"
./bin/heartyfs_read /test_dir
echo

# Clean up
rm external_file.txt external_file_large.txt

echo "Test completed."