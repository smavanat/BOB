#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

//TODO: Test
//      Make it work for different vulkan memory types
//      Figure out how to get the metadata memory to expand/contract as needed

//Checks if a vulkan function has succeeded, returns 0, calls the functions to free data, and prints failure if not
#define VULKAN_ERROR(func, failure_string, ...) do {     \
    if((func) != VK_SUCCESS) {                           \
        printf("%s\n", (failure_string));                \
        __VA_ARGS__;                                     \
        return 0;                                        \
    }                                                    \
} while(0)

//Minimum alignment that the allocator should align to
#define MIN_ALIGNMENT alignof(max_align_t)

//Header used by free memory blocks
typedef struct {
    VkDeviceSize size;
    VkDeviceSize offset;
} Free_Header;

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

//Allocation header returned to the user
typedef struct {
    VkDeviceMemory memory; //Vulkan memory used
    VkDeviceSize size; //Size of usable data stored in the block aligned up to the requested alignment. THIS DOES NOT STORE THE ORIGINAL REQUESTED SIZE
    VkDeviceSize offset; //Offset from start of memory block
    VkDeviceSize padding; //Padding placed in front of the memory region to ensure that it is aligned
} Vulkan_Allocation;

//================================== POOL ALLOCATOR FOR METADATA =====================================
//Pool allocator is just a linked list

//Types of metadata that can be stored in a pool allocator
typedef enum {
    VULKAN_FREE_BLOCK, //Free block headers
    VULKAN_ALLOCATED_BLOCK, //Allocated block headers
} Vulkan_Metadata_Type;

//A single node in the pool allocator doubly linked list
typedef struct Vulkan_Metadata_Elem{
    Vulkan_Metadata_Type type; //Type of the node. This should match the pool allocator's type as well
    struct Vulkan_Metadata_Elem *prev, *next; //Links to other nodes

    //Data stored in the node
    union {
        Vulkan_Allocation allocation_data;
        Free_Memory_Block free_data;
    };
} Vulkan_Metadata_Elem;

//Pool allocator itself
typedef struct {
    Vulkan_Metadata_Elem *allocation_data; //Memory region that actually holds the data
    Vulkan_Metadata_Elem header; //Pointer to the start and end of the circular linked list
} Vulkan_Metadata_Pool;

//Initialises the pool allocator and all of the nodes in the linked list that make it up
uint8_t initialise_metadata_pool(size_t capacity, Vulkan_Metadata_Pool *out) {
    //Allocate the memory region to store the data
    out->allocation_data = malloc(sizeof(Vulkan_Metadata_Elem) * capacity);
    if(!out->allocation_data) return 0;

    //Set the links between the nodes
    for(size_t i = 0; i < capacity; i++) {
        if(i < capacity - 1) out->allocation_data[i].next = &out->allocation_data[i+1];
        if(i > 0) out->allocation_data[i].prev = &out->allocation_data[i-1];
    }

    //Set the links to the header pointer node
    out->allocation_data[0].prev = &out->header;
    out->allocation_data[capacity-1].next = &out->header;
    out->header.next = &out->allocation_data[0];
    out->header.prev = &out->allocation_data[capacity-1];

    return 1;
}

//Pops the first node from the list and assigns a pointer to the start of the
//metadata stored in the pool allocator node to the out argument
//It is technically possible to store data of both free and allocated headers in the
//same pool allocator as we use a union type
uint8_t create_metadata(Vulkan_Metadata_Pool *pool, Vulkan_Metadata_Type type, void **out) {
    //Metadata exhausted
    if(pool->header.next == &pool->header) {
        printf("Metadata exhausted\n");
        return 0;
    }

    //Get the next available node
    Vulkan_Metadata_Elem *data = pool->header.next;

    //Remove it from the linked list
    pool->header.next = data->next;
    data->next->prev = &pool->header;
    data->type = type;

    //Get the offset to the start of the data
    switch (type) {
        case VULKAN_FREE_BLOCK: *out = (char *)data + offsetof(Vulkan_Metadata_Elem, free_data); break;
        case VULKAN_ALLOCATED_BLOCK: *out = (char *)data + offsetof(Vulkan_Metadata_Elem, allocation_data); break;
    }

    return 1;
}

