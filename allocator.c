#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "allocator.h"


#define PAGE_SIZE 2048
#define PAGE_COUNT (MEM_SIZE / PAGE_SIZE)
#define PAGE_TABLE ((page_type **) memory)
#define FREE_TABLE_SIZE (log_2(PAGE_SIZE / 2) + 1)
#define FREE_TABLE (((page_type **) memory) + PAGE_COUNT)
#define BUSY ((page_type *) 1)
#define MIN_BLOCK_SIZE (sizeof(free_block_type))

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define max_block_count(page) ((int)(PAGE_SIZE / page->block_size))
#define is_big_block(page) (page->block_size >= PAGE_SIZE)
#define is_remote_descriptor(size) (size < sizeof(page_type) || size > sizeof(page_type) * 3)


static char memory[MEM_SIZE];


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
    struct page_struct *prev;
};


typedef struct page_struct page_type;


// system utils
page_type *create_page(size_t);
char *get_page_offset(int);
int get_page_num(void *);
void *mem_alloc_big(size_t);
void mem_free_big(void *);
size_t round_to_4(size_t);
int log_2(int);


void mem_init()
{
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        PAGE_TABLE[i] = NULL;
    }

    // first page used for inner usage
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
            printf("# %zu | %d(%d) #\n", page->block_size,
                                         page->free_block_count,
                                         max_block_count(page));
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

    if(real_size > PAGE_SIZE / 2)
    {
        return mem_alloc_big(real_size);
    }

    page_type *page = *(FREE_TABLE + free_id);

    // there is no page with requested size, try to create it
    if(!page)
    {
        page = create_page(real_size);
        if(!page)
        {
            return NULL;
        }

        *(FREE_TABLE + free_id) = page;
    }

    free_block_type *free_block = page->free_block;
    page->free_block = free_block->next;
    page->free_block_count--;

    if(page->free_block_count == 0)
    {
        *(FREE_TABLE + free_id) = page->next;
        page_type *next_page = page->next;
        if(next_page)
        {
            next_page->prev = NULL;
            page->next = NULL;
        }
    }
    return (void *) free_block;
}


void *mem_alloc_big(size_t size)
{
    int page_count = size / PAGE_SIZE;
    // search for big enough sequence of empty pages
    int page_num = 0;
    for (bool found = false; !found; ++page_num)
    {
        found = true;
        for (int j = 0; j < page_count; ++j)
        {
            if (*(PAGE_TABLE + page_num + j))
            {
                found = false;
                break;
            }
        }

        // Last one
        if(page_num >= (PAGE_COUNT - page_count))
        {
            if(!found)
            {
                return NULL;
            }
        }
    }
    // correcting found id
    page_num--;

    // Mark pages as busy
    for (int i = 0; i < page_count; ++i)
    {
        *(PAGE_TABLE + page_num + i) = BUSY;
    }

    page_type *page = mem_alloc(sizeof(page_type));
    page->block_size = size;
    page->free_block = NULL;
    page->free_block_count = 0;
    page->next = NULL;
    page->prev = NULL;
    *(PAGE_TABLE + page_num) = page;
    return (void *)get_page_offset(page_num);
}


void mem_free(void *addr)
{
    int page_num = get_page_num(addr);
    page_type *page = *(PAGE_TABLE + page_num);
    if(is_big_block(page))
    {
        mem_free_big(addr);
        return;
    }
    free_block_type *new_free_block = (free_block_type *) addr;
    new_free_block->next = NULL;

    // add new free block to the end of free blocks list
    free_block_type *free_block = page->free_block;
    if(free_block)
    {
        while(free_block->next)
        {
            free_block = free_block->next;
        }
        free_block->next = new_free_block;
    }
    else
    {
        page->free_block = new_free_block;
    }

    page->free_block_count++;

    int free_id = log_2((int) page->block_size);

    // case when page becomes empty
    if(page->free_block_count == max_block_count(page)
       || (!is_remote_descriptor(page->block_size)
           && page->free_block_count == (max_block_count(page) - 1)))
    {
        page_type *prev_page = page->prev;
        if(prev_page)
        {
            prev_page->next = page->next;
            page_type *next_page = page->next;
            if(next_page)
            {
                next_page->prev = prev_page;
            }
        }
        else
        {
            *(FREE_TABLE + free_id) = NULL;
        }
        *(PAGE_TABLE + page_num) = NULL;

        if(is_remote_descriptor(page->block_size))
        {
            mem_free((void *)page);
        }
        return;
    }

    // case when page must be added as one with free block
    if(page->free_block_count == 1)
    {
        page_type *free_page = *(FREE_TABLE + free_id);
        if(free_page)
        {
            // add current page to the and of the pages list
            while(free_page->next)
            {
                free_page = free_page->next;
            }
            free_page->next = page;
            page->prev = free_page;
            page->next = NULL;
        }
        else
        {
            *(FREE_TABLE + free_id) = page;
        }
    }
    return;
}


void mem_free_big(void *addr)
{
    int page_num = get_page_num(addr);
    page_type *page = *(PAGE_TABLE + page_num);
    int page_count = page->block_size / PAGE_SIZE;
    for (int i = 0; i < page_count; ++i)
    {
        *(PAGE_TABLE + page_num + i) = NULL;
    }
    mem_free((void *)page);
}


void *mem_realloc(void *addr, size_t size)
{
    void *new_addr = mem_alloc(size);
    if(new_addr)
    {
        page_type *page = *(PAGE_TABLE + get_page_num(addr));
        memcpy(new_addr, addr, MIN(page->block_size, size));
        mem_free(addr);
        return new_addr;
    }
    else
    {
        return NULL;
    }
}


page_type *create_page(size_t size)
{
    // try to find free page
    page_type *page;
    int page_num;
    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        page = *(PAGE_TABLE + i);
        if(!page)
        {
            page_num = i;
            PAGE_TABLE[page_num] = BUSY;
            break;
        }
        // last one, there is no free pages, exiting
        if(i == PAGE_COUNT - 1)
        {
            return NULL;
        }
    }

    char *page_offset = get_page_offset(page_num);
    // first free block
    free_block_type *free_block;
    int free_block_count;

    // case when descriptor lies in first block of page
    if(!is_remote_descriptor(size))
    {
        page = (page_type *)page_offset;
        free_block = (free_block_type *)(page_offset + size);
        free_block_count = PAGE_SIZE / size - 1;
    }
    // case when we must allocate memory for descriptor
    else
    {
        page = mem_alloc(sizeof(page_type));

        // no space for descriptor. Release page, exiting
        if(!page)
        {
            PAGE_TABLE[page_num] = NULL;
            return NULL;
        }

        free_block = (free_block_type *)page_offset;
        free_block_count = PAGE_SIZE / size;
    }

    page->block_size = size;
    page->free_block = free_block;
    page->free_block_count = free_block_count;
    page->next = NULL;
    page->prev = NULL;

    // make list of free blocks
    for (int i = 0; i < free_block_count - 1; ++i)
    {
        free_block->next = (free_block_type *)((char *)free_block + size);
        free_block = free_block->next;
    }
    free_block->next = NULL;
    PAGE_TABLE[page_num] = page;
    return page;
}


char *get_page_offset(int page_num)
{
    return (char *)(memory + page_num * PAGE_SIZE);
}


int get_page_num(void *addr)
{
    int offset = (char *)addr - memory;
    return offset / PAGE_SIZE;
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
