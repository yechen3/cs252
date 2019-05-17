#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */
#ifdef TEST_ASSERT
  inline static void assert(int e) {
    if (!e) {
      const char * msg = "Assertion Failed!\n";
      write(2, msg, strlen(msg));
      exit(1);
    }
  }
#else
  #include <assert.h>
#endif

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for manipulating the freelist #Yechen Liu
static inline header * ptr_to_approp_freelist(size_t block_size);
static inline void delete_from_freelist(header * hdr);
static inline void insert_into_freelist(header * hdr);
static inline void insert_chunk_to_freelist();
static inline header * split_block(header * cur, size_t block_size);
static inline void update_freelist(header * old_hdr, header * new_hdr, size_t block_size);
static inline bool last_freelist(header * hdr);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;

/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_state(fp,FENCEPOST);
	set_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_state(hdr, UNALLOCATED);
  set_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->left_size = ALLOC_HEADER_SIZE;
  return hdr;
}

/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */
static inline header * allocate_object(size_t raw_size) {
  // TODO implement allocation #Yechen Liu
  // calculate the required block size
  size_t block_size = ((raw_size > (2*sizeof(size_t))? raw_size + ALLOC_HEADER_SIZE : sizeof(header)) + sizeof(size_t)-1) & ~(sizeof(size_t)-1);
  
  // find the appropriate free list 
  header * hdr = ptr_to_approp_freelist(block_size);
  while (hdr == NULL) {
    insert_chunk_to_freelist();
    hdr = ptr_to_approp_freelist(block_size);
  }

  set_state(hdr, ALLOCATED);

  return hdr;
}

/**
 * @brief Helper allocate more memory when the current chunk run out
 * 
 */
static inline void insert_chunk_to_freelist() {
  header * block = allocate_chunk(ARENA_SIZE);
  header * prevblock = block;
  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  bool updated = false;

  // coalescing when the two chunks are adjacent
  if (((char *)prevFencePost - ALLOC_HEADER_SIZE) == (char *) lastFencePost) {
    header * left_to_lastFP = get_left_header(lastFencePost);
    // Unallocated memory before last fence post.
    size_t left_block_size = 0;
    if (get_state(left_to_lastFP) == UNALLOCATED) {
      left_block_size = get_size(left_to_lastFP);
      if (last_freelist(left_to_lastFP)) {
        updated = true;
      } else{
        delete_from_freelist(left_to_lastFP);
      }
    }
    block = get_header_from_offset(lastFencePost, -left_block_size);
    set_size(block, 2*ALLOC_HEADER_SIZE + get_size(prevblock) + left_block_size);
    block->left_size = left_block_size? left_to_lastFP->left_size : lastFencePost->left_size;
    set_state(block, UNALLOCATED);
  } else {
    insert_os_chunk(prevFencePost);
  }

  lastFencePost = get_header_from_offset(prevblock, get_size(prevblock));
  lastFencePost->left_size = get_size(block);

  // Insert chunk into the free list
  if (!updated){
    header * freelist = &freelistSentinels[N_LISTS - 1];
    block->next = freelist->next;
    freelist->next = block;
    block->next->prev = block;
    block->prev = freelist;
  }
}

/**
 * @brief Helper to find the appropriate free list
 * 
 * @param block_size including header
 * 
 * @return A pointer to the header of block
 */
 static inline header * ptr_to_approp_freelist(size_t block_size) {
  int idx = (int) (block_size - ALLOC_HEADER_SIZE) / sizeof(size_t) - 1;
  idx = idx < 58? idx : 58; 
  for (int i = idx; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for(header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (get_size(cur) > block_size) {
        return split_block(cur, block_size);
      }
      if (get_size(cur) == block_size){
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
        return cur;
      }
    } 
  }
  return NULL;
}

/**
 * @brief Helper to split the block when necessary
 *
 * @param cur pointer points to current block
 * @param block_size including header
 *
 * @return A pointer to the block will be allocated
 */
static inline header * split_block(header * cur, size_t block_size) {
  if (get_size(cur) - block_size >= sizeof(header)) {
    set_size(cur, get_size(cur) - block_size);
    header * firstblock = cur;
    cur = (header *)((char *)cur + get_size(cur));
    set_size(cur, block_size);
    
    // update the block
    cur->left_size = get_size(firstblock);
    
    // update the block to the right
    header * rightblock = (header *)((char *)cur + get_size(cur));
    rightblock->left_size = get_size(cur);
    
    // put firstblock into appropriate freelist
    if (get_size(firstblock) < (N_LISTS+2) * sizeof(size_t)) {
      firstblock->prev->next = firstblock->next;
      firstblock->next->prev = firstblock->prev;
      insert_into_freelist(firstblock);
    }
  }
  return cur;
}

