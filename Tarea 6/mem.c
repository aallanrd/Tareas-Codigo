/* Copyright (c) 2009 Marwan Burelle, MIT; see COPYRIGHT */
#include "mem.h"

#define MIN_SIZE __SIZEOF_POINTER__
#define BLOCK_SIZE __SIZEOF_POINTER__*5
#define OFFSET MIN_SIZE - 1 + sizeof(void*)
#define align(x) ((size_t)(x) + OFFSET) & ~(MIN_SIZE - 1)

void *base = NULL;

t_block extend_heap(t_block last, size_t size);
t_block find_block(t_block last, size_t size);
t_block get_block(void *p);
void split_block(t_block b, size_t size);
void copy_block(t_block src, t_block dst);

void meminfo(void) {
    t_block b = base;
	printf("MemInfo\n");
    while (b){
		printf("Mem Size:%d\n",b->size);
        b = b->next;
    }
}

t_block find_block(t_block last, size_t size){
    t_block b = base;
    while (b && !(b->free && b->size >= size)){
        last = b;
        b = b->next;
    }
    return b;
}

t_block extend_heap(t_block last, size_t s){
    int sb;
    t_block b;
    b = sbrk(0);
    sb = (int)sbrk(BLOCK_SIZE + s);
    if(sb < 0) return NULL;
    
    b->size = s;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    if(last) last->next = b;
    b->free = 0;
    return b;
}

void split_block(t_block b, size_t s){
    t_block new;
    new = (t_block)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    new->free = 1;
    new->ptr = new->data;
    b->size = s;
    b->next = new;
    if(new->next) 
		new->next->prev = new;
}

void *malloc(size_t size) {
    t_block b, last;
    size_t s;
    s = align(size);
    if(base){
        last = base;
        b = find_block(last, s);
        if(b){
            if((b->size - s) >= (BLOCK_SIZE + OFFSET)) 
				split_block(b, s);
            b->free = 0;
        } else {
            b = extend_heap(last, s);
            if(!b) return NULL;
        }
    } else {
        b = extend_heap(NULL, s);
        if (!b) return NULL;
        base = b;
    }
    return (b->data);
}

void *calloc(size_t number, size_t size){
    size_t *new;
    size_t s, i;
    new = malloc(number * size);
    if (new){
        s = align(number * size) << 3;
        for (i = 0; i < s; i++) new[i] = 0;
    }
    return new;
}

t_block fusion(t_block b){
    if(b->next && b->next->free){
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) 
			b->next->prev = b;
    }
    return (b);
}

t_block get_block(void *p){
    char *tmp;
    tmp = p;
    return (p = tmp -= BLOCK_SIZE);
}

int valid_addr(void *p){
    if (base) {
        if (p > base && p < sbrk(0)) {
			return (p == (get_block(p))->ptr);
		}
    }
    return (0);
}

void free(void *p){
    t_block b;
    if (valid_addr(p)){
        b = get_block(p);
        b->free = 1;
        if(b->prev && b->prev->free) 
			b = fusion(b->prev);
        if(b->next){
            fusion(b);
        }
        else{
            if(b->prev) 
				b->prev->next = NULL;
            else 
				base = NULL;
            brk(b);
        }
    }
}

void copy_block(t_block src, t_block dst){
    int *sdata, *ddata;
    size_t i;
    sdata = src->ptr;
    ddata = dst->ptr;
    for(i = 0; i * MIN_SIZE < src->size && i * MIN_SIZE < dst->size; i++)
        ddata[i] = sdata[i];
}


void *realloc(void *p, size_t size){
    size_t s;
    t_block b, new;
    void *newp;
    if(!p) return (malloc(size));
    if (valid_addr(p)){
        s = align(size);
        b = get_block(p);
        if (b->size >= s){
            if(b->size - s >= (BLOCK_SIZE + OFFSET)) split_block(b, s);
        }
        else{
            if(b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s){
                fusion(b);
                if(b->size - s >= (BLOCK_SIZE + OFFSET)) split_block(b, s);
            }
            else{
                newp = malloc(s);
                if (!newp) return NULL;
                new = get_block(newp);
                copy_block(b, new);
                free(p);
                return newp;
            }
        }
        return p;
    }
    return NULL;
}


void *reallocf(void *p, size_t size){
    void *newp;
    newp = realloc(p, size);
    if (!newp) free(p);
    return newp;
}
