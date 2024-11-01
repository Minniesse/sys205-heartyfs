#include "../heartyfs.h"
#include <string.h>
#include <libgen.h>

#define MAX_PATH 256

void mark_block_free(char *bitmap, int block) {
    bitmap[block / 8] |= (1 << (block % 8));
}

struct heartyfs_directory *find_parent_dir(void *disk, char *path, char **dir_name) {
    struct heartyfs_directory *current_dir = (struct heartyfs_directory *)disk;
    char *dir_path = strdup(path);
    char *parent_path = dirname(dir_path);
    *dir_name = basename(path);
    
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
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
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

    char *dir_name;
    struct heartyfs_directory *parent_dir = find_parent_dir(disk, path, &dir_name);
    if (parent_dir == NULL) {
        fprintf(stderr, "Parent directory not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    int dir_index = -1;
    int dir_block = -1;
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, dir_name) == 0) {
            dir_index = i;
            dir_block = parent_dir->entries[i].block_id;
            break;
        }
    }

    if (dir_index == -1) {
        fprintf(stderr, "Directory not found\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    struct heartyfs_directory *dir_to_remove = (struct heartyfs_directory *)(disk + dir_block * BLOCK_SIZE);
    if (dir_to_remove->type != 1) {
        fprintf(stderr, "Not a directory\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    if (dir_to_remove->size > 2) {
        fprintf(stderr, "Directory is not empty\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    // Free directory block
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    mark_block_free(bitmap, dir_block);

    // Remove directory entry from parent directory
    parent_dir->entries[dir_index] = parent_dir->entries[parent_dir->size - 1];
    parent_dir->size--;

    // Clear the directory block
    memset(dir_to_remove, 0, BLOCK_SIZE);

    munmap(disk, DISK_SIZE);
    close(fd);

    printf("Directory '%s' removed successfully\n", dir_name);
    return 0;
}