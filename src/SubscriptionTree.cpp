#include <queue>
#include "nioev/lib/SubscriptionTree.hpp"

namespace nioev::lib {
    
void SubscriptionTree::addSubscription(void *subscriberId, const std::string_view &topicFilter) {
    TreeNode* currentNode = &root;
    lib::splitString(topicFilter, '/', [&](std::string_view part) {
        currentNode = &currentNode->children.emplace(std::string{part}, TreeNode{}).first->second;
        return IterationDecision::Continue;
    });
    currentNode->subscribers.emplace(subscriberId);
}

SubscriptionTree::RemoveSubRet SubscriptionTree::removeSubscription(void *subscriberId, const std::string_view &topicFilter) {
    TreeNode* prevNode = nullptr;
    TreeNode* currentNode = &root;
    bool found = true;
    std::string lastPart;
    lib::splitString(topicFilter, '/', [&](std::string_view part) {
        auto it = currentNode->children.find(std::string{part});
        if(it == currentNode->children.end()) {
            found = false;
            return IterationDecision::Stop;
        }
        lastPart = part;
        prevNode = currentNode;
        currentNode = &it->second;
        return IterationDecision::Continue;
    });
    if(!found)
        return RemoveSubRet::NotFound;
    currentNode->subscribers.erase(subscriberId);
    if(currentNode->subscribers.empty() && currentNode->children.empty() && prevNode) {
        prevNode->children.erase(lastPart);
        return RemoveSubRet::DeletedLastSubFromTopic;
    }
    return RemoveSubRet::Default;
}

void SubscriptionTree::forEveryMatch(const std::string_view &topic, std::function<void(void *)>&& callback)const {
    std::vector<const TreeNode*> currentNodes{&root};
    lib::splitString(topic, '/', [&](std::string_view part) {
        std::vector<const TreeNode*> nextNodes;
        for(auto currentNode: currentNodes) {
            auto it = currentNode->children.find("#");
            if(it != currentNode->children.end()) {
                for(auto s: it->second.subscribers) {
                    callback(s);
                }
            }
            it = currentNode->children.find(std::string{part});
            if(it != currentNode->children.end()) {
                nextNodes.emplace_back(&it->second);
            }
            it = currentNode->children.find("+");
            if(it != currentNode->children.end()) {
                nextNodes.emplace_back(&it->second);
            }
        }
        currentNodes = nextNodes;
        return IterationDecision::Continue;
    });
    for(auto currentNode: currentNodes) {
        for(auto s: currentNode->subscribers) {
            callback(s);
        }
    }
}

bool removeAllSubsRec(void *subscriberId, SubscriptionTree::TreeNode* current, std::string currentSubPath, std::vector<std::string>& deletedSubs) {
    current->subscribers.erase(subscriberId);
    if(current->subscribers.empty() && current->children.empty() && !currentSubPath.empty()) {
        deletedSubs.emplace_back(currentSubPath.substr(0, currentSubPath.size() - 1));
        return true;
    }
    for(auto it = current->children.begin(); it != current->children.end();) {
        if(removeAllSubsRec(subscriberId, &it->second, currentSubPath + it->first + "/", deletedSubs)) {
            it = current->children.erase(it);
        } else {
            ++it;
        }
    }
    return false;
};

std::vector<std::string> SubscriptionTree::removeAllSubscriptions(void *subscriberId) {
    std::vector<std::string> ret;
    removeAllSubsRec(subscriberId, &root, "", ret);
    return ret;
}
}