#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace cachelirs {

// LIR: resident page in Stack S, but not in Queue Q.
// HIR_RES: resident HIR page, in Queue Q and optionally in Stack S.
// HIR_NON_RES: non-resident HIR page (ghost), kept in Stack S.
enum class Status { LIR, HIR_RES, HIR_NON_RES };

template <typename T, typename KeyT = int> struct cache_lirs_t {
  std::size_t sz_; // cache capacity
  std::size_t lir_cap_; // target number of LIR pages
  std::size_t hir_cap_; // target number of HIR pages
  std::size_t current_resident_size_ = 0;
  std::size_t lir_size_ = 0; 

  struct node_t {
    KeyT key;
    T page;
    Status status;
    bool in_stack_s = false;
    typename std::list<KeyT>::iterator stack_s_it;
    typename std::list<KeyT>::iterator queue_q_it;
  };

  std::list<KeyT> stack_s_; // Stack S: LIR pages and recently referenced HIR pages
  std::list<KeyT> queue_q_; // Queue Q: all resident HIR pages
  std::unordered_map<KeyT, node_t> hash_; // resident pages + HIR ghosts in S

  struct cache_node_t {
    KeyT key;
    T page;
  };

  struct result_t {
    bool is_hit;
    std::optional<cache_node_t> evicted;
  };

  explicit cache_lirs_t(std::size_t sz) : sz_(sz) {
    if (sz_ <= 1) {
      lir_cap_ = sz_;
      hir_cap_ = 0;
    } else {
      lir_cap_ = std::max<std::size_t>(1, (sz_ * 95) / 100);
      hir_cap_ = sz_ - lir_cap_;
    }
  }

  std::size_t max_capacity() const { return sz_; }

  // Delete HIR pages from the bottom of S until the bottom page is LIR.
  void prune_stack() {
    while (!stack_s_.empty()) {
      KeyT bottom_key = stack_s_.back();
      auto it = hash_.find(bottom_key);

      if (it == hash_.end()) {
        stack_s_.pop_back();
        continue;
      }

      auto& node = it->second;
      if (node.status == Status::LIR) {
        break;
      }

      node.in_stack_s = false;
      stack_s_.pop_back();

      if (node.status == Status::HIR_NON_RES) {
        hash_.erase(it); // A ghost outside S is no longer useful.
      }
    }
  }

  // Evict the least-recently-used resident HIR page. For a capacity-1
  // cache Q is empty, so the only LIR page is evicted instead.
  std::optional<cache_node_t> evict_lru_page() {
    // try to delete hir
    if (!queue_q_.empty()) {
      KeyT key_for_delete = queue_q_.back();
      auto it = hash_.find(key_for_delete);
      auto& node = it->second;

      cache_node_t evicted_node{node.key, std::move(node.page)};
      queue_q_.pop_back();
      --current_resident_size_;

      if (node.in_stack_s) {
        // Keep the key in S as a ghost; the page data has left the cache.
        node.status = Status::HIR_NON_RES;
      } else {
        hash_.erase(it);
      }

      return evicted_node;
    }

    // Capacity 1 has no HIR pages, so evict the LRU LIR page.
    for (auto rit = stack_s_.rbegin(); rit != stack_s_.rend(); ++rit) {
      auto it = hash_.find(*rit);
      if (it == hash_.end() || it->second.status != Status::LIR) {
        continue;
      }

      node_t& node = it->second;
      cache_node_t evicted_node{node.key, std::move(node.page)};
      stack_s_.erase(node.stack_s_it);
      hash_.erase(it);
      --current_resident_size_;
      --lir_size_;
      return evicted_node;
    }

    return std::nullopt;
  }

  
  cache_node_t erase_and_return(KeyT key) {
    auto it = hash_.find(key);

    cache_node_t result{it->second.key, std::move(it->second.page)};

    if (it->second.in_stack_s) {
      stack_s_.erase(it->second.stack_s_it);
    }
    if (it->second.status == Status::HIR_RES) {
      queue_q_.erase(it->second.queue_q_it);
    } 
    else { // LIR
      --lir_size_; 
    }

    hash_.erase(it);
    --current_resident_size_;
    prune_stack();

    // NEW: If an LIR page left this level, promote the oldest resident HIR page
    // so this level does not remain with fewer LIR pages than its target.
    // This adjustment is needed because all_caches.hpp moves pages between levels.
    const std::size_t target_lir = std::min(lir_cap_, current_resident_size_);
    while (lir_size_ < target_lir && !queue_q_.empty()) {
      KeyT key_for_promotion = queue_q_.back();
      auto promote_it = hash_.find(key_for_promotion);
      node_t& node = promote_it->second;

      queue_q_.erase(node.queue_q_it);
      node.status = Status::LIR;
      ++lir_size_;

      if (node.in_stack_s) {
        stack_s_.erase(node.stack_s_it);
      }
      stack_s_.push_front(key_for_promotion);
      node.stack_s_it = stack_s_.begin();
      node.in_stack_s = true;

      prune_stack();
    }

    return result;
  }

  result_t lookup_update(KeyT key, std::optional<T> page_opt = std::nullopt) {
    if (max_capacity() == 0) {
      return {false, std::nullopt};
    }

    auto hit = hash_.find(key);

    // The page is LIR: move it to the top of S.
    if (hit != hash_.end() && hit->second.status == Status::LIR) {
      auto& node = hit->second;

      if (node.in_stack_s) {
        stack_s_.erase(node.stack_s_it);
      }
      stack_s_.push_front(key);
      node.stack_s_it = stack_s_.begin();
      node.in_stack_s = true;

      prune_stack();
      return {true, std::nullopt};
    }

    // The page is resident HIR.
    if (hit != hash_.end() && hit->second.status == Status::HIR_RES) {
      auto& node = hit->second;

      if (node.in_stack_s) {
        // HIR in S becomes LIR. The oldest LIR page is demoted to HIR.
        queue_q_.erase(node.queue_q_it);
        node.status = Status::LIR;
        ++lir_size_; 

        stack_s_.erase(node.stack_s_it);
        stack_s_.push_front(key);
        node.stack_s_it = stack_s_.begin();
        node.in_stack_s = true;

        KeyT bottom_lir_key = stack_s_.back();
        auto& bottom_lir_node = hash_.find(bottom_lir_key)->second;
        bottom_lir_node.status = Status::HIR_RES;
        --lir_size_; 
        queue_q_.push_front(bottom_lir_key);
        bottom_lir_node.queue_q_it = queue_q_.begin();

        prune_stack();
      } else {
        // HIR outside S stays HIR; only its position in Q changes.
        queue_q_.erase(node.queue_q_it);
        queue_q_.push_front(key);
        node.queue_q_it = queue_q_.begin();
      }

      return {true, std::nullopt};
    }

    // The page is a non-resident HIR ghost in S and is requested again.
    if (hit != hash_.end() && hit->second.status == Status::HIR_NON_RES) {
      if (!page_opt.has_value()) {
        return {false, std::nullopt};
      }

      if (!hit->second.in_stack_s) {
        hash_.erase(hit);
        hit = hash_.end();
      } else {
        std::optional<cache_node_t> evicted_node;
        if (current_resident_size_ >= sz_) {
          evicted_node = evict_lru_page();
        }

        auto& node = hit->second;
        node.page = std::move(*page_opt);
        node.status = Status::LIR;
        ++current_resident_size_;
        ++lir_size_;

        stack_s_.erase(node.stack_s_it);
        stack_s_.push_front(key);
        node.stack_s_it = stack_s_.begin();
        node.in_stack_s = true;

        // The newly reloaded page is LIR, so demote the oldest current LIR.
        KeyT bottom_lir_key = stack_s_.back();
        auto& bottom_lir_node = hash_.find(bottom_lir_key)->second;
        bottom_lir_node.status = Status::HIR_RES;
        --lir_size_;
        queue_q_.push_front(bottom_lir_key);
        bottom_lir_node.queue_q_it = queue_q_.begin();

        prune_stack();
        return {false, evicted_node};
      }
    }

    // No resident page or usable ghost was found.
    if (!page_opt.has_value()) {
      return {false, std::nullopt};
    }

    std::optional<cache_node_t> evicted_node;
    if (current_resident_size_ >= sz_) {
      evicted_node = evict_lru_page();
    }

    // Decide whether the new page can be LIR using lir_size_
    bool insert_as_lir = (lir_size_ < lir_cap_);
    T page = std::move(*page_opt); 
    node_t new_node{key, std::move(page),
                    insert_as_lir ? Status::LIR : Status::HIR_RES,
                    true, {}, {}};

    stack_s_.push_front(key);
    new_node.stack_s_it = stack_s_.begin();

    if (insert_as_lir) {
      ++lir_size_;
    } else {
      queue_q_.push_front(key);
      new_node.queue_q_it = queue_q_.begin();
    }

    hash_.emplace(key, std::move(new_node));
    ++current_resident_size_;
    prune_stack();
    return {false, evicted_node};
  }
};

} // namespace cachelirs
