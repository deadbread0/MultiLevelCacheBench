#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/stat.h>
#include <assert.h>

#include "cacheLFU.hpp"
#include "cacheLIRS.hpp"
#include "cacheARC.hpp"
#include "cache2Q.hpp"

#if defined(LFU_TEST) || defined(LFU) 
#define caches cachelfu
#define cache_t cache_lfu_t 
#endif

#if defined(LIRS_TEST) || defined(LIRS)
#define caches cachelirs
#define cache_t cache_lirs_t
#endif

#if defined(ARC_TEST) || defined(ARC)
#define caches cachearc
#define cache_t cache_arc_t
#endif

#if defined(TWO_Q_TEST) || defined(TWO_Q)
#define caches cache2q
#define cache_t cache_2q_t
#endif

const int MAX_SIZE_OF_WORD = 4;

enum LEVELS {LFU_E, LIRS_E, ARC_E, TWOQ_E};

struct levels_cmp {
    char* name;
    LEVELS level_enum;
};

struct levels_t {
    LEVELS level_num;
    int size;
};

static struct levels_cmp levels_arr[] = {
    {"LFU", LFU_E},
    {"LIRS", LIRS_E},
    {"2Q", TWOQ_E},
    {"ARC", ARC_E}
};
static int amount_of_levels_cmp = sizeof(levels_arr) / sizeof(levels_arr[0]); 

using PageId = long long;
using Page = long long;

Page slow_get_page(PageId key);
bool read_integer(long long &value);
long long *GetNums();
int RunFunc(long long *prms, long long *hits);
char* GetWordFromBuf(char* buf);
int GetSizeOfInputFile(const char* file_name);
char* PutDataFromFileToBuffer(const char* input_file);
levels_t* GetConfig(int cache_size);
int CountAmountOfDigits(int num);


struct Node { 
    long long key; 
    long long page; 
};

struct Res { 
    bool is_hit; 
    std::optional<Node> evicted; 
};

// call func for: 
// 1) move node from one level to another (if r.evicted)
// 2) find request (if !r.evicted)
inline Res call_lookup(void* ptr, LEVELS type, long long key, std::optional<long long> page) 
{
    if (type == LFU_E) {
        auto r = static_cast<cachelfu::cache_lfu_t<long long, long long>*>(ptr)->lookup_update(key, page);
        return { r.is_hit, r.evicted ? std::optional<Node>({r.evicted->key, r.evicted->page}) : std::nullopt };
    }
    if (type == LIRS_E) {
        auto r = static_cast<cachelirs::cache_lirs_t<long long, long long>*>(ptr)->lookup_update(key, page);
        return { r.is_hit, r.evicted ? std::optional<Node>({r.evicted->key, r.evicted->page}) : std::nullopt };
    }
    if (type == ARC_E) {
        auto r = static_cast<cachearc::cache_arc_t<long long, long long>*>(ptr)->lookup_update(key, page);
        return { r.is_hit, r.evicted ? std::optional<Node>({r.evicted->key, r.evicted->page}) : std::nullopt };
    }
    {
        auto r = static_cast<cache2q::cache_2q_t<long long, long long>*>(ptr)->lookup_update(key, page);
        return { r.is_hit, r.evicted ? std::optional<Node>({r.evicted->key, r.evicted->page}) : std::nullopt };
    }
}

inline Node call_erase(void* ptr, LEVELS type, long long key) {
    if (type == LFU_E) { 
        auto r = static_cast<cachelfu::cache_lfu_t<long long, long long>*>(ptr)->erase_and_return(key); 
        return {r.key, r.page}; 
    }
    if (type == LIRS_E) { 
        auto r = static_cast<cachelirs::cache_lirs_t<long long, long long>*>(ptr)->erase_and_return(key); 
        return {r.key, r.page}; 
    }
    if (type == ARC_E) { 
        auto r = static_cast<cachearc::cache_arc_t<long long, long long>*>(ptr)->erase_and_return(key); 
        return {r.key, r.page}; 
    }
    { 
        auto r = static_cast<cache2q::cache_2q_t<long long, long long>*>(ptr)->erase_and_return(key); 
        return {r.key, r.page}; 
    }
}

