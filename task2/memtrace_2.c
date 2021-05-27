#include<execinfo.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<malloc.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>
// #include <addrList.h>

// LD_PRELOAD=./libmem.so ./case1
// gcc memtrace_2.c -g -fPIC -shared -rdynamic -o libmem.so -Wl,-Map,mem.map
// g++ -g -rdynamic -o target task2/case1.cpp -Wl,-Map,target.map
// g++ -g -rdynamic -o case1 case1.cpp -Wl,-Map,case1.map

static void memory_trace_init(void);
static void memory_trace_deinit(void);
static void *my_malloc_hook (size_t size, const void* ptr);
static void  my_free_hook (void* ptr, const void* caller);
static void *my_realloc_hook (void *ptr, size_t size, const void *caller);

static void *my_malloc_hook (size_t size, const void* ptr);
static void  my_free_hook (void* ptr, const void* caller);
static void *my_realloc_hook (void *ptr, size_t size, const void *caller);

static void *(*old_malloc_hook)(size_t size, const void* ptr);
static void  (*old_free_hook)(void* ptr, const void* caller);
static void *(*old_realloc_hook)(void *ptr, size_t size, const void *caller);

#define BACK_TRACE_DEPTH 8
#define CACHE_SIZE       512
#define FLUSH_SIZE       256

typedef struct node{
	void* addr;
	struct node *next;
} LinkList;

void *insert(void* address, LinkList *head){
	LinkList *node, *nex;
    node = (LinkList*)malloc(sizeof(LinkList));
    node->next = head->next;
    node->addr = address;
    head->next = node;
}
void *findInsert(void* address, LinkList *head){
    LinkList *tmp = head;
    while(tmp->next != NULL){
        if(tmp->next->addr == address){
            return;
        }
        tmp = tmp->next;
    }
    insert(address, head);
}

int findDelete(void* address, LinkList *head){
    LinkList *tmp = head;
    while(tmp->next != NULL){
        if(tmp->next->addr == address){
            tmp->next = tmp->next->next;
            return 1; 
        }
        tmp = tmp->next;
    }
    return 0;
}


void *printList(LinkList *head){
    LinkList *tmp = head;
    while(tmp->next != NULL){
        printf("%p ", tmp->next->addr);
        tmp = tmp->next;
    }
}


static long leak_time_threshold = 2000;
static int print_enable = 0;
static int withoutMe = 0;

static LinkList *head;

// typedef map<long, long> VOID_LONG_MAP;
// typedef map<*void, int> VOID_INT_MAP;

// static map<*void, long> addr_to_time;
// static map<*void, int> addr_to_status;

static FILE* g_memory_trace_fp = NULL;
static int   g_memory_trace_cache_used = 0;
static int   g_memory_trace_cache_start = 0;
static int   g_memory_trace_last_flush = 0;
/*additional 3 items: alloc/free addr size*/
static void* g_memory_trace_cache[CACHE_SIZE][BACK_TRACE_DEPTH + 4]; 
static void  memory_trace_flush(void);
static void  memory_trace_write(int alloc, void* addr, int size, long tp);


static void memory_trace_backup(void)
{
    old_malloc_hook  = __malloc_hook;
    old_free_hook    = __free_hook;
    old_realloc_hook = __realloc_hook;

    return;
}

static void memory_trace_hook(void)
{
    __malloc_hook  = my_malloc_hook;
    __free_hook    = my_free_hook;
    __realloc_hook = my_realloc_hook;

    return;
}

static void memory_trace_restore(void)
{
    __malloc_hook  = old_malloc_hook;
    __free_hook    = old_free_hook;
    __realloc_hook = old_realloc_hook;

    return;
}

static void memory_trace_init(void)
{
    if(g_memory_trace_fp == NULL)
    {
        char file_name[260] = {0};
        snprintf(file_name, sizeof(file_name), "./%d_memory.log", getpid());
        if((g_memory_trace_fp = fopen(file_name, "wb+")) != NULL)
        {

            memory_trace_backup();
            // init link head
            head = (LinkList*)malloc(sizeof(LinkList));
            head->next = NULL;
            memory_trace_hook();
        }

        atexit(memory_trace_deinit);
    }

    return;
}

