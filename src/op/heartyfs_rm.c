#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define FILE_TYPE 0
#define NOT_FOUND -1
#define BLOCK_NOT_FOUND -1
#define REMOVE_ERROR -1
#define REMOVE_SUCCESS 0

/**
 * @brief Mark a block as free in the bitmap
 * @param[out] bitmap Pointer to the filesystem bitmap
 * @param[in] block Block number to mark as free
 */
void mark_block_free(char *bitmap, int block) {
    if (!bitmap || block < 0) {
        return;
    }
    
    int byte_index = block / 8;
    int bit_position = block % 8;
    bitmap[byte_index] |= (1 << bit_position);
}

/**
 * @brief Find the parent directory and extract file name from path
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] path Full path to analyze
 * @param[out] file_name Pointer to store the extracted file name
 * @return Pointer to parent directory, or NULL if not found
 */
struct heartyfs_directory *find_parent_dir(void *disk, char *path, char **file_name) {
    if (!disk || !path || !file_name) {
        return NULL;
    }

    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *dir_path = strdup(path);
    if (!dir_path) {
        return NULL;
    }

    char *parent_path = dirname(dir_path);
    *file_name = basename(path);
    
    // Handle root directory case
    if (strcmp(parent_path, "/") == 0 && strcmp(*file_name, "/") == 0) {
        free(dir_path);
        return NULL;  // Can't remove root directory
    }

    // Traverse the path to parent directory
    char *token = strtok(parent_path, "/");
    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                current_dir = (struct heartyfs_directory *)(disk + 
                    current_dir->entries[i].block_id * BLOCK_SIZE);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            free(dir_path);
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    free(dir_path);
    return current_dir;
}

/**
 * @brief Find the index and block ID of a file in a directory
 * @param[in] parent_dir Pointer to the parent directory
 * @param[in] file_name Name of the file to find
 * @param[out] file_index Pointer to store the file's index
 * @param[out] file_block Pointer to store the file's block number
 * @return REMOVE_SUCCESS if found, REMOVE_ERROR if not found
 */
int find_file_entry(struct heartyfs_directory *parent_dir, const char *file_name,
                    int *file_index, int *file_block) {
    if (!parent_dir || !file_name || !file_index || !file_block) {
        return REMOVE_ERROR;
    }

    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, file_name) == 0) {
            *file_index = i;
            *file_block = parent_dir->entries[i].block_id;
            return REMOVE_SUCCESS;
        }
    }
    
    return REMOVE_ERROR;
}

/**
 * @brief Free all data blocks associated with a file
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] file_inode Pointer to the file's inode
 * @param[out] bitmap Pointer to the filesystem bitmap
 */
void free_data_blocks(void *disk, struct heartyfs_inode *file_inode, char *bitmap) {
    if (!disk || !file_inode || !bitmap) {
        return;
    }

    for (int i = 0; i < file_inode->size; i++) {
        mark_block_free(bitmap, file_inode->data_blocks[i]);
    }
}

/**
 * @brief Main function to remove a file from the filesystem
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
    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        return 1;
    }

    // Map filesystem to memory
    void *disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        return 1;
    }

    // Find parent directory and file name
    char *file_name;
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, file_path, &file_name);
    if (!parent_dir) {
        fprintf(stderr, "Parent directory not found\n");
        goto cleanup;
    }

    // Find file in parent directory
    int file_index, file_block;
    if (find_file_entry(parent_dir, file_name, &file_index, &file_block) != REMOVE_SUCCESS) {
        fprintf(stderr, "File not found\n");
        goto cleanup;
    }

    // Verify it's a regular file
    struct heartyfs_inode *file_inode = (struct heartyfs_inode *)(disk + file_block * BLOCK_SIZE);
    if (file_inode->type != FILE_TYPE) {
        fprintf(stderr, "Not a regular file\n");
        goto cleanup;
    }

    // Free all data blocks
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    free_data_blocks(disk, file_inode, bitmap);

    // Free inode block
    mark_block_free(bitmap, file_block);

    // Remove file entry from parent directory
    if (file_index < parent_dir->size - 1) {
        // Move last entry to fill the gap
        parent_dir->entries[file_index] = parent_dir->entries[parent_dir->size - 1];
    }
    parent_dir->size--;

    // Clear the inode
    memset(file_inode, 0, BLOCK_SIZE);

    printf("File '%s' removed successfully\n", file_name);
    munmap(disk, DISK_SIZE);
    close(fd);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    return 1;
}