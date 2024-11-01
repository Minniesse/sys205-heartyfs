#include "../heartyfs.h"
#include <string.h>
#include <sys/stat.h>

/* Constants */
#define MAX_PATH_LENGTH 256
#define FILE_TYPE 0
#define DATA_BLOCK_HEADER_SIZE sizeof(int)
#define MAX_DATA_BLOCK_SIZE (BLOCK_SIZE - DATA_BLOCK_HEADER_SIZE)
#define MAX_FILE_SIZE (119 * MAX_DATA_BLOCK_SIZE)  // Based on inode structure
#define WRITE_ERROR -1
#define WRITE_SUCCESS 0

/**
 * @brief Find the first available free block in the bitmap
 * @param[in] bitmap Pointer to the filesystem bitmap
 * @return Block number of first free block, or -1 if no blocks available
 */
int find_free_block(const char *bitmap) {
    if (!bitmap) {
        return WRITE_ERROR;
    }

    for (int block_num = 2; block_num < NUM_BLOCK; block_num++) {
        int byte_index = block_num / 8;
        int bit_position = block_num % 8;
        
        if (bitmap[byte_index] & (1 << bit_position)) {
            return block_num;
        }
    }
    return WRITE_ERROR;
}

/**
 * @brief Mark a block as used in the bitmap
 * @param[out] bitmap Pointer to the filesystem bitmap
 * @param[in] block Block number to mark as used
 */
void mark_block_used(char *bitmap, int block) {
    if (!bitmap || block < 0) {
        return;
    }

    int byte_index = block / 8;
    int bit_position = block % 8;
    bitmap[byte_index] &= ~(1 << bit_position);
}

/**
 * @brief Find a file in the filesystem
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] path Path to the file
 * @return Pointer to the file's inode, or NULL if not found
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

    while (token != NULL) {
        next_token = strtok(NULL, "/");
        int found = 0;

        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                if (next_token == NULL) {
                    // This is the target file
                    return (struct heartyfs_inode *)(disk + 
                        current_dir->entries[i].block_id * BLOCK_SIZE);
                }
                current_dir = (struct heartyfs_directory *)(disk + 
                    current_dir->entries[i].block_id * BLOCK_SIZE);
                found = 1;
                break;
            }
        }

        if (!found) {
            return NULL;
        }
        token = next_token;
    }

    return NULL;
}

/**
 * @brief Free existing data blocks of a file
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] file_inode Pointer to the file's inode
 * @param[out] bitmap Pointer to the filesystem bitmap
 */
void free_existing_blocks(void *disk, struct heartyfs_inode *file_inode, char *bitmap) {
    if (!disk || !file_inode || !bitmap) {
        return;
    }

    for (int i = 0; i < file_inode->size; i++) {
        mark_block_used(bitmap, file_inode->data_blocks[i]);
        memset(disk + file_inode->data_blocks[i] * BLOCK_SIZE, 0, BLOCK_SIZE);
    }
}

/**
 * @brief Write file contents from external file to heartyfs
 * @param[in] disk Pointer to the filesystem in memory
 * @param[in] file_inode Pointer to the file's inode
 * @param[in] ext_file File pointer to external file
 * @param[in] file_size Size of the external file
 * @param[out] bitmap Pointer to the filesystem bitmap
 * @return WRITE_SUCCESS on success, WRITE_ERROR on failure
 */
int write_file_contents(void *disk, struct heartyfs_inode *file_inode, 
                       FILE *ext_file, off_t file_size, char *bitmap) {
    if (!disk || !file_inode || !ext_file || !bitmap) {
        return WRITE_ERROR;
    }

    size_t bytes_written = 0;
    int block_index = 0;

    while (bytes_written < file_size) {
        // Allocate new block
        int new_block = find_free_block(bitmap);
        if (new_block == WRITE_ERROR) {
            fprintf(stderr, "No free blocks available\n");
            return WRITE_ERROR;
        }
        mark_block_used(bitmap, new_block);
        file_inode->data_blocks[block_index] = new_block;

        // Write data to block
        struct heartyfs_data_block *data_block = 
            (struct heartyfs_data_block *)(disk + new_block * BLOCK_SIZE);
        size_t bytes_to_write = (file_size - bytes_written < MAX_DATA_BLOCK_SIZE) ? 
                               (file_size - bytes_written) : MAX_DATA_BLOCK_SIZE;
        
        data_block->size = bytes_to_write;
        if (fread(data_block->data, 1, bytes_to_write, ext_file) != bytes_to_write) {
            fprintf(stderr, "Error reading from external file\n");
            return WRITE_ERROR;
        }
        
        bytes_written += bytes_to_write;
        block_index++;
    }

    file_inode->size = block_index;
    return WRITE_SUCCESS;
}

/**
 * @brief Main function to write file contents from external file to heartyfs
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @return 0 on success, 1 on failure
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <heartyfs_file_path> <external_file_path>\n", 
                argv[0]);
        return 1;
    }

    // Validate and copy heartyfs path
    char heartyfs_path[MAX_PATH_LENGTH];
    if (strlen(argv[1]) >= MAX_PATH_LENGTH) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }
    strncpy(heartyfs_path, argv[1], MAX_PATH_LENGTH - 1);
    heartyfs_path[MAX_PATH_LENGTH - 1] = '\0';

    // Open and validate external file
    const char *external_path = argv[2];
    FILE *ext_file = fopen(external_path, "rb");
    if (!ext_file) {
        perror("Cannot open external file");
        return 1;
    }

    // Get external file size
    struct stat st;
    if (stat(external_path, &st) != 0) {
        perror("Cannot get external file size");
        fclose(ext_file);
        return 1;
    }

    // Validate file size
    off_t file_size = st.st_size;
    if (file_size > MAX_FILE_SIZE) {
        fprintf(stderr, "External file is too large for heartyfs\n");
        fclose(ext_file);
        return 1;
    }

    // Open filesystem
    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        fclose(ext_file);
        return 1;
    }

    // Map filesystem to memory
    void *disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        fclose(ext_file);
        return 1;
    }

    // Find and validate the file
    struct heartyfs_inode *file_inode = find_file(disk, heartyfs_path);
    if (!file_inode) {
        fprintf(stderr, "File not found in heartyfs\n");
        goto cleanup;
    }

    if (file_inode->type != FILE_TYPE) {
        fprintf(stderr, "Not a regular file\n");
        goto cleanup;
    }

    // Clear existing data blocks and write new content
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    free_existing_blocks(disk, file_inode, bitmap);
    
    if (write_file_contents(disk, file_inode, ext_file, file_size, bitmap) != 
        WRITE_SUCCESS) {
        goto cleanup;
    }

    printf("File '%s' written successfully to heartyfs\n", heartyfs_path);
    munmap(disk, DISK_SIZE);
    close(fd);
    fclose(ext_file);
    return 0;

cleanup:
    munmap(disk, DISK_SIZE);
    close(fd);
    fclose(ext_file);
    return 1;
}