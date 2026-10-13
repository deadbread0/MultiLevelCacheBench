#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <optional>

namespace cachearc {

enum class Status 
{ 
  T1,          // in memory, recent (accessed once) 
  T2,          // in memory, frequent (access ≥ 2 times)
  B1,          // without a page in memory, evicted from T1
  B2           // without a page in memory, evicted from T2
};

template <typename T, typename KeyT = int> 
struct cache_arc_t 
{
  std::size_t sz_;                   
  std::size_t p_ = 0; // capacity of T1 (cap of T2 = sz_ - p_)             

  struct node_t 
  {
    KeyT key;
    T page;
    Status status;
    typename std::list<KeyT>::iterator list_it;
  };

  std::list<KeyT> t1_; 
  std::list<KeyT> t2_;
  std::list<KeyT> b1_; 
  std::list<KeyT> b2_; 
  
  std::unordered_map<KeyT, node_t> hash_;

  struct cache_node_t {
    KeyT key;
    T page;
  };

  struct result_t {
    bool is_hit;
    std::optional<cache_node_t> evicted; 
  };

  explicit cache_arc_t(std::size_t sz) : sz_(sz) {}

  std::size_t max_capacity() const { return sz_; }

  // if we need to change status of page
  std::optional<cache_node_t> replace_one_page(Status target_status) 
  {
    std::optional<cache_node_t> evicted_node = std::nullopt;

    // if T1 is full or we work with B2 and T1 is full
    if (!t1_.empty() && ((t1_.size() > p_) || (target_status == Status::B2 && t1_.size() == p_))) 
    {
      KeyT key_to_move = t1_.back();
      t1_.pop_back();

      auto &node = hash_[key_to_move];
      node.status = Status::B1;
      
      evicted_node = cache_node_t{key_to_move, std::move(node.page)};
      node.page = T{}; 

      b1_.push_front(key_to_move);
      node.list_it = b1_.begin();
    } 
    else if (!t2_.empty()) // t2->b2
    {
      KeyT key_to_move = t2_.back();
      t2_.pop_back();

      auto &node = hash_[key_to_move];
      node.status = Status::B2;
      
      evicted_node = cache_node_t{key_to_move, std::move(node.page)};
      node.page = T{}; // unload the page from memory

      b2_.push_front(key_to_move);
      node.list_it = b2_.begin();
    }
    return evicted_node;
  }

  cache_node_t erase_and_return(KeyT key)
  {
    auto it = hash_.find(key);
    node_t old_node = std::move(it->second);
    
    if (old_node.status == Status::T1) t1_.erase(old_node.list_it);
    else if (old_node.status == Status::T2) t2_.erase(old_node.list_it);
    else if (old_node.status == Status::B1) b1_.erase(old_node.list_it);
    else if (old_node.status == Status::B2) b2_.erase(old_node.list_it);
    
    hash_.erase(it);
    return cache_node_t{old_node.key, std::move(old_node.page)};
  }

  result_t lookup_update(KeyT key, std::optional<T> page_opt = std::nullopt) 
  {
    if (max_capacity() == 0)
      return {false, std::nullopt};

    auto hit = hash_.find(key);

    // elem in ht
    if (hit != hash_.end()) 
    {
      auto &node = hit->second;

      if (node.status == Status::T1) 
      {
        t1_.erase(node.list_it);
        
        node.status = Status::T2;
        t2_.push_front(key);
        node.list_it = t2_.begin();
        return {true, std::nullopt};
      }

      if (node.status == Status::T2) 
      {
        t2_.erase(node.list_it);
        
        t2_.push_front(key);
        node.list_it = t2_.begin();
        return {true, std::nullopt};
      }

      if (node.status == Status::B1) 
      {
        if (!page_opt.has_value()) {
          return {false, std::nullopt};
        }

        // make p_ bigger
        std::size_t d1 = (b1_.size() >= b2_.size()) ? 1 : (b2_.size() / b1_.size());
        p_ = std::min(sz_, p_ + d1);

        std::optional<cache_node_t> evicted_node = replace_one_page(Status::B1); // free page

        b1_.erase(node.list_it);
        node.page = std::move(*page_opt); 
        node.status = Status::T2;      

        t2_.push_front(key);
        node.list_it = t2_.begin();
        return {false, evicted_node};
      }

      if (node.status == Status::B2) 
      {
        if (!page_opt.has_value()) {
          return {false, std::nullopt};
        }

        // make p_ smaller
        std::size_t d2 = (b2_.size() >= b1_.size()) ? 1 : (b1_.size() / b2_.size());
        p_ = (p_ > d2) ? (p_ - d2) : 0;

        std::optional<cache_node_t> evicted_node = replace_one_page(Status::B2); // free page

        b2_.erase(node.list_it);
        node.page = std::move(*page_opt); 
        node.status = Status::T2;

        t2_.push_front(key);
        node.list_it = t2_.begin();
        return {false, evicted_node};
      }
    }

    if (!page_opt.has_value()) {
      return {false, std::nullopt};
    }

    T page = std::move(*page_opt);
    node_t new_node{key, page, Status::T1, {}};
    std::optional<cache_node_t> evicted_node = std::nullopt;

    // limit history of requests
    if (t1_.size() + b1_.size() == sz_) 
    {
      if (t1_.size() < sz_) 
      {
        hash_.erase(b1_.back());
        b1_.pop_back();
        evicted_node = replace_one_page(Status::T1);
      } 

      // T1 is full
      else 
      {
        auto it = hash_.find(t1_.back());
        evicted_node = cache_node_t{it->first, std::move(it->second.page)};
        hash_.erase(it);
        t1_.pop_back();
      }
    } 
    else if (t1_.size() + b1_.size() < sz_) 
    {
      std::size_t total_history = t1_.size() + t2_.size() + b1_.size() + b2_.size();
      if (total_history >= sz_) 
      {
        if (total_history == 2 * sz_) // limit history
        {
          hash_.erase(b2_.back());
          b2_.pop_back();
        }
        evicted_node = replace_one_page(Status::T1);
      }
    }

    t1_.push_front(key);
    new_node.list_it = t1_.begin();
    hash_[key] = std::move(new_node);

    return {false, evicted_node};
  }
};

} // namespace cachearc
