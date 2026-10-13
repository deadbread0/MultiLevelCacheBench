#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <optional>

namespace cachelfu {

template <typename T, typename KeyT = int> struct cache_lfu_t 
{
  std::size_t sz_;
  
  struct cache_node_t 
  {
    KeyT key;
    T page;
    int counter; 
  };

  std::list<cache_node_t> cache_;

  using ListIt = typename std::list<cache_node_t>::iterator;
  std::unordered_map<KeyT, ListIt> hash_;

  struct result_t 
  {
    bool is_hit;
    std::optional<cache_node_t> evicted; 
  };

  explicit cache_lfu_t(std::size_t sz) : sz_(sz) {}

  std::size_t max_capacity() const { return sz_; }
  bool full() const { return (cache_.size() == sz_); }

  // returns node for another cache level
  cache_node_t erase_and_return(KeyT key) 
  {
    auto it = hash_.find(key);
    cache_node_t node = *(it->second); 
    
    cache_.erase(it->second);        
    hash_.erase(it);                 
    
    return node;                    
  }

  result_t lookup_update(KeyT key, std::optional<T> page_opt = std::nullopt) 
  {
    if (max_capacity() == 0)
      return {false, std::nullopt};

    auto hit = hash_.find(key);

    // if elem in ht
    if (hit != hash_.end()) 
    {
      auto eltit = hit->second;
      eltit->counter++;

      auto insert_pos = cache_.begin();
      while (insert_pos != eltit && insert_pos->counter > eltit->counter) 
        ++insert_pos;

      cache_.splice(insert_pos, cache_, eltit);

      return {true, std::nullopt};
    }

    // if elem isn't in ht
    if (!page_opt.has_value()) 
    {
      return {false, std::nullopt};
    }

    // if it's a page from another level, we need to set it there 
    T page = std::move(*page_opt);
    std::optional<cache_node_t> evicted_item = std::nullopt;

    if (full()) 
    {
      evicted_item = std::move(cache_.back()); 
      hash_.erase(evicted_item->key);
      cache_.pop_back();
    }

    auto insert_pos = cache_.end(); 
    while (insert_pos != cache_.begin() && std::prev(insert_pos)->counter <= 1) 
    {
        insert_pos--;
    }

    auto itr = cache_.emplace(insert_pos, cache_node_t{key, std::move(page), 1});
    hash_[key] = itr; 
    
    return {false, evicted_item};
  }
};

} // namespace cachelfu
