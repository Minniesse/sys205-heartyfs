#include "../heartyfs.h"
#include <string.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define FILE_TYPE 0
#define READ_ERROR -1
#define READ_SUCCESS 0

/**
 * @brief Find and validate a file in the filesystem
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] path Path to the file
 * @return Pointer to the file's inode, or NULL if not found or invalid
 * 
 * Traverses the filesystem hierarchy to find the specified file.
 * Validates that the found inode is a regular file.
 */
struct heartyfs_inode *find_file(void *disk, const char *path) {
    if (!disk || !path) {
        return NULL;
    }

    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';

    char *token = strtok(path_copy, "/");
    char *next_token;

    // Traverse the path components
    while (token != NULL) {
        next_token = strtok(NULL, "/");
        int found = 0;

        // Search current directory entries
        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                void *next_block = disk + current_dir->entries[i].block_id * BLOCK_SIZE;

                if (next_token == NULL) {
                    // This is the target file - verify it's a regular file
                    struct heartyfs_inode *file_inode = (struct heartyfs_inode *)next_block;
                    if (file_inode->type != FILE_TYPE) {
                        return NULL;  // Not a regular file
                    }
                    return file_inode;
                }

                // Move to next directory
                current_dir = (struct heartyfs_directory *)next_block;
                found = 1;
                break;
            }
        }

        if (!found) {
            return NULL;  // Path component not found
        }
        token = next_token;
    }

    return NULL;  // Path ended at a directory
}

/**
 * @brief Read and output file contents to stdout
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] file_inode Pointer to the file's inode
 * @return READ_SUCCESS on success, READ_ERROR on failure
 */
int read_file_contents(void *disk, struct heartyfs_inode *file_inode) {
    if (!disk || !file_inode) {
        return READ_ERROR;
    }

    // Read each data block
    for (int i = 0; i < file_inode->size; i++) {
        struct heartyfs_data_block *data_block = (struct heartyfs_data_block *)
            (disk + file_inode->data_blocks[i] * BLOCK_SIZE);

        // Validate data block size
        if (data_block->size > BLOCK_SIZE - sizeof(int)) {
            fprintf(stderr, "Corrupted data block size\n");
            return READ_ERROR;
        }

        // Write block contents to stdout
        if (fwrite(data_block->data, 1, data_block->size, stdout) != data_block->size) {
            perror("Error writing file contents");
            return READ_ERROR;
        }
    }

    return READ_SUCCESS;
}

/**
 * @brief Main function to read and display a file's contents
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @return 0 on success, 1 on failure
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    // Validate and copy path
    char file_path[MAX_PATH_LENGTH];
    if (strlen(argv[1]) >= MAX_PATH_LENGTH) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }
    strncpy(file_path, argv[1], MAX_PATH_LENGTH - 1);
    file_path[MAX_PATH_LENGTH - 1] = '\0';

    // Open filesystem
    int fd = open(DISK_FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Cannot open the disk file");
        return 1;
    }

    // Map filesystem to memory
    void *disk = mmap(NULL, DISK_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        return 1;
    }

    // Find and validate the file
    struct heartyfs_inode *file_inode = find_file(disk, file_path);
    if (!file_inode) {
        fprintf(stderr, "File not found or not a regular file\n");
        goto cleanup;
    }

    // Read and output file contents
    if (read_file_contents(disk, file_inode) != READ_SUCCESS) {
        goto cleanup;
    }

    munmap(disk, DISK_SIZE);
    close(fd);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    return 1;
}