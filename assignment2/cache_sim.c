#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>

typedef enum {dm, fa} cache_map_t;
typedef enum {uc, sc} cache_org_t;
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
    int tag;
} cache_entry_t; 


// DECLARE CACHES AND COUNTERS FOR THE STATS HERE


uint32_t cache_size; 
uint32_t block_size = 64;
uint32_t address_size = 32; 
uint32_t queue_counter = 0; 
cache_map_t cache_mapping;
cache_org_t cache_org;
cache_entry_t* direct_mapped_cache;
cache_entry_t* instruction_cache;
cache_entry_t* data_cache;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;


/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1000];
    char* token;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf,1000, ptr_file)!=NULL) {

        /* Get the access type */
        token = strsep(&string, " \n");        
        if (strcmp(token,"I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token,"D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(0);
        }
        
        /* Get the access type */        
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);

        return access;
    }

    /* If there are no more entries in the file,  
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

/* Map from hex to binary */
char binary_to_hex[16][5] = {"0000", "0001", "0010", "0011",
                             "0100", "0101", "0110", "0111",
                             "1000", "1001", "1010", "1011",
                             "1100", "1101", "1110", "1111"};

/* Function for generating binary string representation from decimal number */
void decimal_to_binary_string(uint32_t decimal, char* binary)   {
    int i = 0, j = 0, length = 0;
    // We find all the digits

    while (decimal > 0) {
        int remainder = decimal % 2;
        char binary_character = remainder + '0';
        binary[i++] = binary_character;
        decimal /= 2; 
    }
    while (i < 32) {
        binary[i++] = '0';
    }

    binary[i--] = '\0';
    
    // Now we swap the digits' position 
    while (j < i) {
        char temporary = binary[i];
        binary[i--] = binary[j];
        binary[j++] = temporary; 

    }
}


