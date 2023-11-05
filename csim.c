#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

typedef struct {
    int valid;
    int tag;
    int dirty;
    long timestamp;  // Timestamp for LRU/MRU
} CacheLine;

typedef struct {
    CacheLine *lines;
} CacheSet;

typedef struct {
    int double_references;
    int evictions;
    int hits;
    int misses;
    int dirty_bytes_active;
    int dirty_bytes_evicted;
    int blockOffsetBits;
    int block_size;
    int tagBits;
    int setBits;
    int numSets;
    int Assoc;
    CacheSet* sets;
} Cache;

void simulateCache(Cache* cache, char operation, unsigned long long address, int size);
Cache* initCache(int s, int E, int b);
void freeCache(Cache* cache);
void processTraceFile(Cache* cache, const char* tracefile);
Cache* parseCommandLine(int argc, char *argv[], int* s, int* E, int* b, char** tracefile);
void updateLRU(CacheSet* set, CacheLine* accessedLine, int E) {
    // find the current highest timestamp (most recently used)
    long highestTimestamp = -1;
    for (int i = 0; i < E; i++) {
        if (set->lines[i].timestamp > highestTimestamp) {
            highestTimestamp = set->lines[i].timestamp;
        }
    }

    // The accessed line is most recently used
    accessedLine->timestamp = highestTimestamp + 1;
}
int getMRUTimestamp(CacheSet* set, int E) {
    int mruTimestamp = set->lines[0].timestamp;
    for (int i = 1; i < E; i++) {
        if (set->lines[i].timestamp > mruTimestamp) {
            mruTimestamp = set->lines[i].timestamp;
        }
    }
    return mruTimestamp;
}

