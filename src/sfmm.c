/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

int find_index(int fibnum){
    int trail = 1;
    int current = 1;
    int index = 0;
    while (fibnum > current && index < NUM_FREE_LISTS-2){
        if (index > 1){
            if (fibnum <= current+trail) {
                index++;
                break;
            }
        }
        index++;
        int temp = trail;
        trail = current;
        current = current + temp;
    }
    return index;
}

void insert(sf_block *block, int index){
    if (&sf_free_list_heads[index] == sf_free_list_heads[index].body.links.next)
        sf_free_list_heads[index].body.links.prev = block;
    else sf_free_list_heads[index].body.links.next->body.links.prev = block;
    block->body.links.next = sf_free_list_heads[index].body.links.next;
    sf_free_list_heads[index].body.links.next = block;
    block->body.links.prev = &sf_free_list_heads[index];
}

void remove_from(sf_block *block, int index){
    sf_block *listnext = sf_free_list_heads[index].body.links.next;
    while (&sf_free_list_heads[index] != listnext){ // while next isn't sentinel
        if (listnext == block){ // if next has address of next
            listnext->body.links.prev->body.links.next = listnext->body.links.next;
            listnext->body.links.next->body.links.prev = listnext->body.links.prev;
            break;
        }
        listnext = listnext->body.links.next;
    }
}

