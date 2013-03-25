#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "allocator.h"


#define PAGE_SIZE 2048
#define PAGE_COUNT (SIZE / PAGE_SIZE)
#define PAGE_TABLE ((page_type **) memory)
#define FREE_TABLE_SIZE (log_2(PAGE_SIZE / 2) + 1)
#define FREE_TABLE (((page_type **) memory) + PAGE_COUNT)
#define BUSY ((page_type *) 1)
#define MIN_BLOCK_SIZE (sizeof(free_block_type))


static char memory[SIZE];


struct free_block_struct
{
    struct free_block_struct *next;
};


typedef struct free_block_struct free_block_type;


struct page_struct
{
    size_t block_size;
    free_block_type *free_block;
    int free_block_count;
    struct page_struct *next;
};


typedef struct page_struct page_type;


// System utils
page_type *create_page(size_t);
char *get_page_offset(int);
size_t round_to_4(size_t);
int log_2(int);


void mem_init()
{
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        PAGE_TABLE[i] = NULL;
    }

    // First page used for inner usage
    PAGE_TABLE[0] = (page_type *) BUSY;

    for (int i = 0; i < FREE_TABLE_SIZE; ++i)
    {
        FREE_TABLE[i] = NULL;
    }
}


void mem_dump()
{
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        printf("[%d]\t", i);
        if(PAGE_TABLE[i] == NULL)
        {
            printf("# free #\n");
        }
        else if(PAGE_TABLE[i] == BUSY)
        {
            printf("##\n");
        }
        else
        {
            page_type *page = PAGE_TABLE[i];
            int block_num = PAGE_SIZE / page->block_size;
            printf("# %zu | %d(%d) #\n", page->block_size,
                                         page->free_block_count,
                                         block_num);
        }
    }
    printf("\n");
}


void *mem_alloc(size_t size)
{
    int free_id = log_2((int) size);
    size_t real_size = pow(2, free_id);

    // size is to small, exiting
    if(real_size < MIN_BLOCK_SIZE)
    {
        return NULL;
    }

    page_type *page = *(FREE_TABLE + free_id);

    // There is no page with requested size, try to create it
    if(!page)
    {
        page = create_page(real_size);
        // TODO: if can't create!

        *(FREE_TABLE + free_id) = page;
    }

    free_block_type *free_block = page->free_block;
    page->free_block = free_block->next;
    page->free_block_count -= 1;

    if(page->free_block_count == 0)
    {
        *(FREE_TABLE + free_id) = page->next;
    }
    return (void *) free_block;
}


page_type *create_page(size_t size)
{
    // Try to find free page
    page_type *page;
    int page_num;
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        page = *(PAGE_TABLE + i);
        if(!page)
        {
            page_num = i;
            break;
        }
        // last one, there is no free pages, exiting
        if(i == PAGE_COUNT - 1)
        {
            return NULL;
        }
    }

    if(size > sizeof(page_type) && size < sizeof(page_type) * 10)
    {
        char *page_offset = get_page_offset(page_num);

        // First free block
        free_block_type *free_block = (free_block_type *)(page_offset + size);
        int free_block_count = PAGE_SIZE / size - 1;

        // Insert descriptor in first block on page
        page = (page_type *)page_offset;
        page->block_size = size;
        page->free_block = free_block;
        page->free_block_count = free_block_count;
        page->next = NULL;

        // Make list of free blocks
        for (int i = 0; i < free_block_count; ++i)
        {
            free_block->next = (free_block_type *)((char *)free_block + size);
            free_block = free_block->next;
        }
    }
    else
    {
        // Not Implemented! Error!
        printf("Not Implemented Error\n");
        exit(EXIT_FAILURE);
    }
    PAGE_TABLE[page_num] = page;
    return page;
}


char *get_page_offset(int page_num)
{
    return (char *)(memory + page_num * PAGE_SIZE);
}


size_t round_to_4(size_t size)
{
    return (size + 3) & ~3;
}


size_t round_size(size_t size)
{
    size_t res_size = 1;
    while (res_size < size) res_size <<= 1;
    return res_size;
}


int log_2(int val)
{
    int res = 1;
    int pred = 1;
    while((pred <<= 1) < val) res++;
    return res;
}
