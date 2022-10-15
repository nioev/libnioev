#pragma once

#include <atomic>
#include <cstdlib>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <memory>
#include <vector>
#include <string_view>
#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "Enums.hpp"

#include <variant>
#include <vector>

namespace nioev::lib {

using uint = unsigned int;

constexpr const char* LOG_TOPIC = "$NIOEV/log";

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

inline std::string errnoToString() {
    char buffer[1024] = { 0 };
#ifdef __linux__
    return strerror_r(errno, buffer, 1024);
#else
    strerror_r(errno, buffer, 1024);
    return buffer;
#endif
}

inline void throwErrno(std::string msg) {
    throw std::runtime_error{msg + ": " + errnoToString()};
}

class SharedBuffer final {
public:
    SharedBuffer() = default;
    ~SharedBuffer() = default;
    [[nodiscard]] const uint8_t *data() const {
        if(!mBuffer)
            return nullptr;
        return mBuffer->data();
    }
    [[nodiscard]] uint8_t *data() {
        if(!mBuffer)
            return nullptr;
        return mBuffer->data();
    }
    [[nodiscard]] size_t size() const {
        if(!mBuffer)
            return 0;
        return mBuffer->size();
    }
    void append(const void* data, size_t size) {
        if(!mBuffer)
            mBuffer = std::make_shared<std::vector<uint8_t>>();
        mBuffer->insert(mBuffer->end(), (uint8_t*)data, (uint8_t*)data + size);
    }
    void insert(size_t index, const void* data, size_t size) {
        if(!mBuffer) {
            if(index == 0) {
                append(data, size);
            } else {
                throw std::runtime_error{"No such index in shared buffer of size " + std::to_string(this->size()) + " at index " + std::to_string(index)};
            }
        }
        mBuffer->insert(mBuffer->begin() + index, (uint8_t*)data, (uint8_t*) data + size);
    }
    SharedBuffer copy() {
        SharedBuffer ret;
        if(!mBuffer)
            return ret;
        ret.append(mBuffer->data(), mBuffer->size());
        ret.mPacketId = mPacketId;
        return ret;
    }
    [[nodiscard]] uint16_t getPacketId() const {
        return mPacketId;
    }
    void setPacketId(uint16_t id) {
        mPacketId = id;
    }
private:
    uint16_t mPacketId{0};
    std::shared_ptr<std::vector<uint8_t>> mBuffer;
};
/*class SharedBuffer final {
public:
    SharedBuffer() = default;
    ~SharedBuffer();
    SharedBuffer(const SharedBuffer&);
    SharedBuffer& operator=(const SharedBuffer&);
    SharedBuffer(SharedBuffer&&) noexcept;
    SharedBuffer& operator=(SharedBuffer&&) noexcept;
    void resize(size_t newSize);

    [[nodiscard]] size_t size() const {
        return mSize;
    }
    [[nodiscard]] const uint8_t* data() const {
        if(mBuffer == nullptr)
            return nullptr;
        return (uint8_t*)(mBuffer + sizeof(std::atomic<int>));
    }
    [[nodiscard]] uint8_t* data() {
        if(mBuffer == nullptr)
            return nullptr;
        return (uint8_t*)(mBuffer + sizeof(std::atomic<int>));
    }
    void append(const void* data, size_t size);
    void insert(size_t index, const void* data, size_t size);
    SharedBuffer copy() const;
    [[nodiscard]] uint16_t getPacketId() const {
        return mPacketId;
    }
    void setPacketId(uint16_t id) {
        mPacketId = id;
    }
private:
    std::atomic<int>& getRefCounter();
    const std::atomic<int>& getRefCounter() const;
    void incRefCount();
    void decRefCount();
    std::byte* mBuffer { nullptr };
    size_t mReserved = 0;
    size_t mSize = 0;
    uint16_t mPacketId{0};
};*/

using MQTTPropertyValue = std::variant<uint8_t, uint16_t, std::vector<uint8_t>, std::string, std::pair<std::string, std::string>, uint32_t>;
using PropertyList = std::unordered_multimap<MQTTProperty, MQTTPropertyValue>;

class BinaryEncoder {
public:
    void encodeByte(uint8_t value) {
        mData.append(&value, 1);
    }
    void encode2Bytes(uint16_t value) {
        value = htons(value);
        mData.append((uint8_t*)&value, 2);
    }
    void encode4Bytes(uint32_t value) {
        value = htonl(value);
        mData.append((uint8_t*)&value, 4);
    }
    void encodePacketId(uint16_t value) {
        encode2Bytes(value);
        mData.setPacketId(value);
    }
    void encodeString(const std::string& str) {
        encode2Bytes(str.size());
        mData.append(str.c_str(), str.size());
    }
    void encodeBytes(const std::vector<uint8_t>& data) {
        mData.append(data.data(), data.size());
    }
    // this function takes about 33% of total calculation time - TODO optimize
    void insertPacketLength() {
        uint32_t packetLength = mData.size() - 1; // exluding first byte
        encodeVarByteInt(packetLength, 1);
    }
    SharedBuffer&& moveData() {
        return std::move(mData);
    }
    void encodePropertyList(const PropertyList& propertyList) {
        auto start = mData.size();
        for(const auto& [propId, propValue] : propertyList) {
            encodeByte(static_cast<uint8_t>(propId));
            switch(propertyToPropertyType(propId)) {
            case MQTTPropertyType::Byte:
                encodeByte(std::get<uint8_t>(propValue));
                break;
            case MQTTPropertyType::TwoByteInt:
                encode2Bytes(std::get<uint16_t>(propValue));
                break;
            case MQTTPropertyType::VarByteInt:
            case MQTTPropertyType::FourByteInt:
                encode4Bytes(std::get<uint32_t>(propValue));
                break;
            case MQTTPropertyType::UTF8String:
                encodeString(std::get<std::string>(propValue));
                break;
            case MQTTPropertyType::UTF8StringPair:
                encodeString(std::get<std::pair<std::string, std::string>>(propValue).first);
                encodeString(std::get<std::pair<std::string, std::string>>(propValue).second);
                break;
            case MQTTPropertyType::BinaryData:
                encodeBytes(std::get<std::vector<uint8_t>>(propValue));
                break;
            default:
                assert(0);
            }
        }
        encodeVarByteInt(mData.size() - start, start);
    }
    void encodeVarByteInt(uint32_t value) {
        encodeVarByteInt(value, mData.size());
    }
    void encodeVarByteInt(uint32_t value, size_t offset) {
        do {
            uint8_t encodeByte = value % 128;
            value = value / 128;
            // if there are more bytes to encode, set the upper bit
            if(value > 0) {
                encodeByte |= 128;
            }
            mData.insert(offset, &encodeByte, 1);
            offset += 1;
        } while(value > 0);

    }

private:
    SharedBuffer mData;
};

class BinaryDecoder {
public:
    explicit BinaryDecoder(const std::vector<uint8_t>& data, uint usableSize)
    : mData(data), mUsableSize(usableSize) {

    }
    std::string decodeString() {
        auto len = decode2Bytes();
        if(len > mData.size() - mOffset) {
            throw std::runtime_error{"Out of bounds string"};
        }
        std::string ret{mData.begin() + mOffset, mData.begin() + mOffset + len};
        mOffset += len;
        return ret;
    }
    std::vector<uint8_t> decodeBytesWithPrefixLength() {
        auto len = decode2Bytes();
        if(len > mData.size() - mOffset) {
            throw std::runtime_error{"Out of bounds string bytes"};
        }
        std::vector<uint8_t> ret{mData.begin() + mOffset, mData.begin() + mOffset + len};
        mOffset += len;
        return ret;
    }
    uint8_t decodeByte() {
        return mData.at(mOffset++);
    }
    uint16_t decode2Bytes() {
        if(2 > mData.size() - mOffset) {
            throw std::runtime_error{"Out of bounds 2 bytes decoding"};
        }
        uint16_t len;
        memcpy(&len, mData.data() + mOffset, 2);
        len = ntohs(len);
        mOffset += 2;
        return len;
    }
    uint32_t decode4Bytes() {
        if(4 > mData.size() - mOffset) {
            throw std::runtime_error{"Out of bounds 2 bytes decoding"};
        }
        uint32_t len;
        memcpy(&len, mData.data() + mOffset, 4);
        len = ntohl(len);
        mOffset += 4;
        return len;
    }
    const uint8_t *getCurrentPtr() {
        return mData.data() + mOffset;
    }
    const size_t getCurrentRemainingLength() {
        return mData.size() - mOffset;
    }
    void advance(uint length) {
        // TODO range checks
        mOffset += length;
    }
    std::vector<uint8_t> getRemainingBytes() {
        std::vector<uint8_t> ret(mData.begin() + mOffset, mData.end());
        mOffset = mData.size();
        return ret;
    }
    bool empty() {
        return mOffset >= mUsableSize;
    }
    uint32_t decodeVarLengthInteger() {
        uint32_t multiplier = 1;
        uint32_t value = 0;
        uint8_t encodedByte;
        do {
            encodedByte = decodeByte();
            value += uint32_t(encodedByte & 127) * multiplier;
            if(multiplier > 128 * 128 * 128) {
                throw std::runtime_error{"Failed to decode var length"};
            }
            multiplier *= 128;
        } while ((encodedByte & 128) != 0);
        return value;
    }
    PropertyList decodeProperties() {
        uint32_t length = decodeVarLengthInteger();
        if(mOffset + length > mData.size()) {
            throw std::runtime_error{"Not enough space for properties"};
        }
        PropertyList ret;
        uint32_t start = mOffset;
        while(mOffset - start < length) {
            auto property = byteToMQTTProperty(decodeByte());
            MQTTPropertyValue value;
            switch(propertyToPropertyType(property)) {
            case MQTTPropertyType::Byte:
                value = decodeByte();
                break;
            case MQTTPropertyType::TwoByteInt:
                value = decode2Bytes();
                break;
            case MQTTPropertyType::FourByteInt:
                value = decode4Bytes();
                break;
            case MQTTPropertyType::VarByteInt:
                value = decodeVarLengthInteger();
                break;
            case MQTTPropertyType::BinaryData:
                value = decodeBytesWithPrefixLength();
                break;
            case MQTTPropertyType::UTF8String:
                value = decodeString();
                break;
            case MQTTPropertyType::UTF8StringPair:
                value = std::make_pair(decodeString(), decodeString());
                break;
            }
            ret.emplace(property, std::move(value));
        }
        return ret;
    }
private:
    const std::vector<uint8_t>& mData;
    uint mOffset = 0;
    uint mUsableSize = 0;
};

template<typename T>
class DestructWrapper final {
public:
    explicit DestructWrapper(T func)
    : mFunc(std::move(func)) {

    }
    ~DestructWrapper() {
        execute();
    }