//Given a pointer to the start of a data offset, uses pointer arithmetic to find the start of
//the pool allocator linked list node that actually stores the data and adds it to the end of the
//allocator linked list
uint8_t destroy_metadata(Vulkan_Metadata_Pool *pool, Vulkan_Metadata_Type type, void *data) {
    //Find the start of the node. No need to do capacity checks, since we can just keep adding
    //nodes. If the user wants, this means they can allocate their own node memory and
    //add them to the end of the list as long as they are careful
    Vulkan_Metadata_Elem *node;
    switch (type) {
        case VULKAN_FREE_BLOCK: node = (Vulkan_Metadata_Elem *)((char *)data - offsetof(Vulkan_Metadata_Elem, free_data)); break;
        case VULKAN_ALLOCATED_BLOCK: node = (Vulkan_Metadata_Elem *)((char *)data - offsetof(Vulkan_Metadata_Elem, allocation_data)); break;
    }

    //Add it to the back of the linked list
    Vulkan_Metadata_Elem *prev = pool->header.prev;
    node->next = prev->next;
    node->prev = prev;
    prev->next->prev = node;
    prev->next = node;

    return 1;
}

//Frees the memory used by a pool allocator
void destroy_pool_allocator(Vulkan_Metadata_Pool *pool) {
    free(pool->allocation_data);
}

#define MIN_FREE_BLOCK_SIZE 64

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

    if(a->data.offset < b->data.offset) return -1;
    if(a->data.offset > b->data.offset) return 1;

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
size_t compute_payload_offset(VkDeviceSize offset, VkDeviceSize alignment) {
    return align_up(offset, alignment) - offset;
}