void main(int argc, char** argv)
{

    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));


    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */

    if ( argc != 4 ) { /* argc should be 2 for correct execution */
        printf("Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] [cache organization: uc|sc]\n");
        exit(0);
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0) {
            cache_mapping = dm;
        } else if (strcmp(argv[2], "fa") == 0) {
            cache_mapping = fa;
        } else {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0) {
            cache_org = uc;
        } else if (strcmp(argv[3], "sc") == 0) {
            cache_org = sc;
        } else {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    int number_of_sets = cache_size / block_size;
    if (cache_org == uc) {
        /* Creating a cache */ 
        direct_mapped_cache = malloc(sizeof(cache_entry_t) * number_of_sets); 
        
        /* Setting up the cache with valid bits set to 0 */ 
        for (int i = 0; i < number_of_sets; i++) {
            cache_entry_t entry = {0, 0}; 
            direct_mapped_cache[i] = entry; 
        }
    }
    else {
        /* Creating two caches */
        number_of_sets /= 2; 
        instruction_cache = malloc(sizeof(cache_entry_t) * number_of_sets); 
        data_cache = malloc(sizeof(cache_entry_t) * number_of_sets); 

        /* Setting up the caches with valid bits set to 0 */
        for (int i = 0; i < number_of_sets; i++) {
            cache_entry_t entry1 = {0, 0}; 
            cache_entry_t entry2 = {0, 0}; 
            instruction_cache[i] = entry1; 
            data_cache[i] = entry2; 
        }
    }
    int* queue = malloc(sizeof(int) * number_of_sets);
    memset(queue, 0, sizeof(int) * number_of_sets); 

    // Finding the number of bits for the cache
    int number_of_index_bits = 0;
    int number_of_offset_bits = log2(block_size);
    int number_of_tag_bits; 

    if (cache_mapping == dm) {
        number_of_index_bits = log2(number_of_sets); 
        number_of_tag_bits = address_size - number_of_index_bits - number_of_offset_bits;
    }
    else {
        number_of_tag_bits = address_size - number_of_offset_bits; 
    }

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen("mem_trace.txt","r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    
    /* Loop until whole trace file has been read */
    mem_access_t access;
    while(1) {
        access = read_transaction(ptr_file);
        //If no transactions left, break out of loop
        if (access.address == 0)
            break;

    /* First we find a string representation of our address in binary */
    char binary_string_representation[33];
    decimal_to_binary_string(access.address, binary_string_representation); 
    
    // printf("Binary representation: %s \n", binary_string_representation);


    /* Now we need to get the index bits */
    uint32_t cache_index;
    if (number_of_index_bits) {
    char* index_bits = malloc(sizeof(char) * (number_of_index_bits + 1));
    for (int i = 0; i < number_of_index_bits; i++) {
        index_bits[i] = binary_string_representation[number_of_tag_bits + i]; 
    }
    index_bits[number_of_index_bits] = '\0';
    // printf("Index bits: %s \n", index_bits);
    cache_index = strtol(index_bits, NULL, 2); 
    }

    /* Then we get the tag bits */
    char* tag_bits = malloc(sizeof(char) * (number_of_tag_bits + 1)); 
    for (int i = 0; i < number_of_tag_bits; i++) {
        tag_bits[i] = binary_string_representation[i]; 
    }
    tag_bits[number_of_tag_bits] = '\0';
    uint32_t cache_tag = strtol(tag_bits, NULL, 2); 

    cache_entry_t cache_entry; 
    cache_entry_t *chosen_cache = NULL; 

    /* Finding which cache to use */ 
    if (cache_org == uc) {
        chosen_cache = direct_mapped_cache;
    }
    else if (access.accesstype) {
        chosen_cache = data_cache;
    }
    else { 
        chosen_cache = instruction_cache; 
    }

    /* Direct Mapping */
    if (cache_mapping == dm) {
        // printf("the cache index for this access is: %u\n", cache_index); 
        cache_entry = chosen_cache[cache_index]; 
  
        cache_statistics.accesses++; 

        /* If the entry is not valid we have a miss and need 
            to replace the cache entry */
        if (!cache_entry.valid) {
            cache_entry.valid = 1;
            cache_entry.tag = cache_tag; // TODO:      
        }
        /* If we have a hit */
        else if (cache_entry.valid && cache_entry.tag == cache_tag) {
            cache_statistics.hits++; 
        }
        /* If it's valid, but we have a miss */
        else {
            cache_entry.tag = cache_tag; 
        }

        /* Updating the cache */
        chosen_cache[cache_index] = cache_entry; 
    }

   /* Fully Associative */
   else {
    
    /* We need to find the correct cache_entry.
       This can be done by looping through the cache 
       until you find an entry with the same tag. If 
       you don't find this then you have to look for 
       a valid bit that is zero. If you don't find this 
       then you have to replace using FIFO */

       int found_entry = 0; 
       cache_statistics.accesses++; 

       for (int i = 0; i < number_of_sets; i++) {
           cache_entry = chosen_cache[i]; 
           if (cache_entry.tag == cache_tag) { // HIT
                cache_statistics.hits++; 
                found_entry = 1; 
                break;
           }
       }

       if (!found_entry) { // MISS
            for (int i = 0; i < number_of_sets; i++) {
                cache_entry = chosen_cache[i]; 
                if (cache_entry.valid == 0) {
                    cache_entry.valid = 1; 
                    cache_entry.tag = cache_tag;
                    found_entry = 1;
                    chosen_cache[i] = cache_entry; 
                    queue[queue_counter++] = i;
                    break;
                }
            }
       }
       if (!found_entry) { // If we still haven't found an entry then FIFO
            cache_entry = chosen_cache[queue_counter];
            cache_entry.tag = cache_tag; 
            chosen_cache[queue_counter] = cache_entry;
            queue_counter = (queue_counter + 1) % number_of_sets;
       }
    }
    }

    /* Print the statistics */

    char* cache_map;
    char* cache_organization;
    if (cache_mapping == dm) {
        cache_map = "direct mapped"; 
    }
    else {
        cache_map  = "fully associative";
    }
    if (cache_org == uc) {
        cache_organization = "unified cache"; 
    }
    else {
        cache_organization = "split cache"; 
    }

    printf("\nCache Organization \n");
    printf("Size:                  %d bytes\n", cache_size);
    printf("Mapping:               %s \n", cache_map);
    printf("Organization:          %s \n", cache_organization);
    printf("Number of index bits:  %d bits\n", number_of_index_bits);
    printf("Number of tag bits:    %d bits\n", number_of_tag_bits);
    printf("Number of offset bits: %d bits\n", number_of_offset_bits);
    printf("Block size:            %d bytes\n", block_size);


    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n", (double) cache_statistics.hits / cache_statistics.accesses);
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);

}