inline int RunLevelCache(levels_t* config, long long* prms, long long* out_hits)
{
    if (!config || !prms || !out_hits) return 1;

    long long cache_size = prms[0];
    long long data_size = prms[1];
    long long* requests = prms + 2;
    long long hits = 0;
    
    int amount_of_levels = 0;
    int current_sum_size = 0;
    while (current_sum_size < cache_size) 
    {
        current_sum_size += config[amount_of_levels].size;
        amount_of_levels++;
    }
    
    void** all_my_caches = (void**)calloc(amount_of_levels, sizeof(void*));
    for (int i = 0; i < amount_of_levels; ++i) 
    {
        size_t sz = static_cast<size_t>(config[i].size); 
        if (config[i].level_num == LFU_E)       all_my_caches[i] = new cachelfu::cache_lfu_t<long long, long long>(sz);
        else if (config[i].level_num == LIRS_E)  all_my_caches[i] = new cachelirs::cache_lirs_t<long long, long long>(sz);
        else if (config[i].level_num == ARC_E)   all_my_caches[i] = new cachearc::cache_arc_t<long long, long long>(sz);
        else if (config[i].level_num == TWOQ_E)  all_my_caches[i] = new cache2q::cache_2q_t<long long, long long>(sz);
    }

    for (long long i = 0; i < data_size; ++i) 
    {
        long long key = requests[i];
        int hit_level_index = -1;

        for (int lvl = 0; lvl < amount_of_levels; ++lvl) 
        {
            if (call_lookup(all_my_caches[lvl], config[lvl].level_num, key, std::nullopt).is_hit) 
            {
                hit_level_index = lvl;
                hits++;
                break; 
            }
        }

        // cache miss at all levels
        if (hit_level_index == -1) 
        {
            Res res = call_lookup(all_my_caches[0], config[0].level_num, key, key);
            std::optional<Node> to_evict = res.evicted;
            int next_lvl = 1;

            while (to_evict.has_value() && next_lvl < amount_of_levels) 
            {
                Res sub_res = call_lookup(all_my_caches[next_lvl], config[next_lvl].level_num, to_evict->key, to_evict->page);
                to_evict = sub_res.evicted;
                next_lvl++;
            }
        }

        // elem in cache, but not at the highest level
        else if (hit_level_index > 0)
        {
            Node hit_node = call_erase(all_my_caches[hit_level_index], config[hit_level_index].level_num, key);
            Res res = call_lookup(all_my_caches[0], config[0].level_num, hit_node.key, hit_node.page);

            std::optional<Node> to_evict = res.evicted;
            int next_lvl = 1;

            while (to_evict.has_value() && next_lvl <= hit_level_index) 
            {
                Res sub_res = call_lookup(all_my_caches[next_lvl], config[next_lvl].level_num, to_evict->key, to_evict->page);
                to_evict = sub_res.evicted;
                next_lvl++;
            }
        }

        // first cache level (the highest)
        else 
        {
            call_lookup(all_my_caches[0], config[0].level_num, key, key);
        }
    }

    for (int i = 0; i < amount_of_levels; ++i) 
    {
        if (config[i].level_num == LFU_E) delete static_cast<cachelfu::cache_lfu_t<long long, long long>*>(all_my_caches[i]);
        else if (config[i].level_num == LIRS_E) delete static_cast<cachelirs::cache_lirs_t<long long, long long>*>(all_my_caches[i]);
        else if (config[i].level_num == ARC_E) delete static_cast<cachearc::cache_arc_t<long long, long long>*>(all_my_caches[i]);
        else if (config[i].level_num == TWOQ_E) delete static_cast<cache2q::cache_2q_t<long long, long long>*>(all_my_caches[i]);
    }
    free(all_my_caches);

    *out_hits = hits;
    return 0; 
}
