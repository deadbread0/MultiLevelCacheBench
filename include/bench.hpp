#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <random>
#include <cmath>
#include <set>
#include <unordered_map>

#include "../include/all_caches.hpp"

levels_t config_2q[]   = { {TWOQ_E, 500} };
levels_t config_lfu[]  = { {LFU_E, 500} };
levels_t config_lirs[] = { {LIRS_E, 500} };
levels_t config_arc[]  = { {ARC_E, 500} };
levels_t config_perfect[]  = { {PERFECT_E, 500} };

levels_t config_2q5000[]   = { {TWOQ_E, 5000} };
levels_t config_lfu5000[]  = { {LFU_E, 5000} };
levels_t config_lirs5000[] = { {LIRS_E, 5000} };
levels_t config_arc5000[]  = { {ARC_E, 5000} };
levels_t config_perfect5000[]  = { {PERFECT_E, 5000} };

levels_t config_2q_2q[]   = { {TWOQ_E, 250}, {TWOQ_E, 250} };
levels_t config_lfu_lfu[]  = { {LFU_E, 250}, {LFU_E, 250} };
levels_t config_lirs_lirs[] = { {LIRS_E, 250}, {LIRS_E, 250} };
levels_t config_arc_arc[]  = { {ARC_E, 250}, {ARC_E, 250} };

levels_t config_2q_lfu[]   = { {TWOQ_E, 250}, {LFU_E, 250} };
levels_t config_lfu_2q[]  = { {LFU_E, 250}, {TWOQ_E, 250} };

levels_t config_2q_arc[]   = { {TWOQ_E, 250}, {ARC_E, 250} };
levels_t config_arc_2q[]  = { {ARC_E, 250}, {TWOQ_E, 250} };

levels_t config_2q_lirs[]   = { {TWOQ_E, 250}, {LIRS_E, 250} };
levels_t config_lirs_2q[]  = { {LIRS_E, 250}, {TWOQ_E, 250} };

levels_t config_lirs_lfu[]   = { {LIRS_E, 250}, {LFU_E, 250} };
levels_t config_lfu_lirs[]  = { {LFU_E, 250}, {LIRS_E, 250} };

levels_t config_arc_lfu[]   = { {ARC_E, 250}, {LFU_E, 250} };
levels_t config_lfu_arc[]  = { {LFU_E, 250}, {ARC_E, 250} };

levels_t config_arc_lirs[]   = { {ARC_E, 250}, {LIRS_E, 250} };
levels_t config_lirs_arc[]  = { {LIRS_E, 250}, {ARC_E, 250} };

struct bench_t {
    const char* file_name;
    levels_t* config;
    int amount_of_levels;
};

const bench_t ALL_FILE_NAMES[] = {
    // {"../new_plot/data_r(h)_2q500.csv", config_2q, 1},   
    {"../new_plot/data_r(h)_lfu500.csv", config_lfu, 1},
    // {"../new_plot/data_r(h)_lirs500.csv", config_lirs, 1},
    // {"../new_plot/data_r(h)_arc500.csv", config_arc, 1},
    // {"../new_plot/data_r(h)_perfect500.csv", config_perfect, 1},

    // {"../new_plot/data_r(h)_2q5000.csv", config_2q5000, 1},   
    // {"../new_plot/data_r(h)_lfu5000.csv", config_lfu5000, 1},
    // {"../new_plot/data_r(h)_lirs5000.csv", config_lirs5000, 1},
    // {"../new_plot/data_r(h)_arc5000.csv", config_arc5000, 1},
    // {"../new_plot/data_r(h)_perfect5000.csv", config_perfect5000, 1},

    // {"../new_plot/data_r(h)_2q_2q500.csv", config_2q_2q, 2},   
    // {"../new_plot/data_r(h)_lfu_lfu500.csv", config_lfu_lfu, 2},
    // {"../new_plot/data_r(h)_lirs_lirs500.csv", config_lirs_lirs, 2},
    // {"../new_plot/data_r(h)_arc_arc500.csv", config_arc_arc, 2},

    // {"../new_plot/data_r(h)_2q_lfu500.csv", config_2q_lfu, 2},   
    // {"../new_plot/data_r(h)_lfu_2q500.csv", config_lfu_2q, 2},

    // {"../new_plot/data_r(h)_2q_arc500.csv", config_2q_arc, 2},   
    // {"../new_plot/data_r(h)_arc_2q500.csv", config_arc_2q, 2},

    // {"../new_plot/data_r(h)_2q_lirs500.csv", config_2q_lirs, 2},   
    // {"../new_plot/data_r(h)_lirs_2q500.csv", config_lirs_2q, 2},

    // {"../new_plot/data_r(h)_arc_lfu500.csv", config_arc_lfu, 2},   
    // {"../new_plot/data_r(h)_lfu_arc500.csv", config_lfu_arc, 2},

    // {"../new_plot/data_r(h)_arc_lirs500.csv", config_arc_lirs, 2},   
    // {"../new_plot/data_r(h)_lirs_arc500.csv", config_lirs_arc, 2},

    // {"../new_plot/data_r(h)_lirs_lfu500.csv", config_lirs_lfu, 2},   
    // {"../new_plot/data_r(h)_lfu_lirs500.csv", config_lfu_lirs, 2}
};

const int amount_of_benchfiles = sizeof(ALL_FILE_NAMES) / sizeof(ALL_FILE_NAMES[0]);

long long ZipfTest(levels_t* config, int total_requests, const std::vector<long long>& prms_vec);
std::vector<long long> generate_zipf_vector(int cache_size, int total_requests, double alpha = 0.95);
long long OptZipfTest(int cache_size, int total_requests, const std::vector<long long>& prms_vec);