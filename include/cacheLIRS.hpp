#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <optional>

namespace cachelirs {

// lir and hir_res - in ht, hir_non_res - not in ht
enum class Status { LIR, HIR_RES, HIR_NON_RES }; 

template <typename T, typename KeyT = int> struct cache_lirs_t 
{
  std::size_t sz_; // cache capacity
  std::size_t lir_cap_; // hot elements
  std::size_t hir_cap_; // cold elements
  std::size_t current_resident_size_ = 0; 

  struct node_t 
  {
    KeyT key;
    T page;
    Status status;
    bool in_stack_s = false; 
    typename std::list<KeyT>::iterator stack_s_it;
    typename std::list<KeyT>::iterator queue_q_it;
  };

  std::list<KeyT> stack_s_; 
  std::list<KeyT> queue_q_; 
  std::unordered_map<KeyT, node_t> hash_;

  struct cache_node_t {
    KeyT key;
    T page;
  };

  struct result_t {
    bool is_hit;
    std::optional<cache_node_t> evicted; //вытесненный элемент
  };

  explicit cache_lirs_t(std::size_t sz) : sz_(sz) 
  {
    if (sz_ <= 1) 
    {
      lir_cap_ = sz_;
      hir_cap_ = 0;
    } 
    else 
    {
      // 95% for lirs
      lir_cap_ = std::max<std::size_t>(1, (sz_ * 95) / 100);
      hir_cap_ = sz_ - lir_cap_;
    }
  }

  std::size_t max_capacity() const { return sz_; }

  // delete all hirs from the bottom of S
  void prune_stack() 
  {
    while (!stack_s_.empty()) 
    {
      KeyT bottom_key = stack_s_.back();
      auto &node = hash_[bottom_key];
      if (node.status == Status::LIR) 
      {
        break; 
      }
      
      node.in_stack_s = false;
      stack_s_.pop_back();
      
      if (node.status == Status::HIR_NON_RES) 
      {
        hash_.erase(bottom_key);
      }
    }
  }

  // if we send this node to another level of cache
  cache_node_t erase_and_return(KeyT key)
  {
    auto it = hash_.find(key);
    node_t old_node = std::move(it->second);
    
    if (old_node.in_stack_s) {
        stack_s_.erase(old_node.stack_s_it);
    }
    if (old_node.status == Status::HIR_RES) {
        queue_q_.erase(old_node.queue_q_it);
    }
    
    hash_.erase(it);
    
    if (old_node.status != Status::HIR_NON_RES) {
        current_resident_size_--;
    }
    
    prune_stack();
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

      if (node.status == Status::LIR) 
      {
        if (node.in_stack_s) 
        {
          stack_s_.erase(node.stack_s_it);
        }
        stack_s_.push_front(key);
        node.stack_s_it = stack_s_.begin();
        node.in_stack_s = true;

        prune_stack();
        return {true, std::nullopt};
      }

      else if (node.status == Status::HIR_RES) 
      {
        bool was_in_stack = node.in_stack_s;
        
        if (was_in_stack) 
        {
          stack_s_.erase(node.stack_s_it);
        }
        stack_s_.push_front(key);
        node.stack_s_it = stack_s_.begin();
        node.in_stack_s = true;

        if (was_in_stack) 
        {
          node.status = Status::LIR;
          queue_q_.erase(node.queue_q_it); 

          KeyT bottom_lir_key = stack_s_.back(); 
          auto &bottom_lir_node = hash_[bottom_lir_key];
          
          bottom_lir_node.status = Status::HIR_RES;
          queue_q_.push_front(bottom_lir_key);
          bottom_lir_node.queue_q_it = queue_q_.begin();

          prune_stack();
        } 

        // if elem isn't in S
        else 
        {
          queue_q_.erase(node.queue_q_it); 
          queue_q_.push_front(key);
          node.queue_q_it = queue_q_.begin();
        }

        return {true, std::nullopt};
      }

      else if (node.status == Status::HIR_NON_RES) 
      {
        if (!page_opt.has_value()) {
          return {false, std::nullopt};
        }

        node.page = std::move(*page_opt);
        std::optional<cache_node_t> evicted_node = std::nullopt;

        // clean place for new elem
        if (!queue_q_.empty())
        {
          KeyT key_for_delete = queue_q_.back();
          queue_q_.pop_back();
          auto &node_for_delete = hash_[key_for_delete];
          
          evicted_node = cache_node_t{key_for_delete, std::move(node_for_delete.page)};

          if (node_for_delete.in_stack_s) 
          {
            // leave only info about existence in S
            node_for_delete.status = Status::HIR_NON_RES; 
          } 
          else 
          {
            hash_.erase(key_for_delete); 
          }
        }

        node.status = Status::LIR;
        if (node.in_stack_s) 
        {
          stack_s_.erase(node.stack_s_it);
        }
        stack_s_.push_front(key);
        node.stack_s_it = stack_s_.begin();
        node.in_stack_s = true;

        KeyT bottom_lir_key = stack_s_.back(); 
        auto& bottom_lir_node = hash_[bottom_lir_key];
        
        bottom_lir_node.status = Status::HIR_RES;
        queue_q_.push_front(bottom_lir_key);
        bottom_lir_node.queue_q_it = queue_q_.begin();

        prune_stack();
        return {false, evicted_node};
      }
    }

    if (!page_opt.has_value()) {
      return {false, std::nullopt};
    }
    
    T page = std::move(*page_opt);
    node_t new_node{key, page, Status::HIR_RES, true, {}, {}};
    std::optional<cache_node_t> evicted_node = std::nullopt;

    if (current_resident_size_ < lir_cap_) 
    {
      new_node.status = Status::LIR;
      stack_s_.push_front(key);
      new_node.stack_s_it = stack_s_.begin();
      hash_[key] = std::move(new_node);
      current_resident_size_++;
    } 

    // lir is full
    else if (current_resident_size_ < sz_) 
    {
      new_node.status = Status::HIR_RES;
      stack_s_.push_front(key);
      new_node.stack_s_it = stack_s_.begin();
      queue_q_.push_front(key);
      new_node.queue_q_it = queue_q_.begin();
      hash_[key] = std::move(new_node);
      current_resident_size_++;
    } 

    // cache is full, remove elem from Q
    else 
    {
      if (!queue_q_.empty()) 
      {
        KeyT key_for_delete = queue_q_.back();
        queue_q_.pop_back();
        auto &node_for_delete = hash_[key_for_delete];
        
        evicted_node = cache_node_t{key_for_delete, std::move(node_for_delete.page)};

        if (node_for_delete.in_stack_s) 
        {
          // leave only info about existence
          node_for_delete.status = Status::HIR_NON_RES; 
        } 
        else 
        {
          hash_.erase(key_for_delete); 
        }
      }

      stack_s_.push_front(key);
      new_node.stack_s_it = stack_s_.begin();
      queue_q_.push_front(key);
      new_node.queue_q_it = queue_q_.begin();
      hash_[key] = std::move(new_node);
    }

    return {false, evicted_node};
  }
};

} // namespace cachelirs
