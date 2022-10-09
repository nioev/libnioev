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
    enum class RemoveSubRet {
        Default,
        NotFound,
        DeletedLastSubFromTopic
    };
    void addSubscription(void* subscriberId, const std::string_view& topicFilter);
    RemoveSubRet removeSubscription(void* subscriberId, const std::string_view& topicFilter);
    std::vector<std::string> removeAllSubscriptions(void* subscriberId);
    void forEveryMatch(const std::string_view& topic, std::function<void(void* subscriberId)>&& callback) const;
private:
    struct TreeNode {
        std::unordered_map<std::string, TreeNode> children;
        std::unordered_set<void*> subscribers;
    };
    friend bool removeAllSubsRec(void *subscriberId, TreeNode* current, std::string currentSubPath, std::vector<std::string>& deletedSubs);
    TreeNode root;
};

}