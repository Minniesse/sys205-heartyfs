#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created and initialized
rm -rf bin
bash script/init_diskfile.sh
make
./bin/heartyfs_init

# Set up test directories
./bin/heartyfs_mkdir /test_dir

# Test cases
echo "Test case 1: Create a file at root"
./bin/heartyfs_creat /test_file.txt
echo

echo "Test case 2: Create a file in a directory"
./bin/heartyfs_creat /test_dir/nested_file.txt
echo

echo "Test case 3: Try to create an existing file"
./bin/heartyfs_creat /test_file.txt
echo

echo "Test case 4: Try to create a file in a non-existent directory"
./bin/heartyfs_creat /nonexistent_dir/test_file.txt
echo

echo "Test case 5: Create multiple files in the same directory"
./bin/heartyfs_creat /test_dir/file1.txt
./bin/heartyfs_creat /test_dir/file2.txt
./bin/heartyfs_creat /test_dir/file3.txt
echo

# Add more test cases as needed

echo "Test completed."