void *sf_malloc(size_t size) {
    //check if prologue exists?
    sf_block *test = sf_mem_start()+48;
    if (test->header != 67){
        sf_mem_grow();

        sf_block *prologue = sf_mem_start() + 48;// start of prologue header
        prologue->header = 67;

        sf_block *wilderness = prologue + (prologue->header&BLOCK_SIZE_MASK)/sizeof(sf_block); //start of wilderness
        wilderness->header = PAGE_SZ - 128 +2;
        wilderness->prev_footer= prologue->header;
        wilderness->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        wilderness->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];

        sf_block *epilogue = sf_mem_end() - 16;
        epilogue->header = 1;
        epilogue->prev_footer = wilderness->header;
        //sf_show_block(epilogue);

        for (int i=0; i<NUM_FREE_LISTS-1; i++){
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = wilderness;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = wilderness;
    }
    //sf_show_heap();

    if (size == 0)
        return NULL;

    // Determine Size to allocate
    //printf("size: %ld\n", size);
    size = size + sizeof(sf_header);
    size_t total = 64;
    while (total < size)
        total += 64;

    // Search every list
    int fibnum = (total/64);
    int index = find_index(fibnum);

    //printf("Total size to allocate: %ld\n", total);
    //printf("At Index: %d\n", index);

    while (index < NUM_FREE_LISTS-1){
        sf_block *sentinel = &sf_free_list_heads[index]; //sentinel
        //printf("sentinel: %p\n", sentinel);
        sf_block *next = sentinel->body.links.next;

        while (sentinel != next){ // while next != sentinel
            size_t blocksize = next->header & BLOCK_SIZE_MASK; // size of block
            sf_block *nextblock = next + blocksize/sizeof(sf_block); // block after current
            //printf("blocksize: %d\n", blocksize);
            if (blocksize>= total){
                //remove block from current list
                int fibnum = (next->header& BLOCK_SIZE_MASK)/64;
                int index = find_index(fibnum);

                remove_from(next, index);
                //

                sf_block *allocated = next;
                allocated->header = total+1; // mark allocated
                if (allocated->prev_footer%2==1) allocated->header +=2; // if prev is al, add 2
                nextblock->header += 2; //set next block's pal
                // Check for splits
                if (blocksize - total >= 64){ // if remaining space is >= 64, have to split
                    sf_block *remainder = next + total/sizeof(sf_block); //address of remainder block
                    remainder->header = blocksize - total + 2; // blocksize of remainder & mark pal
                    remainder->prev_footer = allocated->header;

                    // insert remainder into freelist
                    int fibnum = (remainder->header& BLOCK_SIZE_MASK)/64;
                    int index = find_index(fibnum);
                    //insert
                    insert(remainder, index);

                    nextblock->header -=2; // pal 0
                    next = remainder; // change address to remainder
                }
                nextblock->prev_footer = next->header;
                 // if it is exact size
                //sf_show_heap();
                return allocated->body.payload; // return pointer to payload
            }
            else{
                next = next->body.links.next; // if blocksize isn't enough, check next
            }
        }
        index++; // if next is sentinel, check next size class
    }
    // if it has to go to wilderness
    // check if wilderness still has enough space
    sf_block *epilogue = sf_mem_end() - 16;
    size_t epilogueprev = epilogue->prev_footer;
    sf_block *wilderness = epilogue - (epilogue->prev_footer - 2)/sizeof(sf_block);
    if (wilderness->header < total || wilderness->header == 0 ||
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next == &sf_free_list_heads[NUM_FREE_LISTS-1]){ // or no wilderness
        int temp = total;
        do {
            if (sf_mem_grow() == NULL){ // if malloc too big
                sf_block *epilogue = sf_mem_end() - 16;
                epilogue->header = 3;
                epilogue->prev_footer = wilderness->header;
                return NULL;
            }
            temp -= PAGE_SZ;
            if (wilderness->header%2 == 0) // if wilder isn't used up
                wilderness->header += PAGE_SZ;
        }
        while (temp > PAGE_SZ); // if total larger than a page
        // new epilogue
        epilogue = sf_mem_end() - 16;
        //if (wilderness->header%2 == 0) // if wilder isn't used up
        //    epilogue->prev_footer = epilogueprev;
        //epilogue->header = 1;
        //epilogue->prev_footer = wilderness->header;
    }
    //create wilderness if there is none
    if (sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next == &sf_free_list_heads[NUM_FREE_LISTS-1]){ //if no wilderness
        wilderness = sf_mem_end()-16-PAGE_SZ;
        wilderness->header = PAGE_SZ+2;
        wilderness->prev_footer=epilogueprev; // set prev_footer to old epilogue prev
        //add to list
        wilderness->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        wilderness->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = wilderness;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = wilderness;
    }
    size_t wilderheadersize = wilderness->header& BLOCK_SIZE_MASK;

    // create allocated block
    sf_block *allocated = wilderness;
    allocated->header = total + 1 + 2; //marked allocated
    allocated->prev_footer = wilderness->prev_footer;
    // set new wilderness
    wilderness = sf_mem_end() - 16 - ((wilderheadersize) - total);
    //if (sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next == &sf_free_list_heads[NUM_FREE_LISTS-1]) // no wilderness
    //    wilderness->header = wilderheadersize - total-1;
    wilderness->header = wilderheadersize - total+2; // new wilder size
    wilderness->prev_footer = allocated->header;

    if (wilderness->header >= 64){
        wilderness->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        wilderness->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = wilderness;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = wilderness;
        epilogue->header = 1;
        epilogue->prev_footer = wilderness->header;
    }
    else{ // if wilderness is too small, empty wilder list
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
        epilogue->header = 3;
        epilogue->prev_footer = allocated->header;
    }


    //
    //sf_show_heap();

    return allocated->body.payload;
}