static void memory_trace_deinit(void)
{
    if(g_memory_trace_fp != NULL)
    {
        memory_trace_restore();
        memory_trace_flush();
        g_memory_trace_last_flush = 1;
        memory_trace_flush();
        if(head->next != NULL){
            printf("\n---------------------------------------------------------------------------------\n ");
            printf("Memoryleak truly occurs at these addresses: ");
            printList(head);
            printf("\n---------------------------------------------------------------------------------\n ");
        }
        fclose(g_memory_trace_fp);
        g_memory_trace_fp = NULL;
    }

    return;
}

void (*__malloc_initialize_hook) (void) = memory_trace_init;

static void * my_malloc_hook (size_t size, const void *caller)
{
    void *result = NULL;
    memory_trace_restore(); 
    long tp = (long)clock(); 
    result = malloc (size);
    if(print_enable == 1){
        printf("allocate: %p, size = %d, at time: %d clocks\n", result, size, tp);
    }
    memory_trace_write(1, result, size, tp);
    memory_trace_hook();
    
    return result;
}

static void my_free_hook (void *ptr, const void *caller)
{
    memory_trace_restore();
    long tp = (long)clock(); 
    if(print_enable == 1){
        printf("free: %p, at time: %d clocks\n", ptr, tp);
    }
    free (ptr);
    memory_trace_write(0, ptr, 0, tp);
    memory_trace_hook();

    return;
}

static void *my_realloc_hook (void *ptr, size_t size, const void *caller)
{
    void* result = NULL;
    memory_trace_restore();
    if(print_enable == 1){
        printf("free and reallocte: %p size = %d\n", ptr, size);
    }
    memory_trace_write(0, ptr, 0 , (long)clock());
    result = realloc(ptr, size);
    memory_trace_write(1, result, size, (long)clock());
    memory_trace_hook();

    return result;
}

static void memory_trace_flush_one_entry(int index)
{
    int offset = 0;
    char buffer[512] = {0};
    int fd = fileno(g_memory_trace_fp);
    int alloc  = (int)g_memory_trace_cache[index][BACK_TRACE_DEPTH]; 
    void* addr = g_memory_trace_cache[index][BACK_TRACE_DEPTH+1]; 
    int size   = (int)g_memory_trace_cache[index][BACK_TRACE_DEPTH+2];
    long tp  = (long)g_memory_trace_cache[index][BACK_TRACE_DEPTH+3];
    void** backtrace_buffer = g_memory_trace_cache[index];

    snprintf(buffer, sizeof(buffer), "%s %p %d %d \n", alloc ? "alloc":"free", addr, size, tp);
    if(tp!=0)
    {
        write(fd, buffer, strlen(buffer));
        return;
    }
    
    char** symbols = backtrace_symbols(backtrace_buffer, BACK_TRACE_DEPTH);
    if(symbols != NULL && tp != 0)
    {
        int i = 0;
        offset = strlen(buffer);
        // for (int j = 0; j < BACK_TRACE_DEPTH; j++){
        //     printf(" %s ", symbols[j]);  
        // }  
        // printf("\n");
        for(i = 0; i < BACK_TRACE_DEPTH; i++)
        {
            if(symbols[i] == NULL)
            {
                break;
            };
            
            // strncpy(buffer+offset, symbols[i], sizeof(buffer)-offset);
            char* begin = strchr(symbols[i], '(');
            if(begin != NULL)
            {
                *begin = ' ';
                char* end = strchr(begin, ')');
                if(end != NULL)
                {
                    strcpy(end, " ");
                }
                strncpy(buffer+offset, begin, sizeof(buffer)-offset);
                offset += strlen(begin);
            }
        }
        write(fd, buffer, offset);
        free(symbols);
    }

    return;
}

