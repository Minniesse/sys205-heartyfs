#include "heartyfs.h"
#include <string.h>

/* Constants for initialization */
#define ROOT_DIR_NAME "/"
#define CURRENT_DIR "."
#define PARENT_DIR ".."
#define SUPERBLOCK_ID 0
#define INITIAL_DIR_SIZE 2  // . and .. entries

/**
 * @brief Initialize the superblock (root directory) of the filesystem
 * @param[out] superblock Pointer to the superblock to initialize
 * 
 * Initializes the root directory with . and .. entries both pointing 
 * to itself since root is its own parent.
 */
void init_superblock(struct heartyfs_directory *superblock) {
    // Clear the entire superblock first
    memset(superblock, 0, sizeof(struct heartyfs_directory));
    
    // Initialize directory attributes
    superblock->type = 1;  // Directory type
    strncpy(superblock->name, ROOT_DIR_NAME, sizeof(superblock->name) - 1);
    superblock->size = INITIAL_DIR_SIZE;

    // Initialize current directory entry (.)
    superblock->entries[0].block_id = SUPERBLOCK_ID;
    strncpy(superblock->entries[0].file_name, CURRENT_DIR, 
            sizeof(superblock->entries[0].file_name) - 1);

    // Initialize parent directory entry (..)
    superblock->entries[1].block_id = SUPERBLOCK_ID;
    strncpy(superblock->entries[1].file_name, PARENT_DIR, 
            sizeof(superblock->entries[1].file_name) - 1);
}

/**
 * @brief Initialize the bitmap that tracks free blocks
 * @param[out] bitmap Pointer to the bitmap block to initialize
 * 
 * Sets all bits to 1 (free) initially, then marks the first two blocks 
 * (superblock and bitmap) as used.
 */
void init_bitmap(char *bitmap) {
    // Set all blocks as free initially
    memset(bitmap, 0xFF, BLOCK_SIZE);
    
    // Mark first two blocks (superblock and bitmap) as used
    // First byte becomes 11111100
    bitmap[0] = 0xFC;
}

/**
 * @brief Main function to initialize the heartyfs filesystem
 * @return 0 on success, 1 on failure
 */
int main() {
    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        return 1;
    }

    // Map the disk file into memory
    void *buffer = mmap(NULL, DISK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        return 1;
    }

    // Initialize filesystem structures
    init_superblock((struct heartyfs_directory *)buffer);
    init_bitmap((char *)(buffer + BLOCK_SIZE));

    // Cleanup
    if (munmap(buffer, DISK_SIZE) == -1) {
        perror("Error un-mmapping the file");
        close(fd);
        return 1;
    }
    
    close(fd);
    printf("heartyfs initialized successfully.\n");
    return 0;
}