#include "../include/bench.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>

int main(int argc, char **argv) 
{
    std::ofstream file_time("../new_plot/times.txt");
    if (!file_time.is_open())
    {
        std::cerr << "Error: Could not open or create ../new_plot/times.txt\n";
        return 0;
    }

    for (int j = 0; j < amount_of_benchfiles; j++)
    {  
        std::ofstream file_rt((ALL_FILE_NAMES + j)->file_name);
        if (!file_rt.is_open())
        {
            std::cerr << "Error: file " << (ALL_FILE_NAMES + j)->file_name << " not found or cannot be created\n";
            return 0; 
        }
        
        file_rt << "r,h\n";

        const int MAX_REQUESTS = 500000;
        int amount_of_levels = (ALL_FILE_NAMES + j)->amount_of_levels;
        int cache_size = 0;
        for (int y = 0; y < amount_of_levels; y++)
            cache_size += ((ALL_FILE_NAMES + j)->config[y]).size;
        double total_time = 0;

        for (int i = 0; i < MAX_REQUESTS; i += 5000)
        {
            struct timespec start_time, fin_time;

            std::vector<long long> prms_vec = generate_zipf_vector(cache_size, i);

            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_time);

            long long hits = ZipfTest((ALL_FILE_NAMES + j)->config, i, prms_vec);

            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &fin_time);
            double elapsed = (fin_time.tv_sec - start_time.tv_sec) + 
                             (fin_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

            total_time += elapsed;

            std::cout << "cache size = " << cache_size << ", total_requests = " << i << ", hits = " << hits << "\n";
            file_rt << i << "," << hits << "\n";
        }

        std::cout << "total time = " << total_time << "\n";
        file_time << (ALL_FILE_NAMES + j)->file_name << ": total time = " << total_time << "\n";
    }
    
    return 0;
}




long long ZipfTest(levels_t* config, int total_requests, const std::vector<long long>& prms_vec) 
{    
    if (config[0].level_num == PERFECT_E) 
    {
        int cache_size = config[0].size;
        return OptZipfTest(cache_size, total_requests, prms_vec);
    }

    long long hits = 0;
    int error = RunLevelCache(config, prms_vec.data(), &hits);
    if (error < 0)
      return -1;
    return hits;
    
}

std::vector<long long> generate_zipf_vector(int cache_size, int total_requests, double alpha)
{
    std::vector<long long> prms_vec;
    prms_vec.reserve(total_requests + 2);
    
    prms_vec.push_back(cache_size);
    prms_vec.push_back(total_requests);

    int num_pages = cache_size * 5; 

    std::vector<double> sum_probs(num_pages);
    double current_sum = 0.0;
    
    for (int i = 0; i < num_pages; ++i)
    {
        // Zipf's law: the weight of an element is equal to 1 / (i + 1)^alpha
        current_sum += 1.0 / std::pow(i + 1, alpha);
        sum_probs[i] = current_sum;
    }

    std::mt19937 mersenne_engine(42);

    // make requests
    for (int i = 0; i < total_requests; ++i)
    {
        // get random num from 0.0 to current_sum
        double rand_val = (double)mersenne_engine() / mersenne_engine.max() * current_sum;

        int page_key = 1;
        for (int j = 0; j < num_pages; ++j)
        {
            if (rand_val <= sum_probs[j])
            {
                page_key = j + 1; 
                break;
            }
        }
        prms_vec.push_back(page_key);
    }

    return prms_vec;
}


long long OptZipfTest(int cache_size, int total_requests, const std::vector<long long>& prms_vec) 
{    
    if (prms_vec.size() < 2) return -1;

    int c_size = static_cast<int>(prms_vec[0]);
    int t_requests = static_cast<int>(prms_vec[1]);
    
    const long long* stream = prms_vec.data() + 2;

    long long hits = 0;
    if (c_size <= 0 || t_requests <= 0) return 0;

    std::unordered_map<long long, std::vector<std::size_t>> future_indices;
    for (int i = 0; i < t_requests; ++i) {
        future_indices[stream[i]].push_back(i);
    }
    
    // Reverse to allow O(1) removal of processed steps via pop_back()
    for (auto& [key, indices] : future_indices) {
        std::reverse(indices.begin(), indices.end());
    }

    // std::set stores {next_step, key} pairs and automatically sorts them by future distance
    // cache_hash stores iterators for instant lookup and removal from the set
    std::set<std::pair<std::size_t, long long>> cache_set;
    std::unordered_map<long long, decltype(cache_set)::iterator> cache_hash;

    // Constant for elements that will never be needed in the future (INF)
    const std::size_t INF = 1ULL << 60; 

    for (int i = 0; i < t_requests; ++i) {
        long long current_key = stream[i];

        if (cache_hash.count(current_key)) {
            hits++;
            
            auto it = cache_hash[current_key];
            cache_set.erase(it);
            
            if (!future_indices[current_key].empty()) {
                future_indices[current_key].pop_back();
            }
            
            std::size_t next_pos = future_indices[current_key].empty() ? INF : future_indices[current_key].back();
            cache_hash[current_key] = cache_set.insert({next_pos, current_key}).first;
            
            continue;
        }

        // if miss
        if (cache_hash.size() == static_cast<std::size_t>(c_size)) {
            auto victim_it = std::prev(cache_set.end());
            long long key_to_evict = victim_it->second;
            
            cache_set.erase(victim_it);
            cache_hash.erase(key_to_evict);
        }

        if (!future_indices[current_key].empty()) {
            future_indices[current_key].pop_back();
        }

        std::size_t next_pos = future_indices[current_key].empty() ? INF : future_indices[current_key].back();
        cache_hash[current_key] = cache_set.insert({next_pos, current_key}).first;
    }

    return hits;
}
