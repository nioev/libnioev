#pragma once

#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include "Util.hpp"
#include <string_view>
#include <unordered_set>

namespace nioev::lib {

class SubscriptionTree {
public:
    void addSubscription(void* subscriberId, const std::string_view& topicFilter);
    void removeSubscription(void* subscriberId, const std::string_view& topicFilter);
    void removeAllSubscriptions(void* subscriberId);
    void forEveryMatch(const std::string_view& topic, std::function<void(void* subscriberId)>&& callback) const;
private:
    struct TreeNode {
        std::unordered_map<std::string, TreeNode> children;
        std::unordered_set<void*> subscribers;
    };
    TreeNode root;
};

}