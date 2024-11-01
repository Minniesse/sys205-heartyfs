#include "heartyfs.h"
#include <string.h>

void init_superblock(struct heartyfs_directory *superblock) {
    memset(superblock, 0, sizeof(struct heartyfs_directory));
    superblock->type = 1;  // Directory type
    strcpy(superblock->name, "/");
    superblock->size = 2;  // . and ..

    // Set up . entry
    superblock->entries[0].block_id = 0;  // Superblock is at block 0
    strcpy(superblock->entries[0].file_name, ".");

    // Set up .. entry (root is its own parent)
    superblock->entries[1].block_id = 0;
    strcpy(superblock->entries[1].file_name, "..");
}

void init_bitmap(char *bitmap) {
    memset(bitmap, 0xFF, BLOCK_SIZE);  // Set all bits to 1 (free)
    bitmap[0] = 0xFC;  // Mark first two blocks as used (11111100)
}

int main() {
    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        exit(1);
    }

    void *buffer = mmap(NULL, DISK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        exit(1);
    }

    // Initialize superblock (block 0)
    init_superblock((struct heartyfs_directory *)buffer);

    // Initialize bitmap (block 1)
    init_bitmap((char *)(buffer + BLOCK_SIZE));

    if (munmap(buffer, DISK_SIZE) == -1) {
        perror("Error un-mmapping the file");
    }
    close(fd);

    printf("heartyfs initialized successfully.\n");
    return 0;
}