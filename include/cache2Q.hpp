#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <optional>

namespace cache2q {

enum class Status { IN, MAIN, OUT }; 

template <typename T, typename KeyT = int> struct cache_2q_t 
{
  std::size_t sz_;             //IN + MAIN
  std::size_t max_in_;        
  std::size_t max_out_;      
  std::size_t current_resident_size_ = 0; 

  struct node_t 
  {
    KeyT key;
    T page;
    Status status;
    typename std::list<KeyT>::iterator queue_it;
  };

  std::list<KeyT> queue_in_;  
  std::list<KeyT> queue_out_;  
  std::list<KeyT> queue_main_; 
  
  std::unordered_map<KeyT, node_t> hash_;

  struct cache_node_t {
    KeyT key;
    T page;
  };

  struct result_t {
    bool is_hit;
    std::optional<cache_node_t> evicted;
  };

  explicit cache_2q_t(std::size_t sz) : sz_(sz) 
  {
    //25% IN, 50% OUT => 25% main
    max_in_ = sz_ / 4;
    if (max_in_ == 0 && sz_ > 0) max_in_ = 1;
    
    max_out_ = sz_ / 2;
    if (max_out_ == 0 && sz_ > 0) max_out_ = 1;
  }

  std::size_t max_capacity() const { return sz_; }

  std::optional<cache_node_t> free_one_resident_page()
  {
    std::optional<cache_node_t> evicted_node = std::nullopt;

    if (queue_in_.size() > max_in_ && !queue_in_.empty())
    {
      KeyT key_to_move = queue_in_.back(); //in->out
      queue_in_.pop_back();
      
      auto &node = hash_[key_to_move];
      node.status = Status::OUT;
      
      evicted_node = cache_node_t{key_to_move, std::move(node.page)};
      node.page = T{}; 

      if (queue_out_.size() >= max_out_)
      {
        KeyT dead_key = queue_out_.back();
        queue_out_.pop_back();
        hash_.erase(dead_key);
      }

      queue_out_.push_front(key_to_move);
      node.queue_it = queue_out_.begin();
    }

    // if IN isn't full - delete from the end of main
    else if (!queue_main_.empty()) 
    {
      KeyT key_for_delete = queue_main_.back();
      queue_main_.pop_back();
      
      auto it = hash_.find(key_for_delete);
      evicted_node = cache_node_t{it->first, std::move(it->second.page)};
      hash_.erase(it);
    }
    return evicted_node;
  }

  cache_node_t erase_and_return(KeyT key)
  {
    auto it = hash_.find(key);
    node_t old_node = std::move(it->second);
    
    if (old_node.status == Status::IN) queue_in_.erase(old_node.queue_it);
    else if (old_node.status == Status::MAIN) queue_main_.erase(old_node.queue_it);
    else if (old_node.status == Status::OUT) queue_out_.erase(old_node.queue_it);
    
    hash_.erase(it);
    
    if (old_node.status != Status::OUT) {
        current_resident_size_--;
    }
    
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

      if (node.status == Status::MAIN) 
      {
        queue_main_.erase(node.queue_it);
        queue_main_.push_front(key);
        node.queue_it = queue_main_.begin();
        return {true, std::nullopt};
      }

      else if (node.status == Status::IN) 
      {
        queue_in_.erase(node.queue_it);
        queue_in_.push_front(key);
        node.queue_it = queue_in_.begin();
        return {true, std::nullopt};
      }

      else if (node.status == Status::OUT) 
      {
        if (!page_opt.has_value()) {
          return {false, std::nullopt};
        }

        node.page = std::move(*page_opt);
        queue_out_.erase(node.queue_it);
        std::optional<cache_node_t> evicted_node = std::nullopt;

        if (current_resident_size_ >= sz_)
        {
          evicted_node = free_one_resident_page();
        }
        else
        {
          current_resident_size_++;
        }

        node.status = Status::MAIN;
        queue_main_.push_front(key);
        node.queue_it = queue_main_.begin();
        return {false, evicted_node};
      }
    }

    if (!page_opt.has_value()) {
      return {false, std::nullopt};
    }

    T page = std::move(*page_opt);
    node_t new_node{key, page, Status::IN, {}};
    std::optional<cache_node_t> evicted_node = std::nullopt;

    if (current_resident_size_ < sz_)
    {
      queue_in_.push_front(key);
      new_node.queue_it = queue_in_.begin();
      hash_[key] = std::move(new_node);
      current_resident_size_++;
    }
    else
    {
      evicted_node = free_one_resident_page();

      queue_in_.push_front(key);
      new_node.queue_it = queue_in_.begin();
      hash_[key] = std::move(new_node);
    }

    return {false, evicted_node};
  }
};

} // namespace cache2q
