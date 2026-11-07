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

const int MAX_SIZE_OF_WORD = 5;

enum LEVELS {LFU_E, LIRS_E, ARC_E, TWOQ_E, PERFECT_E};

struct levels_cmp {
    char* name;
    LEVELS level_enum;
};

struct levels_t {
    LEVELS level_num;
    int size;
};

static struct levels_cmp levels_arr[] = {
    {(char*)"LFU", LFU_E},
    {(char*)"LIRS", LIRS_E},
    {(char*)"2Q", TWOQ_E},
    {(char*)"ARC", ARC_E}
};
static int amount_of_levels_cmp = sizeof(levels_arr) / sizeof(levels_arr[0]); 

using PageId = long long;
using Page = long long;

Page slow_get_page(PageId key);
bool read_integer(long long &value);
std::vector<long long> GetNums();
int RunFunc(long long *prms, long long *hits);
char* GetWordFromBuf(char* buf);
int GetSizeOfInputFile(const char* file_name);
std::vector<char> PutDataFromFileToBuffer(const char* input_file);
std::vector<levels_t> GetConfig(int cache_size);
int CountAmountOfDigits(int num);

struct Node { 
    long long key; 
    long long page; 
};

struct Res { 
    bool is_hit; 
    std::optional<Node> evicted; 
};

using CacheInstance = std::variant<
    cachelfu::cache_lfu_t<long long, long long>,
    cachelirs::cache_lirs_t<long long, long long>,
    cachearc::cache_arc_t<long long, long long>,
    cache2q::cache_2q_t<long long, long long>
>;

inline Res call_lookup(CacheInstance& cache, long long key, std::optional<long long> page) 
{
    return std::visit([key, page](auto& concrete_cache) -> Res {
        auto r = concrete_cache.lookup_update(key, page);
        return { r.is_hit, r.evicted ? std::optional<Node>({r.evicted->key, r.evicted->page}) : std::nullopt };
    }, cache);
}

inline Node call_erase(CacheInstance& cache, long long key) 
{
    return std::visit([key](auto& concrete_cache) -> Node {
        auto r = concrete_cache.erase_and_return(key);
        return { r.key, r.page };
    }, cache);
}

inline int RunLevelCache(levels_t* config, const long long* prms, long long* out_hits)
{
    if (!config || !prms || !out_hits) return 1;

    long long cache_size = prms[0];
    long long data_size = prms[1];
    const long long* requests = prms + 2;
    long long hits = 0;
    
    int amount_of_levels = 0;
    int current_sum_size = 0;
    
    while (current_sum_size < cache_size && amount_of_levels < 100) 
    {
        current_sum_size += config[amount_of_levels].size;
        amount_of_levels++;
    }
    
    std::vector<CacheInstance> all_my_caches;
    all_my_caches.reserve(amount_of_levels);

    for (int i = 0; i < amount_of_levels; ++i) 
    {
        size_t sz = static_cast<size_t>(config[i].size); 
        if (config[i].level_num == LFU_E)       
            all_my_caches.emplace_back(cachelfu::cache_lfu_t<long long, long long>(sz));
        else if (config[i].level_num == LIRS_E)  
            all_my_caches.emplace_back(cachelirs::cache_lirs_t<long long, long long>(sz));
        else if (config[i].level_num == ARC_E)   
            all_my_caches.emplace_back(cachearc::cache_arc_t<long long, long long>(sz));
        else if (config[i].level_num == TWOQ_E)  
            all_my_caches.emplace_back(cache2q::cache_2q_t<long long, long long>(sz));
    }

    for (long long i = 0; i < data_size; ++i) 
    {
        long long key = requests[i];
        int hit_level_index = -1;

        for (int lvl = 0; lvl < amount_of_levels; ++lvl) 
        {
            // if you want to check that there aren't double hits
            // std::cout<<i<<"hits "<<hits<<" key "<<key<<std::endl;
            if (call_lookup(all_my_caches[lvl], key, std::nullopt).is_hit) 
            {
                hit_level_index = lvl;
                hits++;
                break; 
            }
        }

        // cache miss at all levels
        if (hit_level_index == -1) 
        {
            Res res = call_lookup(all_my_caches[0], key, key);
            std::optional<Node> to_evict = res.evicted;
            int next_lvl = 1;

            while (to_evict.has_value() && next_lvl < amount_of_levels) 
            {
                Res sub_res = call_lookup(all_my_caches[next_lvl], to_evict->key, to_evict->page);
                to_evict = sub_res.evicted;
                next_lvl++;
            }
        }

        // elem in cache, but not at the highest level
        else if (hit_level_index > 0)
        {
            Node hit_node = call_erase(all_my_caches[hit_level_index], key);
            Res res = call_lookup(all_my_caches[0], hit_node.key, hit_node.page);

            std::optional<Node> to_evict = res.evicted;
            int next_lvl = 1;

            while (to_evict.has_value() && next_lvl <= hit_level_index) 
            {
                Res sub_res = call_lookup(all_my_caches[next_lvl], to_evict->key, to_evict->page);
                to_evict = sub_res.evicted;
                next_lvl++;
            }
        }

        // first cache level (the highest)
        else 
        {
            call_lookup(all_my_caches[0], key, key);
        }
    }

    *out_hits = hits;
    return 0; 
}