void sf_free(void *pp) {
    //printf("%ld\n", (uintptr_t)pp);
    if (pp == NULL) abort();
    if (((uintptr_t)pp % 64) != 0) abort();
    sf_block *block = pp-16;
    if (block->header%2 ==0) abort(); //check if al is 0
    if (pp < sf_mem_start()+112 || pp >= sf_mem_end()-16) abort(); //check bounds
    if ((block->header >> 1 & 1) == 0){
        if (block->prev_footer%2 == 1) abort();
    }

    // if pp is valid
    //find current block size
    size_t blocksize = block->header& BLOCK_SIZE_MASK;

    sf_block *next = block + blocksize/sizeof(sf_block); //??

    // free alone CASE 1
    if (next->header % 2 != 0 && block->prev_footer%2 !=0){
        block->header = block->header - 1; //set al
        next->header -= 2; //set pal
        next->prev_footer = block->header ;
    }

    // check if next block is free but not prev CASE 2
    if (next->header % 2 == 0 ){//&& block->prev_footer%2 != 0){
        block->header = (block->header& BLOCK_SIZE_MASK) + (next->header& BLOCK_SIZE_MASK) ; // add next size to current block
        sf_block *nextnext = next+(next->header& BLOCK_SIZE_MASK)/sizeof(sf_block); // set next after next
        nextnext->prev_footer = block->header; // set pal to 0

        // remove coalesced block from list it was in
        int fibnum = (next->header& BLOCK_SIZE_MASK)/64;
        int index = find_index(fibnum);
        remove_from(next,index);
    }

    // check if prev block is free but not next CASE 3
    if (block->prev_footer%2 == 0){// && next->header % 2 != 0 ){
        size_t prevsize = block->prev_footer& BLOCK_SIZE_MASK; //size of prev block
        sf_block *prev = block - prevsize/sizeof(sf_block); //address of the prev block

        //remove coalesced block from list it was in
        int fibnum = (prevsize)/64;
        int index = find_index(fibnum);
        remove_from(prev,index);

        // CASE 4
        if (next->header % 2 == 0) {
            prev->header = block->header + prevsize+2 ; // set prev header
            sf_block *nextnext = prev + (prev->header& BLOCK_SIZE_MASK)/sizeof(sf_block);
            nextnext->prev_footer = prev->header;
        }
        else {
            prev->header = block->header-1 + prevsize+2 ;
            //sf_block *next = prev + prev->header; // find next after current free
            next->prev_footer = prev->header; // set next prev footer
            next->header -= 2;
        }

        block = prev; // block head at prev
    }

    // set next and prev
    // insert into free lists
    if ((block + (block->header& BLOCK_SIZE_MASK)/sizeof(sf_block))->header == 1 ){// if is wilderness
        //sf_free_list_heads[NUM_FREE_LISTS-1] = *block;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = block;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = block;
        block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    }
    else{ // if not wilderness, insert into appropiate class
        int fibnum = (block->header& BLOCK_SIZE_MASK)/64;
        int index = find_index(fibnum);
        //add block to list
        // if list only has sentinel
        insert(block, index);

    }
    return;
}