static void memory_leak_check(void){
    int check_size = CACHE_SIZE;
    if(g_memory_trace_last_flush == 0){
        check_size = FLUSH_SIZE;
    }
    for(int i = 0; i < FLUSH_SIZE; i++)
    {
        int cur = (g_memory_trace_cache_start + i) % CACHE_SIZE;
        int check = 1;
        int nex = 0;
        // check this memory is freed or not, try to free it
        if((int)g_memory_trace_cache[cur][BACK_TRACE_DEPTH] == 0){
            findDelete(g_memory_trace_cache[cur][BACK_TRACE_DEPTH + 1], head);
            continue;
        }
        for(int j = 1; j < check_size; j++){
            nex = (j + cur) % CACHE_SIZE;
            // current address = next address, update status
            if(g_memory_trace_cache[nex][BACK_TRACE_DEPTH+1] == g_memory_trace_cache[cur][BACK_TRACE_DEPTH+1]){
                // printf("%p and ",g_memory_trace_cache[cur][BACK_TRACE_DEPTH+1]);
                // printf("%p\n",g_memory_trace_cache[nex][BACK_TRACE_DEPTH+1]);
                // printf("check occurs \n");
                /* if time between two accesses is longer than 'leak_time_threshold', assert leak occurs, check = 1 and break; */
                if((long)g_memory_trace_cache[nex][BACK_TRACE_DEPTH+3] - (long)g_memory_trace_cache[cur][BACK_TRACE_DEPTH+3] >= leak_time_threshold){
                    check = 2;
                    break;
                }
                else{
                    check = 0;
                    break;
                }
                // printf("check finishes \n");
            }
        }
        if(check > 0){
            // Show backtrace information of leak part
            if(check == 1){
                printf("\n---------------------------------------------------------------------------------\n ");
                printf("The target memory isn't freed\n ");
                findInsert(g_memory_trace_cache[cur][BACK_TRACE_DEPTH + 1],head);
            }
            else if (check == 2)
            {
                printf("Time limit exceed: %d clocks between 2 accesses.\n", (long)g_memory_trace_cache[nex][BACK_TRACE_DEPTH+3] - (long)g_memory_trace_cache[cur][BACK_TRACE_DEPTH+3]);
            }
            
            printf("mem leak may occurs at: %p size = %d\nBacktrace information are shown here:",
             g_memory_trace_cache[cur][BACK_TRACE_DEPTH + 1], (int)g_memory_trace_cache[cur][BACK_TRACE_DEPTH + 2]);
            void** backtrace_buffer = g_memory_trace_cache[cur];
            char** symbols = backtrace_symbols(backtrace_buffer, BACK_TRACE_DEPTH);
            for (int j = 0; j < BACK_TRACE_DEPTH; j++)
            {
            printf(" %s ", symbols[j]);  
            }  
            printf("\n");
        }
    }
}

static void memory_trace_flush(void)
{
    int fd = fileno(g_memory_trace_fp);
    int i = 0;
    memory_leak_check();
    for(i = g_memory_trace_cache_start; i < FLUSH_SIZE + g_memory_trace_cache_start; i++)
    {
        memory_trace_flush_one_entry(i);
    }
    g_memory_trace_cache_used = g_memory_trace_cache_used - FLUSH_SIZE;
    if(g_memory_trace_cache_start > 0){
        g_memory_trace_cache_start = 0;
    }
    else{
        g_memory_trace_cache_start = FLUSH_SIZE;
    }
    return;
}

static void memory_trace_write(int alloc, void* addr, int size, long tp)
{
    if(withoutMe == 0){
        withoutMe = 1;
        return;
    }
    if(g_memory_trace_cache_used >= CACHE_SIZE)
    {
        memory_trace_flush();
    }

    int i = 0;
    void* backtrace_buffer[BACK_TRACE_DEPTH] = {0};
    backtrace(backtrace_buffer, BACK_TRACE_DEPTH);

    for(i = 0; i < BACK_TRACE_DEPTH; i++)
    {
        g_memory_trace_cache[(g_memory_trace_cache_used + g_memory_trace_cache_start) % CACHE_SIZE][i] = backtrace_buffer[i];
    }
    g_memory_trace_cache[(g_memory_trace_cache_used + g_memory_trace_cache_start) % CACHE_SIZE][BACK_TRACE_DEPTH] = (void*)alloc;
    g_memory_trace_cache[(g_memory_trace_cache_used + g_memory_trace_cache_start) % CACHE_SIZE][BACK_TRACE_DEPTH+1] = addr;
    g_memory_trace_cache[(g_memory_trace_cache_used + g_memory_trace_cache_start) % CACHE_SIZE][BACK_TRACE_DEPTH+2] = (void*)size;
    g_memory_trace_cache[(g_memory_trace_cache_used + g_memory_trace_cache_start) % CACHE_SIZE][BACK_TRACE_DEPTH+3] = (void*)tp;

    g_memory_trace_cache_used++;

    return;
}

