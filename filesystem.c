#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define BLOCK (4 * 1024)

#define INODE 1
#define DATA 2

#define FREE 3
#define ALLOC 4

#define NAMELEN 60

#define STD 5
#define ALL 6

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

struct Superblock{
    int size;
    int fcount;
    int first;
    int free;
};

struct Inode{
    int size;
    int address_block;
    char name[NAMELEN];
};

struct Number{
    int num;
};

struct Character{
    char a;
};

int inode_bitmap_addr(int index) {
    return BLOCK + index * sizeof(struct Number);
}

int data_bitmap_addr(int index) {
    return 2*BLOCK + index * sizeof(struct Number);
}

int inode_addr(int index) {
    return 3*BLOCK + index * sizeof(struct Inode);
}

int data_block_addr(int index, int first) {
    return first + index * BLOCK;
}

int addr_index(int data_address, int first) {
    return (data_address - first) / BLOCK + 1;
}

int nr_inode_blocks(int blocks) {
    return (blocks - 3) / BLOCK * sizeof(struct Inode) + 1;
}

int block_allocated(int index, int type, FILE *vdisk) {
    int blocks, inblocks;
    struct Superblock superblock;
    struct Number n;

    fseek(vdisk, 0, SEEK_SET);
    fread(&superblock, sizeof(struct Superblock), 1, vdisk);
    blocks = superblock.size / BLOCK;
    inblocks = nr_inode_blocks(blocks);
    if (index < 0 || (type == INODE && index >= inblocks * BLOCK / sizeof(struct Inode)) || (type == DATA && index >= blocks - inblocks)) {
        printf ("Invalid  block index: %d", index);
        return -1;
    }
    if (type == INODE) {
        fseek(vdisk, inode_bitmap_addr(index), SEEK_SET);
    } else {
        fseek(vdisk, data_bitmap_addr(index), SEEK_SET);
    }
    fread(&n, sizeof(n), 1, vdisk);
    return n.num;
}

int create_file_system(int size) {
    FILE *vdisk;
    int blocks = size / BLOCK;
    int inblocks = nr_inode_blocks(blocks);
    int i;
    char buffer[BLOCK];
    struct Superblock superblock;
    struct Number n;

    printf("blocks: %d\ninode blocks: %d\n", blocks, inblocks);

    vdisk = fopen("virtual_disk", "wb+");
    if (vdisk == NULL) {
        puts("Error: the disk cannot be created");
        return 1;
    }
    fseek(vdisk, size-1, SEEK_SET);
    buffer[0] = 'a';
    fwrite(buffer, sizeof(char), 1, vdisk);

    fseek(vdisk, 0, SEEK_SET);
    superblock.size = size;
    superblock.fcount = 0;
    superblock.free = size;
    superblock.first = (inblocks + 3) * BLOCK;
    printf("First block at (hex): %x\n", superblock.first);
    fwrite(&superblock, sizeof(superblock), 1, vdisk);

    fseek(vdisk, inode_bitmap_addr(0), SEEK_SET);
    n.num = FREE;
    printf("max num of inodes: %lu\n", inblocks * BLOCK / sizeof(struct Inode));
    for (i = 0; i < inblocks * BLOCK / sizeof(struct Inode); ++i) {
        fwrite(&n, sizeof(n), 1, vdisk);
    }
    fseek(vdisk, data_bitmap_addr(0), SEEK_SET);
    for (i = 0; i < blocks - inblocks - 3; ++i) {
        fwrite(&n, sizeof(n), 1, vdisk);
    }
    fclose(vdisk);
    return 0;
}

