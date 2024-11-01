#!/bin/bash

# Change to the root directory of the project
cd "$(dirname "$0")/.." || exit

# Ensure the disk file is created and initialized
rm -rf bin
bash script/init_diskfile.sh
make
./bin/heartyfs_init

# Set up test directories and files
./bin/heartyfs_mkdir /test_dir
./bin/heartyfs_creat /test_file.txt
./bin/heartyfs_creat /test_dir/nested_file.txt

# Test cases
echo "Test case 1: Remove a file at root"
./bin/heartyfs_rm /test_file.txt
echo

echo "Test case 2: Remove a file in a directory"
./bin/heartyfs_rm /test_dir/nested_file.txt
echo

echo "Test case 3: Try to remove a non-existent file"
./bin/heartyfs_rm /nonexistent_file.txt
echo

echo "Test case 4: Try to remove a directory (should fail)"
./bin/heartyfs_rm /test_dir
echo

echo "Test case 5: Create and remove multiple files"
./bin/heartyfs_creat /test_dir/file1.txt
./bin/heartyfs_creat /test_dir/file2.txt
./bin/heartyfs_creat /test_dir/file3.txt
./bin/heartyfs_rm /test_dir/file2.txt
./bin/heartyfs_rm /test_dir/file1.txt
./bin/heartyfs_rm /test_dir/file3.txt
echo

# Add more test cases as needed

echo "Test completed."