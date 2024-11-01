#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created and initialized
sh script/init_diskfile.sh
make
./bin/heartyfs_init

# Create test files and directories
./bin/heartyfs_mkdir /test_dir
./bin/heartyfs_creat /test_file.txt
./bin/heartyfs_creat /test_dir/nested_file.txt

# Create test content
echo "This is a test file for heartyfs_write." > external_file.txt
echo "This is a larger test file for heartyfs_write. It contains multiple lines of text to test writing larger amounts of data." > external_file_large.txt

# Test cases
echo "Test case 1: Write to a file at root"
./bin/heartyfs_write /test_file.txt external_file.txt
echo

echo "Test case 2: Write to a file in a directory"
./bin/heartyfs_write /test_dir/nested_file.txt external_file.txt
echo

echo "Test case 3: Overwrite existing content"
./bin/heartyfs_write /test_file.txt external_file_large.txt
echo

echo "Test case 4: Try to write to a non-existent file"
./bin/heartyfs_write /nonexistent_file.txt external_file.txt
echo

echo "Test case 5: Try to write from a non-existent external file"
./bin/heartyfs_write /test_file.txt nonexistent_external_file.txt
echo

# Clean up
rm external_file.txt external_file_large.txt

echo "Test completed."