#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

#define MAX_PATH 256

void mark_block_free(char *bitmap, int block) {
    bitmap[block / 8] |= (1 << (block % 8));
}

struct heartyfs_directory *find_parent_dir(void *disk, char *path, char **file_name) {
    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *dir_path = strdup(path);
    char *parent_path = dirname(dir_path);
    *file_name = basename(path);
    
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

    char *file_name;
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, path, &file_name);
    if (parent_dir == NULL) {
        fprintf(stderr, "Parent directory not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    int file_index = -1;
    int file_block = -1;
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, file_name) == 0) {
            file_index = i;
            file_block = parent_dir->entries[i].block_id;
            break;
        }
    }

    if (file_index == -1) {
        fprintf(stderr, "File not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    struct heartyfs_inode *file_inode = (struct heartyfs_inode *)(disk + file_block * BLOCK_SIZE);
    if (file_inode->type != 0) {
        fprintf(stderr, "Not a regular file\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    // Free data blocks
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    for (int i = 0; i < file_inode->size; i++) {
        mark_block_free(bitmap, file_inode->data_blocks[i]);
    }

    // Free inode block
    mark_block_free(bitmap, file_block);

    // Remove file entry from parent directory
    parent_dir->entries[file_index] = parent_dir->entries[parent_dir->size - 1];
    parent_dir->size--;

    // Clear the inode
    memset(file_inode, 0, BLOCK_SIZE);

    munmap(disk, DISK_SIZE);
    close(fd);

    printf("File '%s' removed successfully\n", file_name);
    return 0;
}
