#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>

typedef enum {dm, fa} cache_map_t; // Direct mapped or fully associative
typedef enum {uc, sc} cache_org_t; // unified/split
typedef enum {instruction, data} access_t;

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;

typedef struct {
    int valid; 
    unsigned long tag;
    int index; 
} cache_entry; // Data is stored in blocks of 64 bytes 


int ADDRESS_SIZE = 32;
int BLOCK_SIZE = 64; 


// DECLARE CACHES AND COUNTERS FOR THE STATS HERE

char binary_to_hex[16][5] = {"0000", "0001", "0010", "0011",
                             "0100", "0101", "0110", "0111",
                             "1000", "1001", "1010", "1011",
                             "1100", "1101", "1110", "1111"};

// Function for converting hex-characters to binary characters
int hex_binary(char* hex, char* binary)   {
    for (int i = 0; i < strlen(hex); i++) {
        char temp[2] = {hex[i], '\0'};
        unsigned long index = strtol(temp, NULL, 16); 
        strcat(binary, binary_to_hex[index]);
    }
    return 0; 
}

int main(int argc, char *argv[]) {

    // Getting the command-line arguments:

    size_t size = strtol(argv[1], NULL, 10); 

    cache_map_t map;
    if (strcmp(argv[2], "dm") == 0) {
        map = 0;
    }
    else map = 1; 

    cache_org_t org; 
    if (strcmp(argv[3], "uc") == 0) {
        org = 0;
    }
    else org = 1; 


    // Creating the cache
    int number_of_blocks = size / BLOCK_SIZE;  // 1024 / 64 = 16
    cache_entry* cache = malloc(sizeof(cache_entry) * number_of_blocks);
    cache_stat_t counter = {0, 0};

    // Getting the cache ready by setting inserting cache-entries and 
    // setting the valid-bits to zero
    for (int i = 0; i < number_of_blocks; i++) {
        cache_entry temp;
        temp.valid = 0; 
        temp.tag = 0;
        temp.index = i; 
        cache[i] = temp; 
    }

    // Memory addresses are 32 bits long 

    cache_entry first = cache[0];
    cache_entry second = cache[1];
    cache_entry third = cache[2];
    cache_entry twelve = cache[12];


    // Reading the memory trace from the file
    FILE *fp; 
    char line[13];
    fp = fopen("mem_trace.txt", "r");

    // Going through all the lines
    while (fgets(line, 13, fp) != NULL) {
        // printf("%s\n", line); 
    
        // Maybe we can put this outside the while loop somehow?

        if (map == dm && org == uc) { // Direct Mapping and unified cache

            // Let's turn the input into a bit-string
            char binary[33];
            // Clearing the binary string
            memset(binary, 0, strlen(binary)); 

            // Isolating the hex part of the line
            char hex[9];
            for (int i = 2, j = 0; i < 10; i++, j++) {
                hex[j] = line[i]; 
            }
            printf("Printing hex now: \n");
            for (int i = 0; i < strlen(hex); i++) {
                printf("%c", hex[i]); 
            }
            printf("\n"); 

            // This calculates a binary representation of the hex, so 
            // we're able to look at it bitwise
            hex_binary(hex, binary); 

            // First we have to calculate the tag, index and offset
            printf("Printing the binary...\n");
            for (int i = 0; i < 32; i++) {
                printf("%c", binary[i]);
            }
            printf("\n"); 


            // Index and tag bits needed:
            int offset_size = log2(BLOCK_SIZE);
            int index_bits = log2(number_of_blocks); // log2(16) = 4

            // 32 - 6 - 4 = 22
            int tag_bits = ADDRESS_SIZE - offset_size - index_bits;

            // Now we need to get the index, so that we know which entry 
            // to look at:
            char* address_index_bits = malloc(sizeof(char) * (index_bits + 1)); 
            for (int i = 0; i < index_bits; i++) {
                address_index_bits[i] = binary[tag_bits + i];
            }
            address_index_bits[index_bits] = '\0';

            unsigned long cache_index = strtol(address_index_bits, NULL, 2); 
            printf("Accessing index %lu...\n", cache_index); 

            // Getting the tag
            char* address_tag_bits = malloc(sizeof(char) * (tag_bits + 1)); 
            for (int i = 0; i < tag_bits; i++) {
                address_tag_bits[i] = binary[i];
            }
            address_tag_bits[tag_bits] = '\0'; 
            unsigned long cache_tag = strtol(address_tag_bits, NULL, 2); 
            printf("Looking at the tag: %lu...\n", cache_tag); 

            // Now we know which index to check in the cache!
            cache_entry cache_access = cache[cache_index];
            counter.accesses++; 

            if (!cache_access.valid) {
                // This means it's a miss and that we need to insert 
                // our tag into the entry
                cache_access.tag = cache_tag;
                cache_access.valid = 1; 
                cache[cache_index] = cache_access;
            }
            else if (cache_access.valid && cache_access.tag == cache_tag) {
                
                // This means that it's a hit!
                counter.hits++;
            }
            else {
                // If it's not a hit then we need to update the cache entry
                cache_access.tag = cache_tag; 
                cache[cache_index] = cache_access;
            }

            printf("\n\n"); 

        }
    }
    printf("\n");

    fclose(fp); 

    for (int i = 0; i < number_of_blocks; i++) {
        printf("The valid bit in cache entry %d is %d\n", i, cache[i].valid);
        printf("The tag in the entry is %lu \n", cache[i].tag); 
    }

    printf("The amount of accesses were %lu \n", counter.accesses); 
    printf("The amount of hits were %lu \n", counter.hits); 

    return 0;
}