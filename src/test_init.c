#include "heartyfs.h"
#include <assert.h>

void test_superblock(struct heartyfs_directory *superblock) {
    assert(superblock->type == 1);
    assert(strcmp(superblock->name, "/") == 0);
    assert(superblock->size == 2);
    assert(superblock->entries[0].block_id == 0);
    assert(strcmp(superblock->entries[0].file_name, ".") == 0);
    assert(superblock->entries[1].block_id == 0);
    assert(strcmp(superblock->entries[1].file_name, "..") == 0);
    printf("Superblock initialization: PASSED\n");
}

void test_bitmap(char *bitmap) {
    assert((unsigned char)bitmap[0] == 0xFC);
    for (int i = 1; i < BLOCK_SIZE; i++) {
        assert((unsigned char)bitmap[i] == 0xFF);
    }
    printf("Bitmap initialization: PASSED\n");
}

int main() {
    int fd = open(DISK_FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Cannot open the disk file");
        exit(1);
    }

    void *buffer = mmap(NULL, DISK_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        exit(1);
    }

    test_superblock((struct heartyfs_directory *)buffer);
    test_bitmap((char *)(buffer + BLOCK_SIZE));

    if (munmap(buffer, DISK_SIZE) == -1) {
        perror("Error un-mmapping the file");
    }
    close(fd);

    printf("All tests passed. heartyfs initialization is correct.\n");
    return 0;
}