//Performs best first search on the rb tree
uint8_t rb_search(RB_Tree *tree, size_t desired_size, size_t alignment, Free_Memory_Block **out) {
    Free_Memory_Block *best = NULL;
    Free_Memory_Block *b = tree->root;
    size_t padding = 0;

    while(b) {
        padding = compute_payload_offset(b->data.offset, alignment);
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
typedef struct Vulkan_Allocator_t {
    Doubly_Linked_List list;
    RB_Tree tree;
    Vulkan_Metadata_Pool meta_pool;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDevice device;
    VkMemoryType type;
} Vulkan_Allocator;

//Initialises the allocator
uint8_t initialise_allocator(size_t size, Vulkan_Allocator *out, VkDevice device) {
    //Actual assinged memory region is the region + the size of the header to track it
    size_t total_sz = size;

    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL, .allocationSize = total_sz };
    VULKAN_ERROR(vkAllocateMemory(device, &alloc_info, NULL, &out->memory), "Failed to allocate initial memory\n");

    //Setup data structures
    init_doubly_linked_list(&out->list);
    out->tree = (RB_Tree){0};
    initialise_metadata_pool(1024, &out->meta_pool);

    //Set up the first free memory to account for the entire assigned memory region
    Free_Memory_Block *first;
    if(!create_metadata(&out->meta_pool, VULKAN_FREE_BLOCK, (void **)&first)) return 0;
    first->data.size = total_sz;
    first->data.offset = 0;
    dll_insert(first, &out->list.header);
    rb_insert(&out->tree, first);

    out->size = total_sz;
    out->device = device;

    return 1;
}

//Destroys the allocator
void destroy_allocator(Vulkan_Allocator *alloc, VkDevice device) {
    destroy_pool_allocator(&alloc->meta_pool);
    vkFreeMemory(device, alloc->memory, NULL);
    alloc->memory = NULL;
    alloc->size = 0;
}

void destroy_free_block(Vulkan_Allocator *alloc, Free_Memory_Block *block) {
    dll_remove(block);
    rb_remove(&alloc->tree, block);
    destroy_metadata(&alloc->meta_pool, VULKAN_FREE_BLOCK, block);
}

//Main allocation function
uint8_t allocate_memory(Vulkan_Allocator *alloc, size_t size, size_t alignment, Vulkan_Allocation **out_alloc) {
    alignment = (alignment > MIN_ALIGNMENT) ? alignment : MIN_ALIGNMENT; //Set the alignment to a proper value

    //Search through the free list for a free block that has enough space to allocate the data
    Free_Memory_Block *affected_block;
    if(!rb_search(&alloc->tree, size, alignment, &affected_block)) {
        printf("Not enough memory\n");
        return 0;
    }
    size_t payload_offset = compute_payload_offset(affected_block->data.offset, alignment);

    size_t required_size = size + payload_offset;
    size_t rest = affected_block->data.size - required_size;

    //If there is a substantial amount of memory space left over, create a new free region from it
    if(rest >= MIN_FREE_BLOCK_SIZE) {
        Free_Memory_Block *new_free_block;
        if(!create_metadata(&alloc->meta_pool, VULKAN_FREE_BLOCK, (void **)&new_free_block)) return 0;
        new_free_block->data.size = rest;
        new_free_block->data.offset = affected_block->data.offset + required_size;
        dll_insert(new_free_block, affected_block);
        rb_insert(&alloc->tree, new_free_block);
    }
    //Otherwise just add it on to the end of the current one
    else {
        size += rest;
    }
    //Creating the block header and the pointer to start of memory
    if(!create_metadata(&alloc->meta_pool, VULKAN_ALLOCATED_BLOCK, (void **)out_alloc)) return 0;
    (*out_alloc)->memory = alloc->memory;
    (*out_alloc)->offset = affected_block->data.offset + payload_offset;
    (*out_alloc)->size = size;
    (*out_alloc)->padding = payload_offset;

    //Destroy the affected block
    destroy_free_block(alloc, affected_block);
    return 1;
}

//Coalesces two adjacent free regions together to form one large free region
void coalesce(Vulkan_Allocator *alloc, Free_Memory_Block *prev, Free_Memory_Block *new_block) {
    //If the next header isn't the sentinel value and the pointers are adjacnet, combine
    if(new_block->next != &alloc->list.header && new_block->data.offset + new_block->data.size == new_block->next->data.offset) {
        Free_Memory_Block *next = new_block->next;
        rb_remove(&alloc->tree, new_block);
        destroy_free_block(alloc, next);
        new_block->data.size += next->data.size;
        rb_insert(&alloc->tree, new_block);
    }

    //If the previous header isn't the sentinel value and the pointers are adjacnet, combine
    if(prev != &alloc->list.header && prev->data.offset + prev->data.size == new_block->data.offset) {
        rb_remove(&alloc->tree, prev);
        destroy_free_block(alloc, new_block);
        prev->data.size += new_block->data.size;
        rb_insert(&alloc->tree, prev);
    }
}

//Helper function to create a new free block at the given start address
//with the given size. Only blocks of size at least MIN_FREE_BLOCK_SIZE will be created
void create_new_free_block(Vulkan_Allocator *alloc, uintptr_t start, size_t size) {
    if(size < MIN_FREE_BLOCK_SIZE) return; //Early exit

    //Create the new block
    Free_Memory_Block *block;
    if(!create_metadata(&alloc->meta_pool, VULKAN_FREE_BLOCK, (void **)&block)) return;
    block->data.size = size;
    block->data.offset = start;
    block->next = NULL;
    block->prev = NULL;

    //Seach where to place it in the linked list
    Free_Memory_Block *b = &alloc->list.header;
    while(b->next != &alloc->list.header) {
        if(block->data.offset < b->next->data.offset) break;

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
void free_memory(Vulkan_Allocator *alloc, Vulkan_Allocation *ptr) {
    //Create a new free memory block to hold the freed region
    create_new_free_block(alloc, ptr->offset, ptr->size);
    destroy_metadata(&alloc->meta_pool, VULKAN_ALLOCATED_BLOCK, ptr);
}
