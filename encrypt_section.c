/*
 * encrypt_section.c
 *
 *  Created on: 2014-8-29
 *      Author: lyz
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "elf.h"
 
int main(int argc, char** argv){
 
    char section_name[] = ".log";
    char *shstr = NULL;
    char *content = NULL;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shdr;
    int i;
    unsigned int base, length;
    unsigned short nblock;
    unsigned short nsize;
    unsigned char block_size = 16;
 
    int fd;
 
    if(argc < 2){
        puts("PLease input so file name!");
        return -1;
    }
 
    fd = open(argv[1], O_RDWR);
 
    if(fd < 0){
        printf("open %s failed!\n", argv[1]);
        goto _error;
    }
    printf("open %s success!\n", argv[1]);
 
    if(read(fd, &ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)){
        printf("Read Elf header failed!");
        goto _error;
    }
    printf("Read Elf header success!\n");
 
    lseek(fd, ehdr.e_shoff + sizeof(Elf32_Shdr) * ehdr.e_shstrndx, SEEK_SET);  //定位section header
 
    if(read(fd, &shdr, sizeof(Elf32_Shdr)) != sizeof(Elf32_Shdr)){
        printf("Read Section string table failed!");
        goto _error;
    }
    printf("Read Section string table success!\n");
 
    if((shstr = (char *)malloc(shdr.sh_size)) == NULL){
        printf("malloc space for section string table failed!");
        goto _error;
    }
 
    lseek(fd, shdr.sh_offset, SEEK_SET);  //定位string table的位置
    if(read(fd, shstr, shdr.sh_size) != shdr.sh_size){
        printf("Read string table failed!");
        goto _error;
    }
    printf("Read string table success!\n");
 
    lseek(fd, ehdr.e_shoff, SEEK_SET);
    for(i = 0; i < ehdr.e_shnum; i++){
        if(read(fd, &shdr, sizeof(Elf32_Shdr)) != sizeof(Elf32_Shdr)){
            printf("Find section .log procedure failed!");
            goto _error;
        }
        if(strcmp(shstr + shdr.sh_name, section_name) == 0) {
            base = shdr.sh_offset;
            length = shdr.sh_size;
            printf("Find section %s \n", section_name);
            break;
        }
    }
 
    lseek(fd, base, SEEK_SET);
    content = (char*)malloc(length);
    if(content == NULL){
        printf("malloc space for content failed!");
        goto _error;
    }
 
    if(read(fd, content, length) != length){
        printf("Read section .log dailed!");
        goto _error;
    }
    printf("Read section .log success!\n");
 
    nblock = length / block_size;
    nsize = base / 4096 + (base % 4096 == 0 ? 0 : 1);
 
    printf("base = %d, length = %d\n", base, length);
    printf("nblock = %d, nsize = %d\n", nblock, nsize);

    ehdr.e_entry = (length << 16) + nsize;
    ehdr.e_shoff = base;
 
    for(i = 0; i < length; i++){
        content[i] = content[i] ^ 0x11;
    }

    lseek(fd, 0, SEEK_SET);
    if(write(fd, &ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)){
        printf("Write Elf header to so failed!\n");
        goto _error;
    }
 
    lseek(fd, base, SEEK_SET);
    if(write(fd, content, length) != length){
        printf("Write modified content to so failed!");
        goto _error;
    }
    printf("Write modified content to so success!\n");
 
    printf("Encrypt Completed!\n");
 
_error:
    close(fd);
    free(content);
    free(shstr);
    return 0;
 
}