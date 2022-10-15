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

class Subscriber {
public:
    virtual void operator()(const std::string& topic, const uint8_t* payload, size_t payloadLen, QoS qos, Retained retained) = 0;
    virtual ~Subscriber() = default;
};
enum class RemoveSubRet {
    Default,
    NotFound,
    DeletedLastSubFromTopic
};

template<typename SubType>
class SubscriptionTree {
private:
    struct TreeNode {
        std::unordered_map<std::string, TreeNode> children;
        std::unordered_set<SubType> subscribers;
    };
public:
    void addSubscription(const std::string_view &topicFilter, SubType subscriberId) {
        TreeNode* currentNode = &root;
        lib::splitString(topicFilter, '/', [&](std::string_view part) {
            currentNode = &currentNode->children.emplace(std::string{part}, TreeNode{}).first->second;
            return IterationDecision::Continue;
        });
        currentNode->subscribers.emplace(std::move(subscriberId));
    }

    RemoveSubRet removeSubscription(const std::string_view &topicFilter, const SubType& subscriberId) {
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

    void forEveryMatch(const std::string_view &topic, std::function<void(SubType& )>&& callback)const {
        std::vector<const TreeNode*> currentNodes{&root};
        lib::splitString(topic, '/', [&](std::string_view part) {
            std::vector<const TreeNode*> nextNodes;
            for(auto currentNode: currentNodes) {
                auto it = currentNode->children.find("#");
                if(it != currentNode->children.end()) {
                    for(auto& s: it->second.subscribers) {
                        callback(const_cast<SubType&>(s));
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
            for(auto& s: currentNode->subscribers) {
                callback(const_cast<SubType&>(s));
            }
        }
    }

    std::vector<std::string> removeAllSubscriptions(const SubType& subscriberId) {
        std::vector<std::string> ret;
        removeAllSubsRec(subscriberId, &root, "", ret);
        return ret;
    }
private:
    TreeNode root;

    bool removeAllSubsRec(const SubType& subscriberId, SubscriptionTree<SubType>::TreeNode* current, const std::string& currentSubPath, std::vector<std::string>& deletedSubs) {
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
};

}