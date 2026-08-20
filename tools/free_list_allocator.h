#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#include <stddef.h>
#include <stdint.h>

typedef struct FL_Allocator_t FL_Allocator;
uint8_t initialise_allocator(size_t size, FL_Allocator *out);
void *allocate_memory(FL_Allocator *alloc, size_t size, size_t alignment);
void *reallocate_memory(FL_Allocator *alloc, void *ptr, size_t new_sz, size_t alignment);
void free_memory(FL_Allocator *alloc, void *ptr);
void move_memory(void *src, void *dst, size_t sz);
void copy_memory(void *src, void *dst, size_t sz);

#endif //ALLOCATOR_H

#ifdef ALLOCATOR_IMPLEMENTATION

#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

//Minimum alignment that the allocator should align to
#define MIN_ALIGNMENT alignof(max_align_t)

//Header used by free memory blocks
typedef struct {
    size_t size;
} Free_Header;

//Header used by allocated memory blocks
typedef struct {
    size_t payload_size; //Size of usable data stored in the block aligned up to the requested alignment. THIS DOES NOT STORE THE ORIGINAL REQUESTED SIZE
    size_t payload_offset; //Distance from the beginning of the block to the actual data (this offset is padding + header)
} Allocated_Header;

typedef enum Color: char {
    BLACK,
    RED
} Color;

//Linked list/rb tree object that holds the free block data
typedef struct Free_Memory_Block {
    //For linked list
    struct Free_Memory_Block *next, *prev;

    //For RB tree
    struct Free_Memory_Block *parent;
    union {
        //Union so we can use ->left/->right or ->child[0]/->child[1]
        struct {
            struct Free_Memory_Block *left, *right;
        };
        struct Free_Memory_Block *child[2];
    };
    Color color;

    Free_Header data;
} Free_Memory_Block;

#define MIN_FREE_BLOCK_SIZE sizeof(Free_Memory_Block) + (MIN_ALIGNMENT - 1)

//Linked list to hold our free blocks
typedef struct {
    Free_Memory_Block header;
} Doubly_Linked_List;

//Initialises the linked list to have a circularly defined sentinel node
void init_doubly_linked_list(Doubly_Linked_List *list) {
    list->header.prev = &list->header;
    list->header.next = &list->header;
    list->header.data.size = 0;
}

//Inserts an element into the linked list
void dll_insert(Free_Memory_Block *node, Free_Memory_Block *prev) {
    node->next = prev->next;
    node->prev = prev;

    prev->next->prev = node;
    prev->next = node;
}

