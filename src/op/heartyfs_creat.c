#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

#define MAX_PATH 256

int find_free_block(char *bitmap) {
    for (int i = 2; i < NUM_BLOCK; i++) {
        if (bitmap[i / 8] & (1 << (i % 8))) {
            return i;
        }
    }
    return -1;
}

void mark_block_used(char *bitmap, int block) {
    bitmap[block / 8] &= ~(1 << (block % 8));
}

struct heartyfs_directory *find_parent_dir(void *disk, char *path) {
    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *dir_path = strdup(path);
    char *parent_path = dirname(dir_path);
    char *token = strtok(parent_path, "/");

    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                current_dir = (struct heartyfs_directory *)(disk + current_dir->entries[i].block_id * BLOCK_SIZE);
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    char path[MAX_PATH];
    strncpy(path, argv[1], MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        return 1;
    }

    void *disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        return 1;
    }

    struct heartyfs_directory *parent_dir = find_parent_dir(disk, path);
    if (parent_dir == NULL) {
        fprintf(stderr, "Parent directory not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    if (parent_dir->size >= 14) {
        fprintf(stderr, "Parent directory is full\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    char *file_name = basename(path);
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, file_name) == 0) {
            fprintf(stderr, "File already exists\n");
            munmap(disk, DISK_SIZE);
            close(fd);
            return 1;
        }
    }

    char *bitmap = (char *)(disk + BLOCK_SIZE);
    int inode_block = find_free_block(bitmap);
    if (inode_block == -1) {
        fprintf(stderr, "No free blocks available\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    mark_block_used(bitmap, inode_block);

    struct heartyfs_inode *new_inode = (struct heartyfs_inode *)(disk + inode_block * BLOCK_SIZE);
    memset(new_inode, 0, sizeof(struct heartyfs_inode));
    new_inode->type = 0; // Regular file
    strncpy(new_inode->name, file_name, 27);
    new_inode->name[27] = '\0';
    new_inode->size = 0;

    // Add new entry to parent directory
    parent_dir->entries[parent_dir->size].block_id = inode_block;
    strncpy(parent_dir->entries[parent_dir->size].file_name, file_name, 27);
    parent_dir->entries[parent_dir->size].file_name[27] = '\0';
    parent_dir->size++;

    munmap(disk, DISK_SIZE);
    close(fd);

    printf("File '%s' created successfully\n", file_name);
    return 0;
}