int save(char *name) {
    FILE *vdisk, *fp;
    char buffer[BLOCK];
    char a;
    struct Superblock superblock;
    struct Inode inode;
    struct Number n;

    int size, fsize, nblocks;
    int bitmap_iterator, index, cur;
    int i;

    printf("Saving %s to virtual disk\n", name);

/*     //open both files*/    
    vdisk = fopen("virtual_disk", "r+b");
    if (vdisk == NULL) {
        puts("Failed to open virutal disk file");
        return -1;
    }
    fp = fopen(name, "r");
    if (fp == NULL) {
        puts("Failed to open file");
        fclose(vdisk);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    size = fsize = ftell(fp);
    printf("File size: %d,\n fsize: %d\n", size, fsize);
    rewind(fp);
    rewind(vdisk);

/*     //read data from the superblock
 */    fread(&superblock, sizeof(struct Superblock), 1, vdisk);
    if (fsize > superblock.free) {
        printf("Not enough space");
        return -1;
    }
    inode.size = size;
    strcpy(inode.name, name);
    nblocks = size / BLOCK;
    index = 0;
    cur = superblock.first;
    printf("First allocable block: %x\n", cur);

/*     //find an empty block for addresses
 */    while (block_allocated(index, DATA, vdisk) == ALLOC) {
         index++;
    }
    printf("Free block index: %d\n", index);
    inode.address_block = data_block_addr(index, superblock.first);
    printf("Address block (hex): %x\n", inode.address_block);
    i = 0;

/*     //mark address block as allocated
 */    fseek(vdisk, data_bitmap_addr(index), SEEK_SET);
    printf("Block allocation checked at: %x\n", data_bitmap_addr(index));
    n.num = ALLOC;
    fwrite(&n, sizeof(n), 1, vdisk);

    for (; fsize > 0; fsize-=BLOCK) {
        printf("Current fsize: %d\n", fsize);
/*         // find an empty block for file data
 */        while (block_allocated(index, DATA, vdisk) == ALLOC) {
            index++;
        }
        printf("Empty block index: %d\n", index);

/*         // copy data*/        
        fread(buffer, sizeof(char), BLOCK, fp);
        cur = data_block_addr(index, superblock.first);
        printf("Position for insertion (hex): %x\n", cur);
        fseek(vdisk, cur, SEEK_SET);
        fwrite(buffer, sizeof(char), MIN(fsize, BLOCK), vdisk);

/*         // save the block address */        
        fseek(vdisk, inode.address_block + i*sizeof(n), SEEK_SET);
        fwrite(&cur, sizeof(n), 1, vdisk);
        i++;

/*         // mark the data block as allocated */     
        printf("Bitmap position for allocation (hex): %x\n", data_bitmap_addr(index));
        fseek(vdisk, data_bitmap_addr(index), SEEK_SET);
        n.num = ALLOC;
        fwrite(&n, sizeof(n), 1, vdisk);
    }
/*     //save the inode*/    
    index = 0;
    while (block_allocated(index, INODE, vdisk) == ALLOC) {
         index++;
    }
    fseek(vdisk, inode_bitmap_addr(index), SEEK_SET);
    n.num = ALLOC;
    fwrite(&n, sizeof(n), 1, vdisk);

    fseek(vdisk, inode_addr(index), SEEK_SET);
    fwrite(&inode, sizeof(inode), 1, vdisk);
    superblock.free -= size;
    superblock.fcount++;

    fseek(vdisk, 0, SEEK_SET);
    fwrite(&superblock, sizeof(superblock), 1, vdisk);

    fclose(vdisk);
    fclose(fp);

    return 0;
}
int read_file(char name[]) {
    FILE *vdisk, *fp;
    char buffer[BLOCK];
    struct Superblock superblock;
    struct Inode inode;
    struct Number n;

    int index, size, found = 0;
    char read_name[NAMELEN];

/*     //open both files*/    
    vdisk = fopen("virtual_disk", "rb");
    if (vdisk == NULL) {
        puts("Failed to open virtual disk file");
        fclose(vdisk);
        return -1;
    }
    fp = fopen(name, "w");
    if (fp == NULL) {
        puts("Failed to open file");
        fclose(vdisk);
        return -1;
    }
    rewind(vdisk);
    fread(&superblock, sizeof(struct Superblock), 1, vdisk);
    for (index = 0; index < nr_inode_blocks(superblock.size/BLOCK) *BLOCK / sizeof(inode); index++) {
        if (block_allocated(index, INODE, vdisk) == ALLOC) {
            printf("inode at (hex): %x, ", inode_addr(index));
            fseek(vdisk, inode_addr(index) + 8, SEEK_SET);
            fread(read_name, sizeof(char), NAMELEN, vdisk);
            printf("name: %s\n", read_name);
            if (strncmp(read_name, name, strlen(name)) == 0) {
                printf("Found %s\n", name);
                found = 1;
                break;
            }
        }
    }
    if (found == 1) {
        fseek(vdisk, inode_addr(index), SEEK_SET);
        fread(&inode, sizeof(inode), 1, vdisk);
        size = inode.size;
        printf("File size: %d\n", inode.size);
        for (index = 0; index < size / BLOCK + 1; index++) {
            printf("Looking at address of index %d\nAddress (hex): %lx\n", index, inode.address_block + index * sizeof(n));
            fseek(vdisk, inode.address_block + index * sizeof(n), SEEK_SET);
            fread(&n, sizeof(n), 1, vdisk);
            printf("Address it points to (hex): %x\n", n.num);
            fseek(vdisk, n.num, SEEK_SET);
            fread(buffer, sizeof(char), MIN(size, BLOCK), vdisk);
            fwrite(buffer, sizeof(char), MIN(size, BLOCK), fp);
            size -= BLOCK;
        }
    } else printf("No such file\n");
    fclose(vdisk);
    fclose(fp);
    return 0;
}

void show_files(int mode) {
    FILE *vdisk;
    struct Superblock superblock;
    struct Inode inode;
    struct Number n;
    int i;
    vdisk = fopen("virtual_disk", "rb");
    fread(&superblock, sizeof(superblock), 1, vdisk);
    printf("All files:\n");
    for (i = 0; i < nr_inode_blocks(superblock.size/BLOCK) * BLOCK / sizeof(inode); i++) {
        if (block_allocated(i, INODE, vdisk) == ALLOC) {
            fseek(vdisk, 3*BLOCK + i * sizeof(inode), SEEK_SET);
            fread(&inode, sizeof(inode), 1, vdisk);
            if(*(inode.name) != '.' || mode == ALL) {
                printf("%s %10d\n", inode.name, inode.size);
            }
        }
    }
    fclose(vdisk);
}

void show_disk_usage() {
    FILE *vdisk;
    struct Superblock superblock;
    struct Number n;
    int index;
    int nr_inodes, nr_data_blocks;
    
    vdisk = fopen("virtual_disk", "rb");
    fseek(vdisk, 0, SEEK_SET);
    fread(&superblock, sizeof(superblock), 1, vdisk);
    printf("Total disk size: %d bytes\nFree space: %d\nTotal number of files: %d\n", 
    superblock.size, superblock.free, superblock.fcount);

/*  show inode space usage */
    printf("Inodes:\n");
    nr_inodes = nr_inode_blocks(superblock.size / BLOCK) * BLOCK / sizeof(struct Inode);
    for (index = 0; index < nr_inode_blocks(superblock.size / BLOCK) * BLOCK / sizeof(struct Inode); index++) {
        fseek(vdisk, inode_bitmap_addr(index), SEEK_SET);
        fread(&n, sizeof(n), 1, vdisk);
        if (n.num == ALLOC) printf("#");
        else if (n.num == FREE) printf("_");
        else printf("?");
    }
    printf("\nData:\n");
    nr_data_blocks = superblock.size / BLOCK - nr_inode_blocks(superblock.size / BLOCK) - 3;
    for (index = 0; index < nr_data_blocks; index++) {
        fseek(vdisk, data_bitmap_addr(index), SEEK_SET);
        fread(&n, sizeof(n), 1, vdisk);
        if (n.num == ALLOC) printf("#");
        else if (n.num == FREE) printf("_");
        else printf("?");
    }
    printf("\n");
    fclose(vdisk);
}
void remove_file(char name[]) {
    FILE *vdisk;
    struct Superblock superblock;
    struct Inode inode;
    struct Number n;
    int index, i, nr_inodes, size, found = 0;
    char read_name[NAMELEN];

    vdisk = fopen("virtual_disk", "r+b");

    fseek(vdisk, 0, SEEK_SET);
    fread(&superblock, sizeof(superblock), 1, vdisk);

    nr_inodes = nr_inode_blocks(superblock.size / BLOCK) * BLOCK / sizeof(inode);
    for (index = 0; index < nr_inodes; index++) {
        if (block_allocated(index, INODE, vdisk) == ALLOC) {
            printf("inode at (hex): %x, ", inode_addr(index));
            fseek(vdisk, inode_addr(index) + 8, SEEK_SET);
            fread(read_name, sizeof(char), NAMELEN, vdisk);
            printf("name: %s\n", read_name);
            if (strncmp(name, read_name, NAMELEN) == 0) {
                printf("Found %s\n", name);
                found = 1;
                break;
            }
        }
    }
    if (found == 1) {
        fseek(vdisk, inode_addr(index), SEEK_SET);
        fread(&inode, sizeof(inode), 1, vdisk);
        for (i = 0; i < inode.size / BLOCK + 1; i++) {
            fseek(vdisk, inode.address_block + i*sizeof(n), SEEK_SET);
            fread(&n, sizeof(n), 1, vdisk);
            fseek(vdisk, data_bitmap_addr(addr_index(n.num, superblock.first)), SEEK_SET);
            n.num = FREE;
            fwrite(&n, sizeof(n), 1, vdisk);
        }
        fseek(vdisk, data_bitmap_addr(addr_index(inode.address_block, superblock.first)), SEEK_SET);
        fwrite(&n, sizeof(n), 1, vdisk);

        fseek(vdisk, inode_bitmap_addr(index), SEEK_SET);
        fwrite(&n, sizeof(n), 1, vdisk);

        superblock.fcount--;
        superblock.free += inode.size;

        fseek(vdisk, 0, SEEK_SET);
        fwrite(&superblock, sizeof(superblock), 1, vdisk);
    } else printf("No such file\n");

    fclose(vdisk);
}
void delete_file_system() {
    if (remove("virtual_disk") == 0) printf("File system deleted successfully\n");
    else printf("Failed to remove file\n");
}

int main(int argv, char *argc[]) {
    int size;
    char name[NAMELEN];
    char opt;
    if (argv > 3) {
        puts("invalid number of arguments");
        return 1;
    } else if (argv == 3) {
        size = atoi(argc[2]) * 1024;
    }

    switch(*argc[1]) {
        case 'c':
            printf("creating new file system of size %d\n", size);
            create_file_system(size);
        break;
        case 's':
            printf("saving file to file system\n");
            printf("Filename: ");
            fgets(name, NAMELEN, stdin);
            name[strlen(name) - 1] = 0;
            save(name);
        break;
        case 'r': 
            printf("reading file from file system\n");
            printf("Filename: ");
            fgets(name, NAMELEN, stdin);
            name[strlen(name) - 1] = 0;
            read_file(name);
        break;
        case 'l':
            printf("listing all files in file system\n");
            printf("Show hidden files? (y/n) ");
            scanf("%c", &opt);
            switch (opt){
                case 'y':
                    show_files(ALL);
                break;
                case 'n':
                    show_files(STD);
                break;
                default:
                    printf("instruction unknown\n");
            }
        break;
        case 'u':
            printf("showing memory usage\n");
            show_disk_usage();
        break;
        case 'd':
            printf("deleting file from file system\n");
            printf("Filename: ");
            fgets(name, NAMELEN, stdin);
            name[strlen(name) - 1] = 0;
            remove_file(name);
        break;
        case 'k':
            printf("deleting the file system\n");
            delete_file_system();
        break;
        default:
            printf("instruction unknown\n");
    }

    /* printf("size: %d bytes\n", size);
    create_file_system(size);
    save(".Ala miała kota");
    save("Kot");
    read_file(".Ala");
    show_files(ALL);
    show_disk_usage();
    remove_file("Kot");
    show_disk_usage();
    delete_file_system(); */
    return 0;
}