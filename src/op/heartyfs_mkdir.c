#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define DIR_TYPE 1
#define INITIAL_DIR_ENTRIES 2  // . and ..
#define MAX_DIR_ENTRIES 14
#define CURRENT_DIR "."
#define PARENT_DIR ".."

/**
 * @brief Find the first available free block in the bitmap
 * @param[in] bitmap Pointer to the filesystem bitmap
 * @return Block number of the first free block, or -1 if no blocks available
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
 * @brief Initialize a new directory structure
 * @param[out] new_dir Pointer to the directory structure to initialize
 * @param[in] dir_name Name of the directory
 * @param[in] dir_block Block number of this directory
 * @param[in] parent_block Block number of the parent directory
 */
void init_directory(struct heartyfs_directory *new_dir, const char *dir_name, 
                   int dir_block, int parent_block) {
    memset(new_dir, 0, sizeof(struct heartyfs_directory));
    
    // Set directory attributes
    new_dir->type = DIR_TYPE;
    strncpy(new_dir->name, dir_name, sizeof(new_dir->name) - 1);
    new_dir->name[sizeof(new_dir->name) - 1] = '\0';
    new_dir->size = INITIAL_DIR_ENTRIES;

    // Initialize current directory entry (.)
    new_dir->entries[0].block_id = dir_block;
    strncpy(new_dir->entries[0].file_name, CURRENT_DIR, 
            sizeof(new_dir->entries[0].file_name) - 1);

    // Initialize parent directory entry (..)
    new_dir->entries[1].block_id = parent_block;
    strncpy(new_dir->entries[1].file_name, PARENT_DIR, 
            sizeof(new_dir->entries[1].file_name) - 1);
}

/**
 * @brief Main function to create a new directory in the filesystem
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @return 0 on success, 1 on failure
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Validate and copy path
    char dir_path[MAX_PATH_LENGTH];
    if (strlen(argv[1]) >= MAX_PATH_LENGTH) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }
    strncpy(dir_path, argv[1], MAX_PATH_LENGTH - 1);
    dir_path[MAX_PATH_LENGTH - 1] = '\0';

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
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, dir_path);
    if (!parent_dir) {
        fprintf(stderr, "Parent directory not found\n");
        goto cleanup;
    }

    // Check if parent directory is full
    if (parent_dir->size >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "Parent directory is full\n");
        goto cleanup;
    }

    // Get directory name from path
    char *dir_name = basename(dir_path);
    
    // Check if directory already exists
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, dir_name) == 0) {
            fprintf(stderr, "Directory '%s' already exists\n", dir_name);
            goto cleanup;
        }
    }

    // Find free block for new directory
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    int new_block = find_free_block(bitmap);
    if (new_block == -1) {
        fprintf(stderr, "No free blocks available\n");
        goto cleanup;
    }

    // Mark block as used
    mark_block_used(bitmap, new_block);

    // Initialize new directory
    struct heartyfs_directory *new_dir = (struct heartyfs_directory *)
        (disk + new_block * BLOCK_SIZE);
    int parent_block = ((void *)parent_dir - disk) / BLOCK_SIZE;
    init_directory(new_dir, dir_name, new_block, parent_block);

    // Add entry to parent directory
    struct heartyfs_dir_entry *new_entry = &parent_dir->entries[parent_dir->size];
    new_entry->block_id = new_block;
    strncpy(new_entry->file_name, dir_name, sizeof(new_entry->file_name) - 1);
    new_entry->file_name[sizeof(new_entry->file_name) - 1] = '\0';
    parent_dir->size++;

    printf("Directory '%s' created successfully\n", dir_name);
    munmap(disk, DISK_SIZE);
    close(fd);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    return 1;
}