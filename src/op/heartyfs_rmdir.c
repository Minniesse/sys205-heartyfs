#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define DIR_TYPE 1
#define MIN_DIR_ENTRIES 2  // . and ..
#define NOT_FOUND -1
#define REMOVE_ERROR -1
#define REMOVE_SUCCESS 0
#define ROOT_DIR "/"

/**
 * @brief Mark a block as free in the bitmap
 * @param[out] bitmap Pointer to the filesystem bitmap
 * @param[in] block Block number to mark as free
 * @return void
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
 * @brief Find the parent directory and extract directory name from path
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] path Full path to analyze
 * @param[out] dir_name Pointer to store the extracted directory name
 * @return Pointer to parent directory, or NULL if not found
 */
struct heartyfs_directory *find_parent_dir(void *disk, char *path, char **dir_name) {
    if (!disk || !path || !dir_name) {
        return NULL;
    }

    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *dir_path = strdup(path);
    if (!dir_path) {
        return NULL;
    }

    char *parent_path = dirname(dir_path);
    *dir_name = basename(path);
    
    // Handle root directory case
    if (strcmp(parent_path, ROOT_DIR) == 0 && strcmp(*dir_name, ROOT_DIR) == 0) {
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
 * @brief Find the index and block ID of a directory in its parent
 * @param[in] parent_dir Pointer to the parent directory
 * @param[in] dir_name Name of the directory to find
 * @param[out] dir_index Pointer to store the directory's index
 * @param[out] dir_block Pointer to store the directory's block number
 * @return REMOVE_SUCCESS if found, REMOVE_ERROR if not found
 */
int find_dir_entry(struct heartyfs_directory *parent_dir, const char *dir_name,
                   int *dir_index, int *dir_block) {
    if (!parent_dir || !dir_name || !dir_index || !dir_block) {
        return REMOVE_ERROR;
    }

    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, dir_name) == 0) {
            *dir_index = i;
            *dir_block = parent_dir->entries[i].block_id;
            return REMOVE_SUCCESS;
        }
    }
    
    return REMOVE_ERROR;
}

/**
 * @brief Validate if a directory can be removed
 * @param[in] dir Pointer to the directory to check
 * @return REMOVE_SUCCESS if can be removed, REMOVE_ERROR if not
 */
int validate_directory_removal(struct heartyfs_directory *dir) {
    if (!dir) {
        return REMOVE_ERROR;
    }

    // Check if it's a directory
    if (dir->type != DIR_TYPE) {
        fprintf(stderr, "Not a directory\n");
        return REMOVE_ERROR;
    }

    // Check if directory is empty (only contains . and ..)
    if (dir->size > MIN_DIR_ENTRIES) {
        fprintf(stderr, "Directory is not empty\n");
        return REMOVE_ERROR;
    }

    return REMOVE_SUCCESS;
}

/**
 * @brief Main function to remove a directory from the filesystem
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

    // Find parent directory and directory name
    char *dir_name;
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, dir_path, &dir_name);
    if (!parent_dir) {
        fprintf(stderr, "Parent directory not found\n");
        goto cleanup;
    }

    // Find directory in parent
    int dir_index, dir_block;
    if (find_dir_entry(parent_dir, dir_name, &dir_index, &dir_block) != REMOVE_SUCCESS) {
        fprintf(stderr, "Directory not found\n");
        goto cleanup;
    }

    // Get directory block and validate removal
    struct heartyfs_directory *dir_to_remove = 
        (struct heartyfs_directory *)(disk + dir_block * BLOCK_SIZE);
    if (validate_directory_removal(dir_to_remove) != REMOVE_SUCCESS) {
        goto cleanup;
    }

    // Free directory block
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    mark_block_free(bitmap, dir_block);

    // Remove directory entry from parent directory
    if (dir_index < parent_dir->size - 1) {
        // Move last entry to fill the gap
        parent_dir->entries[dir_index] = parent_dir->entries[parent_dir->size - 1];
    }
    parent_dir->size--;

    // Clear the directory block
    memset(dir_to_remove, 0, BLOCK_SIZE);

    printf("Directory '%s' removed successfully\n", dir_name);
    munmap(disk, DISK_SIZE);
    close(fd);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    return 1;
}