void *sf_realloc(void *pp, size_t size) {
    if (pp == NULL) sf_errno = EINVAL;
    if (((uintptr_t)pp % 64) != 0) sf_errno = EINVAL;
    sf_block *block = pp-16;
    if (block->header%2 ==0) sf_errno = EINVAL; //check if al is 0
    if (pp < sf_mem_start()+112 || pp >= sf_mem_end()-16) sf_errno = EINVAL; //check bounds
    if ((block->header >> 1 & 1) == 0){
        if (block->prev_footer%2 == 1) sf_errno = EINVAL;
    }
    if (sf_errno == EINVAL) return NULL;

    // if pointer valid but size = 0
    if (size == 0){
        sf_free(pp);
        return NULL;
    }
    size_t blocksize = block->header& BLOCK_SIZE_MASK;

    size = size + sizeof(sf_header);
    // reallocating to larger size
    if (blocksize < size){
        void *new_space = sf_malloc(size);
        if (new_space == NULL) return NULL;
        memcpy(new_space, pp, (block->header&BLOCK_SIZE_MASK) - 8);
        sf_free(pp);
        return new_space;
    }

    // reallocating to smaller size
    if (blocksize >= size){
        size_t total = 64;
        while (total < size)
            total += 64;
        // if will splinter
        if (total == blocksize){
            memcpy(pp, pp, size-sizeof(sf_header));
        }
        // split
        if (total < blocksize){
            block->header = block->header - (blocksize-total); //update block header
            sf_block *free = block + total/sizeof(sf_block); // address of block to free
            free->header = blocksize - total +2; // pal to 2
            free->prev_footer = block->header;

            sf_block *next = free + (free->header&BLOCK_SIZE_MASK)/sizeof(sf_block); // next block after free

            //coalese with next if free
            if (next->header % 2 == 0 ){//&& block->prev_footer%2 != 0){
                free->header = (free->header& BLOCK_SIZE_MASK) + (next->header& BLOCK_SIZE_MASK)+2 ; // add next size to current block
                sf_block *nextnext = next+(next->header& BLOCK_SIZE_MASK)/sizeof(sf_block); // set next after next
                nextnext->prev_footer = free->header;

                // if next is wilderness
                if (nextnext->header == 1){
                    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = free;
                    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = free;
                    free->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
                    free->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
                    memcpy(pp,pp,size);
                    return pp;
                }
                else{
                    //nextnext->header -= 2; //set pal to 0
                    // remove coalesced block from list it was in
                    int fibnum = (next->header& BLOCK_SIZE_MASK)/64;
                    int index = find_index(fibnum);
                    remove_from(next,index);
                }
            }
            else{ // if no coalese
                next->prev_footer = free->header;
                next->header-=2; //set pal
            //sf_free(free+16);
            }

            int index = find_index((free->header&BLOCK_SIZE_MASK)/64);
            insert(free, index);

            memcpy(pp,pp, size-sizeof(sf_header));
        }
        return pp;

    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    if (size == 0)
        return NULL;

    if (align < 64 || (align && (!(align&(align-1)))) == 0){
        sf_errno = EINVAL;
        return NULL;
    }

    size_t sizetemp = align + size + 64;// +sizeof(sf_header);
    size_t total = 64;//size allocated
    while (total < sizetemp+sizeof(sf_header))
        total += 64;

    size += sizeof(sf_header);
    size_t blocksize = 64; // size
    while (blocksize < size)
        blocksize += 64;

    void* payload = sf_malloc(sizetemp);
    if (payload == NULL) return NULL;

    sf_block *block = payload - 16;
    //sf_block *next = block + (block->header&BLOCK_SIZE_MASK); // address of next block

    if ((uintptr_t)payload % align != 0){ // if payload isn't aligned with requirement
        // find aligned address for allocate block
        size_t addresstoadd = 0;
        while ((uintptr_t)payload % align != 0){
            payload += 64; //move payload's address
            addresstoadd+=64;
        }
        sf_block *allocate = block+addresstoadd/sizeof(sf_block); // address of block to allocate
        allocate->header = total-(addresstoadd) +1+2; // set al

        block->header = block->header - (allocate->header&BLOCK_SIZE_MASK); // set newblock size header
        allocate->prev_footer = block->header;

        sf_free(block->body.payload); // free space before allocated

        //free space after allocated, if any
        if ((allocate->header&BLOCK_SIZE_MASK) - blocksize >= 64){ // if there's extra space after
            sf_block *tofree = allocate + blocksize/sizeof(sf_block); // address of block to free
            tofree->header = (allocate->header&BLOCK_SIZE_MASK) - blocksize + 2 + 1;
            allocate->header = blocksize+1;//update allocated header

            tofree->prev_footer = allocate->header;
            sf_free(tofree->body.payload);
            //tofree->header
        }
        return allocate->body.payload;

    }
    else{// if payload satisfies align requirement
        // update header
        block->header = block->header - (total - blocksize);
        sf_block *tofree = block + blocksize/sizeof(sf_block);
        tofree->header = total-blocksize + 2 +1;
        tofree->prev_footer=block->header;
        sf_free(tofree->body.payload); //free the end unused space

        //next->prev_footer = tofree->header;
        return block->body.payload;
    }

}