//Removes an element from the linked list
void dll_remove(Free_Memory_Block *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

//Red-Black tree implementation
//Most of this rb tree implementation was based off of the wikipedia article:
//https://en.wikipedia.org/wiki/Red%E2%80%93black_tree#Implementation
//However the deletion code was based off of the pseudocode found in CLRS 3rd Ed:
//https://edutechlearners.com/download/Introduction_to_algorithms-3rd%20Edition.pdf
typedef enum Direction: char {
    LEFT,
    RIGHT,
} Direction;

typedef struct {
    Free_Memory_Block *root;
} RB_Tree;

static Direction direction(const Free_Memory_Block *n) {
    return n == n->parent->right ? RIGHT : LEFT;
}

Free_Memory_Block *rotate_subtree(RB_Tree *tree, Free_Memory_Block *sub, int dir) {
    Free_Memory_Block *sub_parent = sub->parent;
    Free_Memory_Block *new_root = sub->child[1-dir];
    Free_Memory_Block *new_child = new_root->child[dir];

    sub->child[1-dir] = new_child;

    if(new_child) {
        new_child->parent = sub;
    }

    new_root->child[dir] = sub;

    new_root->parent = sub_parent;
    sub->parent = new_root;
    if(sub_parent)
        sub_parent->child[sub == sub_parent->right] = new_root;
    else
        tree->root = new_root;

    return new_root;
}

static inline Color color(Free_Memory_Block *n) {
    return n ? n->color : BLACK;
}

void rb_insert_intrn(RB_Tree *tree, Free_Memory_Block *node, Free_Memory_Block *parent, Direction dir) {
    node->color = RED;
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;

    //Inserting at the root
    if(!parent) {
        tree->root = node;
        return;
    }

    parent->child[dir] = node;

    //rebalance the tree
    do {
        //Case #1
        if(color(parent) == BLACK) return;

        Free_Memory_Block *grandparent = parent->parent;

        //Case #4
        if(!grandparent) {
            parent->color = BLACK;
            return;
        }

        dir = direction(parent);
        Free_Memory_Block *uncle = grandparent->child[1-dir];
        if(!uncle || color(uncle) == BLACK) {
            //Case #5
            if(node == parent->child[1-dir]) {
                rotate_subtree(tree, parent, dir);
                node = parent;
                parent = grandparent->child[dir];
            }

            //Case #6
            rotate_subtree(tree, parent, dir);
            parent->color = BLACK;
            grandparent->color = RED;
            return;
        }

        //Case #2
        parent->color = BLACK;
        uncle->color = BLACK;
        grandparent->color = RED;
        node = grandparent;
    } while((parent = node->parent));

    //Case #3
}

static int compare_blocks(const Free_Memory_Block *a, const Free_Memory_Block *b) {
    if(a->data.size < b->data.size) return -1;
    if(a->data.size > b->data.size) return 1;

    if(a < b) return -1;
    if(a > b) return 1;

    return 0;
}

void rb_insert(RB_Tree *tree, Free_Memory_Block *b) {
    Free_Memory_Block *parent = NULL;
    Free_Memory_Block **link = &tree->root;
    Direction dir = LEFT;

    while(*link) {
        parent = *link;

        if(compare_blocks(b, parent) < 0) {
            link = &parent->left;
            dir = LEFT;
        }
        else {
            link = &parent->right;
            dir = RIGHT;
        }
    }

    rb_insert_intrn(tree, b, parent, dir);
}

void rb_transplant(RB_Tree *tree, Free_Memory_Block *a, Free_Memory_Block *b) {
    if(a->parent == NULL) tree->root = b;
    else if(a == a->parent->left) a->parent->left = b;
    else a->parent->right = b;
    if(b) b->parent = a->parent;
}

Free_Memory_Block *rb_tree_minimum(Free_Memory_Block *x) {
    while(x->left != NULL) {
        x = x->left;
    }

    return x;
}

void rb_delete_fixup(RB_Tree *tree, Free_Memory_Block *node, Free_Memory_Block *parent) {
    if(node == NULL || parent == NULL) return;
    while(node != tree->root && color(node) == BLACK) {
        Direction dir = (node == parent->right) ? RIGHT : LEFT;
        Free_Memory_Block *w = parent->child[1-dir];
        if(color(w) == RED) {
            w->color = BLACK;
            parent->color = RED;
            rotate_subtree(tree, parent, dir);
            w = parent->child[1-dir];
        }
        if(!w || (color(w->left) == BLACK && color(w->right) == BLACK)) {
            if(w) w->color = RED;
            node = parent;
        }
        else {
            if(!w || color(w->child[1-dir]) == BLACK) {
                if(w->child[dir]) w->child[dir]->color = BLACK;
                if(w) w->color = RED;
                rotate_subtree(tree, w, 1-dir);
                w = parent->child[1-dir];
            }
            if(w) w->color = parent->color;
            parent->color = BLACK;
            if(w->child[1-dir]) w->child[1-dir]->color = BLACK;
            rotate_subtree(tree, parent, dir);
            node = tree->root;
        }

        if(node != tree->root) parent = node->parent;
    }
    node->color = BLACK;
}

void rb_remove(RB_Tree *tree, Free_Memory_Block *node) {
    Free_Memory_Block *x, *p, *y = node;
    Color y_orig_col = y->color;

    if(node->left == NULL) {
        x = node->right;
        p = node;
        rb_transplant(tree, node, node->right);
    }
    else if(node->right == NULL) {
        x = node->left;
        p = node;
        rb_transplant(tree, node, node->left);
    }
    else {
        y = rb_tree_minimum(node->right);
        y_orig_col = y->color;
        x = y->right;
        if(y->parent == node) {
            p = y;
            if(x) x->parent = y;
        }
        else {
            p = y->parent;
            rb_transplant(tree, y, y->right);
            y->right = node->right;
            y->right->parent = y;
        }
        rb_transplant(tree, node, y);
        y->left = node->left;
        y->left->parent = y;
        y->color = node->color;
    }
    if(y_orig_col == BLACK) rb_delete_fixup(tree, x, p);

    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
}

//Alignments must be powers of 2
size_t align_up(size_t value, size_t alignment) {
    //Assert that alignments are powers of 2
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

//Helper function to compute the aligned padding we will need to add after the block header to ensure the
//memory region allocated after it has its start aligned
size_t compute_payload_offset(Free_Memory_Block *block, size_t alignment, size_t header_size) {
    uintptr_t start_addr = (uintptr_t)block;
    uintptr_t aligned = align_up(start_addr + header_size, alignment);
    return aligned - header_size - start_addr;
}

uint8_t rb_search(RB_Tree *tree, size_t desired_size, size_t alignment, Free_Memory_Block **out) {
    Free_Memory_Block *best = NULL;
    Free_Memory_Block *b = tree->root;
    size_t padding = 0;

    while(b) {
        padding = compute_payload_offset(b, alignment, sizeof(Allocated_Header));
        if(b->data.size < desired_size + padding) {
            b = b->right;
        }
        else {
            best = b;
            b = b->left;
        }
    }

    if(best == NULL) return 0;
    *out = best;
    return 1;
}

//Our allocator
struct FL_Allocator_t {
    Doubly_Linked_List list;
    RB_Tree tree;
    void *memory;
    size_t size;
};

//Initialises the allocator
uint8_t initialise_allocator(size_t size, FL_Allocator *out) {
    //Actual assinged memory region is the region + the size of the header to track it
    size_t total_sz = size + sizeof(Free_Memory_Block);
    out->memory = malloc(total_sz);
    if(!out->memory) return 0;

    //Setup data structures
    init_doubly_linked_list(&out->list);
    out->tree = (RB_Tree){0};

    //Set up the first free memory to account for the entire assigned memory region
    Free_Memory_Block *first = (Free_Memory_Block *)out->memory;
    first->data.size = total_sz;
    dll_insert(first, &out->list.header);
    rb_insert(&out->tree, first);

    out->size = total_sz;

    return 1;
}

//Destroys the allocator
void destroy_allocator(FL_Allocator *alloc) {
    free(alloc->memory);
    alloc->memory = NULL;
    alloc->size = 0;
}

//Main allocation function
void *allocate_memory(FL_Allocator *alloc, size_t size, size_t alignment) {
    size_t allocation_header_size = sizeof(Allocated_Header);
    alignment = (alignment > MIN_ALIGNMENT) ? alignment : MIN_ALIGNMENT; //Set the alignment to a proper value

    //Search through the free list for a free block that has enough space to allocate the data
    Free_Memory_Block *affected_block;
    if(!rb_search(&alloc->tree, size + allocation_header_size, alignment, &affected_block)) {
        printf("Not enough memory\n");
        return NULL;
    }
    size_t payload_offset = compute_payload_offset(affected_block, alignment, allocation_header_size);

    size_t required_size = size + payload_offset + allocation_header_size;
    size_t rest = affected_block->data.size - required_size;

    //If there is a substantial amount of memory space left over, create a new free region from it
    if(rest >= MIN_FREE_BLOCK_SIZE) {
        Free_Memory_Block *new_free_block = (Free_Memory_Block *)((uint8_t *)affected_block + required_size);
        new_free_block->data.size = rest;
        dll_insert(new_free_block, affected_block);
        rb_insert(&alloc->tree, new_free_block);
    }
    //Otherwise just add it on to the end of the current one
    else {
        size += rest;
    }
    dll_remove(affected_block); //Get rid of the allocated block
    rb_remove(&alloc->tree, affected_block);

    //Creating the block header and the pointer to start of memory
    uintptr_t data_addr = (uintptr_t)affected_block + payload_offset + allocation_header_size;
    uintptr_t header_addr = data_addr - allocation_header_size;
    ((Allocated_Header *)header_addr)->payload_size= size;
    ((Allocated_Header *)header_addr)->payload_offset = payload_offset;

    return (void *)data_addr;
}

//Coalesces two adjacent free regions together to form one large free region
void coalesce(FL_Allocator *alloc, Free_Memory_Block *prev, Free_Memory_Block *new_block) {
    //If the next header isn't the sentinel value and the pointers are adjacnet, combine
    if(new_block->next != &alloc->list.header && (uintptr_t)new_block + new_block->data.size == (uintptr_t)new_block->next) {
        rb_remove(&alloc->tree, new_block);
        rb_remove(&alloc->tree, new_block->next);
        dll_remove(new_block->next);
        new_block->data.size += new_block->next->data.size;
        rb_insert(&alloc->tree, new_block);
    }

    //If the previous header isn't the sentinel value and the pointers are adjacnet, combine
    if(prev != &alloc->list.header && (uintptr_t)prev + prev->data.size == (uintptr_t)new_block) {
        rb_remove(&alloc->tree, prev);
        rb_remove(&alloc->tree, new_block);
        dll_remove(new_block);
        prev->data.size += new_block->data.size;
        rb_insert(&alloc->tree, prev);
    }
}

//Helper function to create a new free block at the given start address
//with the given size. Only blocks of size at least MIN_FREE_BLOCK_SIZE will be created
void create_new_free_block(FL_Allocator *alloc, uintptr_t start, size_t size) {
    if(size < MIN_FREE_BLOCK_SIZE) return; //Early exit

    //Create the new block
    Free_Memory_Block *block = (Free_Memory_Block *)(start);
    block->data.size = size;
    block->next = NULL;
    block->prev = NULL;

    //Seach where to place it in the linked list
    Free_Memory_Block *b = &alloc->list.header;
    while(b->next != &alloc->list.header) {
        if(block < b->next) break;

        b = b->next;
    }
    if(b) {
        dll_insert(block, b);
        rb_insert(&alloc->tree, block);
    }

    //Combine it with adjacent regions
    coalesce(alloc, b, block);
}

//Frees a block of memory
void free_memory(FL_Allocator *alloc, void *ptr) {
    uintptr_t curr_addr = (uintptr_t)ptr; //Changing the pointer address into a value
    uintptr_t header_addr = curr_addr - sizeof(Allocated_Header); //Getting the address of the header for the block
    Allocated_Header *allocation_header = (Allocated_Header *)header_addr; //Getting the header data for the block

    //Create a new free memory block to hold the freed region
    create_new_free_block(alloc, curr_addr - allocation_header->payload_offset, allocation_header->payload_size + allocation_header->payload_offset);
}

//Function to reallocate the region of memory that starts at ptr to a new region
//of new_sz bytes aligned up to match the given alignment. Returns a pointer to the new region
//Follows the following strategy:
//1. Determine the size of the aligned memory region we would need to allocate
//2. If aligned_sz can fit into the current region
//      a. If aligned_sz < curr_region_sz / 2 and curr_region_sz - aligned_sz > MIN_FREE_BLOCK_SIZE
//         spin out the remainder into a new free region. The returned pointer is the original
//      b. Otherwise do nothing and return the original pointer
//3. Else if aligned_sz cannot fit into the current region but there is a free region following the current one
//   which can fit the extra memory, expand into it, and if the leftover memory is > MIN_FREE_BLOCK_SIZE,
//   spin it out into a new free region and return the original pointer
//4. Else if aligned_sz cannot fit into the current region but there is a free region before the current one
//   which can fit the extra memory, expand into it, and if the leftover memory is > MIN_FREE_BLOCK_SIZE,
//   spin it out into a new free region and return a pointer to the padded start of the previous region
//5. If aligned_sz cannot fit into the current region and both adjacent regions are free and the combined
//   region can fit the new aligned memory, create a new allocated region spanning the three blocks, craeting
//   a new free region if the leftover memory is > MIN_FREE_BLOCK_SIZE and return a pointer to the padded
//   start of the previous region
void *reallocate_memory(FL_Allocator *alloc, void *ptr, size_t new_sz, size_t alignment) {
    alignment = alignment < MIN_ALIGNMENT ? MIN_ALIGNMENT : alignment;
    uintptr_t curr_addr = (uintptr_t)ptr;
    uintptr_t header_addr = curr_addr - sizeof(Allocated_Header);
    Allocated_Header *header = (Allocated_Header *)header_addr;
    uintptr_t block_start = curr_addr - header->payload_offset;
    size_t aligned_size = align_up(new_sz, alignment);

    //If the new memory size can fit into the currently allocated region
    if(header->payload_size >= aligned_size) {
        //If the region is being shrunk try and see if we can spin out some
        //of the memory into a new free region
        size_t rest = header->payload_size - aligned_size;

        if(aligned_size < header->payload_size / 2 && rest > MIN_FREE_BLOCK_SIZE) {
            header->payload_size = aligned_size;
            create_new_free_block(alloc, curr_addr + aligned_size, rest);
        }

        return ptr;
    }
    else {
        size_t needed = aligned_size - header->payload_size; //How much data we need to fit

        //Find adjacent free blocks (if they exists)
        uintptr_t next_block = curr_addr + header->payload_size; //The address of where the next free block should be
        Free_Memory_Block *foundr = NULL; //Free right block
        Free_Memory_Block *foundl = NULL; //Free left block
        Free_Memory_Block *b = &alloc->list.header; //Searching block

        while(b->next != &alloc->list.header) {
            if(next_block < (uintptr_t)b->next) break;
            else if(next_block == (uintptr_t)b->next) {
                foundr = b->next;
                break;
            }
            else if(block_start == (uintptr_t)b->next + b->data.size) foundl = b->next;

            b = b->next;
        }

        //Adjust data on the free regions
        uintptr_t offset = 0;
        size_t required = aligned_size;
        size_t available = header->payload_offset + header->payload_size;
        if(foundl != NULL) {
            offset = compute_payload_offset(foundl, alignment, sizeof(Allocated_Header));
            available += foundl->data.size;
            required += offset;
        }
        if(foundr) available += foundr->data.size;

        //Size is growing, current block isn't big enough, but the next block is free
        //and has enough space to fit the rest of the required data
        if(foundr != NULL && foundr->data.size >= needed) {
            dll_remove(foundr);
            rb_remove(&alloc->tree, foundr);

            size_t remainder = foundr->data.size - needed;
            //If the new data block is too large keep the remainder as a free block
            if(remainder > MIN_FREE_BLOCK_SIZE) {
                create_new_free_block(alloc, next_block + needed, remainder);
                foundr->data.size = needed;
            }
            header->payload_size += foundr->data.size;
            return ptr;
        }
        //Size is growing, current block isn't big enough, but both previous block is free
        //and has enough space to contain all of the extra data. Also handles the case
        //where both sides are free and can be used
        else if (foundl != NULL && available >= required) {
            //Get rid of all of the old blocks
            dll_remove(foundl);
            rb_remove(&alloc->tree, foundl);
            if(foundr != NULL) {
                dll_remove(foundr);
                rb_remove(&alloc->tree, foundr);
            }

            //Compute the new allocated block values
            uintptr_t new_start = (uintptr_t)foundl + offset;

            //Check if there is going to be any significant remainder
            size_t remainder = available - required;
            uint8_t need_new_block = 0;
            //Do not allocate the free block memory now as it may override some of the data we need to copy into the
            //new allocated region. Just set a flag to true
            if(remainder > MIN_FREE_BLOCK_SIZE) need_new_block = 1;
            else aligned_size += remainder;

            //Create the new allocated region and copy the data into it
            Allocated_Header *new_header = (Allocated_Header *)(new_start - sizeof(Allocated_Header));
            new_header->payload_size = aligned_size;
            new_header->payload_offset = offset;
            move_memory(ptr, (void *)new_start, header->payload_size);

            //Create the new free region if necessary
            if(need_new_block) create_new_free_block(alloc, new_start + aligned_size, remainder);

            return (void *)new_start;
        }
        //Have to realloc to a new block
        else {
            void *new_block = allocate_memory(alloc, new_sz, alignment);
            if(new_block == NULL) return NULL;
            move_memory(ptr, new_block, header->payload_size);
            free_memory(alloc, ptr);
            return new_block;
        }
    }
}

#endif //ALLOCATOR_IMPLEMENTATION