int getLRUIndex(CacheSet* set, int E) {
    int lruTimestamp = set->lines[0].timestamp;
    int lruIndex = 0;
    for (int i = 1; i < E; i++) {
        if (set->lines[i].timestamp < lruTimestamp) {
            lruTimestamp = set->lines[i].timestamp;
            lruIndex = i;
        }
    }
    return lruIndex;
}
CacheLine* findCacheLine(CacheSet* set, unsigned long long address, int E, int setIndexBits, int blockOffsetBits) {
    int tag = address >> (setIndexBits + blockOffsetBits); // Correct tag calculation
    for (int i = 0; i < E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            // Cache hit, return the cache line
            return &(set->lines[i]);
        }
    }
    // Cache miss
    return NULL;
}
Cache* initCache(int s, int E, int b) {
    int S = 1 << s;  // Number of sets
    int B = 1 << b;  // Block size
    int tagBits = 64 - s - b ;
    Cache* cache = malloc(sizeof(Cache));
    if (!cache) {
        perror("Error: Unable to allocate memory for cache");
        exit(EXIT_FAILURE);
    }
    cache->sets = malloc(S * sizeof(CacheSet));
    if (!cache->sets) {
        perror("Error: Unable to allocate memory for sets");
        free(cache);
        exit(EXIT_FAILURE);
    }

    // Initialize other fields in Cache structure
    cache->double_references = 0;
    cache->evictions = 0;
    cache->hits = 0;
    cache->misses = 0;
    cache->dirty_bytes_active = 0;
    cache->dirty_bytes_evicted = 0;
    cache->block_size = B;  // Set the block size
    cache->tagBits = tagBits;
    cache->setBits = s;
    cache->blockOffsetBits = b;
    cache->Assoc = E;
    cache->numSets = S; // Assign the computed number of sets to cache->numSets
    for (int i = 0; i < S; i++) {
        cache->sets[i].lines = malloc(E * sizeof(CacheLine));
        if (!cache->sets[i].lines) {
            perror("Error: Unable to allocate memory for lines");
            // Free previously allocated memory
            for (int j = 0; j < i; j++) {
                free(cache->sets[j].lines);
            }
            free(cache->sets);
            free(cache);
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < E; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].timestamp = 0;
        }
    }
    return cache;
}
void freeCache(Cache* cache) {
    int S = cache->numSets;
    for (int i = 0; i < S; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}
void processTraceFile(Cache* cache, const char* tracefile) {
    FILE* file = fopen(tracefile, "r");
    if (!file) {
        perror("Error: Unable to open trace file");
        exit(EXIT_FAILURE);
    }

    char operation;
    unsigned long long address;
    int size;

    while (fscanf(file, " %c %llx,%d", &operation, &address, &size) == 3) {
        // Process each line in the trace file
        simulateCache(cache, operation, address, size);
    }

    fclose(file);
}
Cache* parseCommandLine(int argc, char *argv[], int* s, int* E, int* b, char** tracefile) {
    int opt;
    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
            case 's':
                *s = atoi(optarg);
                break;
            case 'E':
                *E = atoi(optarg);
                break;
            case 'b':
                *b = atoi(optarg);
                break;
            case 't':
                *tracefile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Validate that all required arguments are provided
    if (*s <= 0 || *E <= 0 || *b <= 0 || *tracefile == NULL) {
        fprintf(stderr, "Invalid arguments. Please provide all required parameters.\n");
        exit(EXIT_FAILURE);
    }

    // Print the parsed values
    printf("s = %d, E = %d, b = %d, tracefile = %s\n", *s, *E, *b, *tracefile);

    // Initialize cache based on command-line arguments
    Cache* cache = initCache(*s, *E, *b);

    // Calculate and print cache size
    int S = 1 << *s;  // Number of sets
    int B = 1 << *b;  // Block size
    int C = S * *E * B;  // Cache size
    printf("Calculated Cache Size: %d bytes\n", C);

    return cache;
}
void simulateCache(Cache* cache, char operation, unsigned long long address, int size) {
    printf("simulateCache called with op: %c, addr: %llx, size: %d, blockOffset: %d\n",
           operation, address, size, cache->blockOffsetBits);

    // Extract the set index and tag
    int setIndex = (address >> cache->blockOffsetBits) & ((1 << cache->setBits) - 1);
    int tag = address >> (cache->blockOffsetBits + cache->setBits);
    printf("setIndex: %d, tag: %d\n", setIndex, tag);

    CacheSet* set = &cache->sets[setIndex];
    CacheLine* line = findCacheLine(set, address, cache->Assoc, cache->setBits, cache->blockOffsetBits);

    printf("CacheLine found: %p\n", (void*)line);

    if (operation != 'I') {
        if (line != NULL) { // Hit
            cache->hits++;
            if (line->timestamp == getMRUTimestamp(set, cache->Assoc)) {
                cache->double_references++;
            }
            printf("Cache Hit\n");
            if (operation == 'M' || operation == 'S') {
                if (!line->dirty) {
                    line->dirty = 1; // Mark as dirty due to modification
                    cache->dirty_bytes_active += cache->block_size;
                }
            }
            if (operation == 'M') {
                cache->double_references++; // Increment double references on modification hit
                cache->hits++;
            }
            updateLRU(set, line, cache->Assoc);
        } else { // Miss
            cache->misses++;
            printf("Cache Miss\n");
            if (operation == 'M') {
                // For 'M', it's a miss followed by a hit (load then store)
                cache->hits++;
                cache->double_references++; // Increment double references on modification miss
            }
            // Find LRU line to replace
            int lruIndex = getLRUIndex(set, cache->Assoc);
            CacheLine* lruLine = &set->lines[lruIndex];

            // Evict LRU line if it's valid
            if (lruLine->valid) {
                cache->evictions++;
                if (lruLine->dirty) {
                    cache->dirty_bytes_evicted += cache->block_size;
                    cache->dirty_bytes_active -= cache->block_size;
                }
            }
            lruLine->valid = 1;
            lruLine->tag = tag;
            // Set dirty directly based on the operation 'M' or 'S'
            lruLine->dirty = (operation == 'M' || operation == 'S') ? 1 : 0; // Mark as dirty due to modification if 'M' or 'S'
            if (operation == 'M' || operation == 'S') {
                cache->dirty_bytes_active += cache->block_size; // Increment only if the line was not dirty before
            }
            lruLine->timestamp = getMRUTimestamp(set, cache->Assoc) + 1;
        }
    }
}



int main(int argc, char *argv[]) {
    int s, E, b;
    char* tracefile;
    // Parse command line arguments
    Cache* cache = parseCommandLine(argc, argv, &s, &E, &b, &tracefile);

    // Process the trace file and simulate the cache
    processTraceFile(cache, tracefile);

    // Print summary
    printSummary(cache->hits, cache->misses, cache->evictions, cache->dirty_bytes_evicted, cache->dirty_bytes_active, cache->double_references);

    freeCache(cache);
    return 0;
}