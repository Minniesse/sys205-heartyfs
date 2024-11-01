#include "../heartyfs.h"
#include <string.h>

#define MAX_PATH 256

struct heartyfs_inode *find_file(void *disk, const char *path) {
    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';

    char *token = strtok(path_copy, "/");
    char *next_token;

    while (token != NULL) {
        next_token = strtok(NULL, "/");

        int found = 0;
        for (int i = 0; i < current_dir->size; i++) {
            if (strcmp(current_dir->entries[i].file_name, token) == 0) {
                if (next_token == NULL) {
                    // This is the file we're looking for
                    return (struct heartyfs_inode *)(disk + current_dir->entries[i].block_id * BLOCK_SIZE);
                }
                current_dir = (struct heartyfs_directory *)(disk + current_dir->entries[i].block_id * BLOCK_SIZE);
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    char path[MAX_PATH];
    strncpy(path, argv[1], MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    int fd = open(DISK_FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Cannot open the disk file");
        return 1;
    }

    void *disk = mmap(NULL, DISK_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        return 1;
    }

    struct heartyfs_inode *file_inode = find_file(disk, path);
    if (file_inode == NULL) {
        fprintf(stderr, "File not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    if (file_inode->type != 0) {
        fprintf(stderr, "Not a regular file\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    for (int i = 0; i < file_inode->size; i++) {
        struct heartyfs_data_block *data_block = (struct heartyfs_data_block *)(disk + file_inode->data_blocks[i] * BLOCK_SIZE);
        fwrite(data_block->data, 1, data_block->size, stdout);
    }

    munmap(disk, DISK_SIZE);
    close(fd);

    return 0;
}