    void execute() {
        if(mFunc) {
            mFunc.value()();
            mFunc.reset();
        }
    }

private:
    std::optional<T> mFunc;
};

static bool startsWith(const std::string& str, const std::string_view& prefix) {
    for(size_t i = 0; i < prefix.size(); ++i) {
        if(i >= str.size())
            return false;
        if(prefix.at(i) != str.at(i)) {
            return false;
        }
    }
    return true;
}

enum class IterationDecision {
    Continue,
    Stop
};

template<typename T>
static void splitString(const std::string_view& str, char delimiter, T callback) {
    std::string::size_type offset = 0, nextOffset = 0;
    do {
        nextOffset = str.find(delimiter, offset);
        if(callback(std::string_view{str}.substr(offset, nextOffset - offset)) == IterationDecision::Stop) {
            break;
        }
        offset = nextOffset + 1;
    } while(nextOffset != std::string::npos);
}

static bool doesTopicMatchSubscription(const std::string_view& topic, const std::vector<std::string>& topicSplit) {
    size_t partIndex = 0;
    bool doesMatch = true;
    if((topic.at(0) == '$' && topicSplit.at(0).at(0) != '$') || (topic.at(0) != '$' && topicSplit.at(0).at(0) == '$')) {
        return false;
    }
    splitString(topic, '/', [&] (const auto& actualPart) {
        if(topicSplit.size() <= partIndex) {
            doesMatch = false;
            return IterationDecision::Stop;
        }
        const auto& expectedPart = topicSplit.at(partIndex);
        if(actualPart == expectedPart || expectedPart == "+") {
            partIndex += 1;
            return IterationDecision::Continue;
        }
        if(expectedPart == "#") {
            partIndex = topicSplit.size();
            return IterationDecision::Stop;
        }
        doesMatch = false;
        return IterationDecision::Stop;
    });
    return doesMatch && partIndex == topicSplit.size();
}
static bool doesTopicMatchSubscription(const std::vector<std::string>& topic, const std::vector<std::string>& subscription) {
    size_t partIndex = 0;
    bool doesMatch = true;
    if(topic.empty())
        return subscription.empty();
    if((topic.at(0).at(0) == '$' && subscription.at(0).at(0) != '$') || (topic.at(0).at(0) != '$' && subscription.at(0).at(0) == '$')) {
        return false;
    }
    for(auto& actualPart: topic) {
        if(subscription.size() <= partIndex) {
            doesMatch = false;
            break;
        }
        const auto& expectedPart = subscription.at(partIndex);
        if(actualPart == expectedPart || expectedPart == "+") {
            partIndex += 1;
            continue;
        }
        if(expectedPart == "#") {
            partIndex = subscription.size();
            break;
        }
        doesMatch = false;
        break;
    }
    return doesMatch && partIndex == subscription.size();
}

inline std::vector<std::string> splitTopics(const std::string& topic) {
    std::vector<std::string> parts;
    splitString(topic, '/', [&parts](const std::string_view& part) {
        parts.emplace_back(part);
        return IterationDecision::Continue;
    });
    return parts;
}

inline bool hasWildcard(const std::string& topic) {
    return std::any_of(topic.begin(), topic.end(), [](char c) {
        return c == '#' || c == '+';
    });
}

// return e.g. ".js" or ".mp3"
inline std::string_view getFileExtension(const std::string& filename) {
    auto index = filename.find_last_of('.');
    if(index == std::string::npos) {
        return {};
    }
    return std::string_view{filename}.substr(index);
}

inline bool hasValidScriptExtension(const std::string& filename) {
    auto ext = getFileExtension(filename);
    return ext == ".js" || ext == ".cpp";
}

// return e.g. test for test.mp3
inline std::string_view getFileStem(const std::string& filename) {
    auto start = filename.find_last_of('/');
    if(start == std::string::npos) {
        start = 0;
    }
    auto end = filename.find_last_of('.');
    if(end == std::string::npos) {
        end = filename.size();
    }
    return std::string_view{filename}.substr(start, end - start);
}

class Stopwatch {
public:
    Stopwatch(const char* name)
    : mName(name) {
        mStart = std::chrono::steady_clock::now();
    }
    ~Stopwatch() {
#ifdef SPDLOG_API
        spdlog::debug("{} took {}µs", mName, getElapsedUS());
#endif
    }
    uint64_t getElapsedUS() {
        return std::chrono::duration_cast<std::chrono::microseconds>((std::chrono::steady_clock::now() - mStart)).count();
    }
    Stopwatch(const Stopwatch&) = delete;
    void operator=(const Stopwatch&) = delete;
    Stopwatch(Stopwatch&&) = delete;
    void operator=(Stopwatch&&) = delete;

private:
    const char* mName{ nullptr };
    std::chrono::steady_clock::time_point mStart;
};

constexpr static const char* LOG_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] %^[%-7l]%$ [%-15N] %v";

static inline std::vector<uint8_t> stringToBuffer(const std::string& input) {
    return std::vector<uint8_t>((const uint8_t*)input.c_str(), (const uint8_t*)input.c_str() + input.size());
}
}