/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
  // TODO implement deallocation #Yechen Liu
  header * hdr = ptr_to_header(p);

  if (get_state(hdr) == UNALLOCATED) {
    fprintf(stderr, "Double Free Detected\n");
    assert(false);
    exit(1);
  }
  set_state(hdr, UNALLOCATED);

  // check for availablity to coalesce
  header * rightblock = (header *)((char *)hdr + get_size(hdr)),
      * leftblock = (header *)((char *)hdr - hdr->left_size);

  if (get_state(rightblock) == UNALLOCATED & 
        get_state(leftblock) == UNALLOCATED) {
      size_t new_size = get_size(hdr) + get_size(leftblock) + get_size(rightblock);
      if (last_freelist(leftblock)) {
        delete_from_freelist(rightblock);
        set_size(leftblock, new_size);
      } else if (last_freelist(rightblock)) {
        delete_from_freelist(leftblock);
        update_freelist(rightblock, leftblock, new_size);
      } else {
        delete_from_freelist(leftblock);
        delete_from_freelist(rightblock);
        set_size(leftblock, new_size);
        insert_into_freelist(leftblock);
      }
      get_right_header(rightblock)->left_size = new_size;
  } else if (get_state(rightblock) == UNALLOCATED) {
    if (last_freelist(rightblock)) {
      update_freelist(rightblock, hdr, get_size(hdr) + get_size(rightblock));
    } else {
      delete_from_freelist(rightblock);
      set_size(hdr, get_size(hdr) + get_size(rightblock));
      insert_into_freelist(hdr);
    }
    rightblock = (header *)((char *)hdr + get_size(hdr));
    rightblock->left_size = get_size(hdr);
  } else if (get_state(leftblock) == UNALLOCATED) {
    if (last_freelist(leftblock)) {
      set_size(leftblock, get_size(hdr) + get_size(leftblock));
    } else {
      delete_from_freelist(leftblock);
      set_size(leftblock, get_size(hdr) + get_size(leftblock));
      insert_into_freelist(leftblock);
    }
    rightblock->left_size = get_size(leftblock);
  } else {
    insert_into_freelist(hdr);
  }
  set_state(hdr, UNALLOCATED);
}

/**
* @brief Helper to determine whether a block is in freelistSentinel[58]
*
* @param hdr The pointer to header
*
* @return True if it is in freelistSentinel[58]
*/
static inline bool last_freelist(header * hdr){
  return get_size(hdr) >= (N_LISTS+2)*sizeof(size_t);
}

/**
* @brief Helper to update the header not to remove it
*
* @param old_hdr The pointer to header has already in freelist
* @param new_hdr The pointer to header that need to put in freelist
*/
static inline void update_freelist(header * old_hdr, header * new_hdr, size_t block_size) {
  set_size(new_hdr, block_size);
  new_hdr->next = old_hdr->next;
  new_hdr->prev = old_hdr->prev;
  old_hdr->next->prev = new_hdr;
  old_hdr->prev->next = old_hdr;
}

/**
 * @brief Helper to delete a block from freelist or update 
 * the block in freelist
 * 
 * @param hdr The pointer to a header
 *
 * @return True if the block is deleted
 */
static inline void delete_from_freelist(header * hdr) {
  int idx = (get_size(hdr)-ALLOC_HEADER_SIZE)/sizeof(size_t)-1;
  idx = idx > (N_LISTS-1)? N_LISTS-1 : idx;

  header * freelist = &freelistSentinels[idx];
  for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
    if (cur == hdr) {
      cur->prev->next = cur->next;
      cur->next->prev = cur->prev; 
    }
  }
}

/**
 * @brief Helper to insert a block into the freelist
 * 
 * @param hdr The pointer to a header 
 */
static inline void insert_into_freelist(header * hdr) {
  int idx = (get_size(hdr)-ALLOC_HEADER_SIZE)/sizeof(size_t)-1;
  idx = idx > (N_LISTS-1)? N_LISTS-1 : idx;

  header * freelist = &freelistSentinels[idx];
  hdr->next = freelist->next;
  freelist->next = hdr;
  freelist->next->next->prev = hdr;
  hdr->prev = freelist;
}

/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);

  lastFencePost = get_header_from_offset(block, get_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
  if (!size) {
    return NULL;
  }
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size); 
  pthread_mutex_unlock(&mutex);
  return (header *)((char *)hdr + ALLOC_HEADER_SIZE);
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  if (p != NULL){
    pthread_mutex_lock(&mutex);
    deallocate_object(p);
    pthread_mutex_unlock(&mutex);
  }
}

bool verify() {
  return verify_freelist() && verify_tags();
}
