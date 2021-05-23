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
#define CACHE_SIZE       10

// static clock_t start_time;
static FILE* g_memory_trace_fp = NULL;
static int   g_memory_trace_cache_used = 0;
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
    printf("allocate: %p, size = %d, at time: %d clocks\n", result, size, tp);
    memory_trace_write(1, result, size, tp);
    memory_trace_hook();
    
    return result;
}

static void my_free_hook (void *ptr, const void *caller)
{
    memory_trace_restore();
    long tp = (long)clock(); 
    printf("free: %p, at time: %d clocks\n", ptr, tp);
    free (ptr);
    memory_trace_write(0, ptr, 0, tp);
    memory_trace_hook();

    return;
}

static void *my_realloc_hook (void *ptr, size_t size, const void *caller)
{
    void* result = NULL;
    memory_trace_restore();
    printf("free and reallocte: %p size = %d\n", ptr, size);
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

    snprintf(buffer, sizeof(buffer), "%s %p %d %d ", alloc ? "alloc":"free", addr, size, tp);
    if(!alloc)
    {
        write(fd, buffer, strlen(buffer));
        return;
    }
    
    char** symbols = backtrace_symbols(backtrace_buffer, BACK_TRACE_DEPTH);
    if(symbols != NULL)
    {
        int i = 0;
        offset = strlen(buffer);
        for (int j = 0; j < BACK_TRACE_DEPTH; j++){
            printf(" %s ", symbols[j]);  
        }  
        printf("\n");
        for(i = 0; i < BACK_TRACE_DEPTH; i++)
        {
            if(symbols[i] == NULL)
            {
                break;
            }
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

static void memory_trace_flush(void)
{
    char buff[64] = {0x00};  
    sprintf(buff,"cat /proc/%d/maps", getpid());    
    int fd = fileno(g_memory_trace_fp);
    // system((const char*) buff);  
    write(fd, buff, strlen(buff));
    int i = 0;
    for(i = 0; i < g_memory_trace_cache_used; i++)
    {
        memory_trace_flush_one_entry(i);
    }
    g_memory_trace_cache_used = 0;

    return;
}

static void memory_trace_write(int alloc, void* addr, int size, long tp)
{
    if(g_memory_trace_cache_used >= CACHE_SIZE)
    {
        memory_trace_flush();
    }

    int i = 0;
    void* backtrace_buffer[BACK_TRACE_DEPTH] = {0};
    backtrace(backtrace_buffer, BACK_TRACE_DEPTH);

    for(i = 0; i < BACK_TRACE_DEPTH; i++)
    {
        g_memory_trace_cache[g_memory_trace_cache_used][i] = backtrace_buffer[i];
    }
    g_memory_trace_cache[g_memory_trace_cache_used][BACK_TRACE_DEPTH] = (void*)alloc;
    g_memory_trace_cache[g_memory_trace_cache_used][BACK_TRACE_DEPTH+1] = addr;
    g_memory_trace_cache[g_memory_trace_cache_used][BACK_TRACE_DEPTH+2] = (void*)size;
    g_memory_trace_cache[g_memory_trace_cache_used][BACK_TRACE_DEPTH+3] = (void*)tp;

    g_memory_trace_cache_used++;

    return;
}

