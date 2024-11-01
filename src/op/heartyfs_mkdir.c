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
    char *path_copy = strdup(path);
    char *parent_path = dirname(path_copy);
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
            free(path_copy);
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    free(path_copy);
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

    char *dir_name = basename(path);
    for (int i = 0; i < parent_dir->size; i++) {
        if (strcmp(parent_dir->entries[i].file_name, dir_name) == 0) {
            fprintf(stderr, "Directory '%s' already exists\n", dir_name);
            munmap(disk, DISK_SIZE);
            close(fd);
            return 1;
        }
    }

    char *bitmap = (char *)(disk + BLOCK_SIZE);
    int new_block = find_free_block(bitmap);
    if (new_block == -1) {
        fprintf(stderr, "No free blocks available\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        return 1;
    }

    mark_block_used(bitmap, new_block);

    struct heartyfs_directory *new_dir = (struct heartyfs_directory *)(disk + new_block * BLOCK_SIZE);
    memset(new_dir, 0, sizeof(struct heartyfs_directory));
    new_dir->type = 1;
    strncpy(new_dir->name, dir_name, 27);
    new_dir->name[27] = '\0';
    new_dir->size = 2;

    // Set up . entry
    new_dir->entries[0].block_id = new_block;
    strcpy(new_dir->entries[0].file_name, ".");

    // Set up .. entry
    new_dir->entries[1].block_id = ((void *)parent_dir - disk) / BLOCK_SIZE;
    strcpy(new_dir->entries[1].file_name, "..");

    // Add new entry to parent directory
    parent_dir->entries[parent_dir->size].block_id = new_block;
    strncpy(parent_dir->entries[parent_dir->size].file_name, dir_name, 27);
    parent_dir->entries[parent_dir->size].file_name[27] = '\0';
    parent_dir->size++;

    munmap(disk, DISK_SIZE);
    close(fd);

    printf("Directory '%s' created successfully\n", dir_name);
    return 0;
}