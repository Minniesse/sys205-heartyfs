#include "../heartyfs.h"
#include <string.h>
#include <sys/stat.h>

#define MAX_PATH 256
#define MAX_FILE_SIZE (119 * (BLOCK_SIZE - 4))  // Maximum file size based on inode structure

int find_free_block(char *bitmap) {
    for (int i = 2; i < NUM_BLOCK; i++) {
        if (bitmap[i / 8] & (1 << (i % 8))) {
            // printf("print i: %d\n", i);
            return i;
        }
    }
    return -1;
}

void mark_block_used(char *bitmap, int block) {
    bitmap[block / 8] &= ~(1 << (block % 8));
}

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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <heartyfs_file_path> <external_file_path>\n", argv[0]);
        return 1;
    }

    char heartyfs_path[MAX_PATH];
    strncpy(heartyfs_path, argv[1], MAX_PATH - 1);
    heartyfs_path[MAX_PATH - 1] = '\0';

    char *external_path = argv[2];

    // Open the external file
    FILE *ext_file = fopen(external_path, "rb");
    if (ext_file == NULL) {
        perror("Cannot open external file");
        return 1;
    }

    // Get the size of the external file
    struct stat st;
    if (stat(external_path, &st) != 0) {
        perror("Cannot get external file size");
        fclose(ext_file);
        return 1;
    }
    off_t file_size = st.st_size;

    if (file_size > MAX_FILE_SIZE) {
        fprintf(stderr, "External file is too large for heartyfs\n");
        fclose(ext_file);
        return 1;
    }

    int fd = open(DISK_FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open the disk file");
        fclose(ext_file);
        return 1;
    }

    void *disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Cannot map the disk file onto memory");
        close(fd);
        fclose(ext_file);
        return 1;
    }

    struct heartyfs_inode *file_inode = find_file(disk, heartyfs_path);
    if (file_inode == NULL) {
        fprintf(stderr, "File not found in heartyfs\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        fclose(ext_file);
        return 1;
    }

    if (file_inode->type != 0) {
        fprintf(stderr, "Not a regular file\n");
        munmap(disk, DISK_SIZE);
        close(fd);
        fclose(ext_file);
        return 1;
    }

    // Clear existing data blocks
    char *bitmap = (char *)(disk + BLOCK_SIZE);
    for (int i = 0; i < file_inode->size; i++) {
        mark_block_used(bitmap, file_inode->data_blocks[i]);
        memset(disk + file_inode->data_blocks[i] * BLOCK_SIZE, 0, BLOCK_SIZE);
    }

    // Write new data
    size_t bytes_written = 0;
    int block_index = 0;
    while (bytes_written < file_size) {
        int new_block = find_free_block(bitmap);
        if (new_block == -1) {
            fprintf(stderr, "No free blocks available\n");
            munmap(disk, DISK_SIZE);
            close(fd);
            fclose(ext_file);
            return 1;
        }
        mark_block_used(bitmap, new_block);
        file_inode->data_blocks[block_index] = new_block;

        struct heartyfs_data_block *data_block = (struct heartyfs_data_block *)(disk + new_block * BLOCK_SIZE);
        size_t bytes_to_write = (file_size - bytes_written < BLOCK_SIZE - 4) ? (file_size - bytes_written) : (BLOCK_SIZE - 4);
        // printf("type %d\n", file_inode->type);
        data_block->size = bytes_to_write;
        // printf("type %d\n", file_inode->type);
        fread(data_block->data, 1, bytes_to_write, ext_file);
        
        bytes_written += bytes_to_write;
        block_index++;
        // printf("Debug: Writing block %d, size %zu\n", new_block, bytes_to_write);
    }

    file_inode->size = block_index;
    printf("\n");
    // printf("Debug: Writing file %s\n", heartyfs_path);
    // printf("Debug: File size: %ld bytes\n", file_size);

    munmap(disk, DISK_SIZE);
    close(fd);
    fclose(ext_file);

    printf("File '%s' written successfully to heartyfs\n", heartyfs_path);
    return 0;
}