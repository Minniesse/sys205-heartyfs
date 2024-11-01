#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define FILE_TYPE 0
#define INITIAL_FILE_SIZE 0

/**
 * @brief Find the first available free block in the bitmap
 * @param[in] bitmap Pointer to the filesystem bitmap
 * @return Block number of the first free block, or -1 if no blocks are available
 */
int find_free_block(const char *bitmap) {
    for (int block_num = 2; block_num < NUM_BLOCK; block_num++) {
        int byte_index = block_num / 8;
        int bit_position = block_num % 8;
        
        if (bitmap[byte_index] & (1 << bit_position)) {
            return block_num;
        }
    }
    return -1;
}

/**
 * @brief Mark a block as used in the bitmap
 * @param[out] bitmap Pointer to the filesystem bitmap
 * @param[in] block Block number to mark as used
 */
void mark_block_used(char *bitmap, int block) {
    int byte_index = block / 8;
    int bit_position = block % 8;
    bitmap[byte_index] &= ~(1 << bit_position);
}

/**
 * @brief Find the parent directory for a given path
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] path Full path to analyze
 * @return Pointer to the parent directory, or NULL if not found
 */
struct heartyfs_directory *find_parent_dir(void *disk, char *path) {
    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *path_copy = strdup(path);
    if (!path_copy) {
        return NULL;
    }

    char *parent_path = dirname(path_copy);
    char *token = strtok(parent_path, "/");

    // Traverse the directory tree
    while (token != NULL) {
        int found = 0;
        // Search through current directory entries
        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                current_dir = (struct heartyfs_directory *)(disk + 
                    current_dir->entries[i].block_id * BLOCK_SIZE);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            free(path_copy);
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    free(path_copy);
    return current_dir;
}

/**
 * @brief Main function to create a new file in the filesystem
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

    // Find parent directory
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, file_path);
    if (!parent_dir) {
        fprintf(stderr, "Parent directory not found\n");
        goto cleanup;
    }

    // Check if parent directory is full
    if (parent_dir->size >= 14) {
        fprintf(stderr, "Parent directory is full\n");
        goto cleanup;
    }

    // Get file name from path
    char *file_name = basename(file_path);
    
    // Check if file already exists
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, file_name) == 0) {
            fprintf(stderr, "File already exists\n");
            goto cleanup;
        }
    }

    // Find free block for inode
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    int inode_block = find_free_block(bitmap);
    if (inode_block == -1) {
        fprintf(stderr, "No free blocks available\n");
        goto cleanup;
    }

    // Mark block as used
    mark_block_used(bitmap, inode_block);

    // Initialize new inode
    struct heartyfs_inode *new_inode = (struct heartyfs_inode *)
        (disk + inode_block * BLOCK_SIZE);
    memset(new_inode, 0, sizeof(struct heartyfs_inode));
    new_inode->type = FILE_TYPE;
    strncpy(new_inode->name, file_name, sizeof(new_inode->name) - 1);
    new_inode->name[sizeof(new_inode->name) - 1] = '\0';
    new_inode->size = INITIAL_FILE_SIZE;

    // Add entry to parent directory
    struct heartyfs_dir_entry *new_entry = &parent_dir->entries[parent_dir->size];
    new_entry->block_id = inode_block;
    strncpy(new_entry->file_name, file_name, sizeof(new_entry->file_name) - 1);
    new_entry->file_name[sizeof(new_entry->file_name) - 1] = '\0';
    parent_dir->size++;

    printf("File '%s' created successfully\n", file_name);
    munmap(disk, DISK_SIZE);
    close(fd);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    return 1;
}