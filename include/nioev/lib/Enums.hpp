#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>

namespace nioev::lib {

enum class MQTTMessageType : uint8_t
{
    Invalid = 0,
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5,
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14,
    Count = 15
};

enum class QoS : uint8_t
{
    QoS0 = 0,
    QoS1 = 1,
    QoS2 = 2,
};

static inline QoS minQoS(QoS a, QoS b) {
    return static_cast<uint8_t>(a) < static_cast<uint8_t>(b) ? a : b;
}

enum class Retained
{
    No,
    Yes
};
using Retain = Retained;

enum class CleanSession
{
    Yes,
    No
};

enum class Compression
{
    NONE,
    ZSTD
};

enum class MQTTPropertyType
{
    Byte,
    TwoByteInt,
    BinaryData,
    UTF8String,
    UTF8StringPair,
    FourByteInt,
    VarByteInt,
};


enum class MQTTProperty : uint8_t
{
    PAYLOAD_FORMAT_INDICATOR = 0x01,
    MESSAGE_EXPIRY_INTERVAL = 0x02,
    CONTENT_TYPE = 0x03,
    RESPONSE_TOPIC = 0x08,
    CORRELATION_DATA = 0x09,
    SUBSCRIPTION_IDENTIFIER = 0x0B,
    SESSION_EXPIRY_INTERVAL = 0x11,
    ASSIGNED_CLIENT_IDENTIFIER = 0x12,
    SERVER_KEEP_ALIVE = 0x13,
    AUTHENTICATION_METHOD = 0x15,
    AUTHENTICATION_DATA = 0x16,
    REQUEST_PROBLEM_INFORMATION = 0x17,
    WILL_DELAY_INTERVAL = 0x18,
    REQUEST_RESPONSE_INFORMATION = 0x19,
    RESPONSE_INFORMATION = 0x1A,
    SERVER_REFERENCE = 0x1C,
    REASON_STRING = 0x1F,
    RECEIVE_MAXIMUM = 0x21,
    TOPIC_ALIAS_MAXIMUM = 0x22,
    TOPIC_ALIAS = 0x23,
    MAXIMUM_QOS = 0x24,
    RETAIN_AVAILABLE = 0x25,
    USER_PROPERTY = 0x26,
    MAXIMUM_PACKET_SIZE = 0x27,
    WILDCARD_SUBSCRIPTION_AVAILABLE = 0x28,
    SUBSCRIPTION_IDENTIFIER_AVAILABLE = 0x29,
    SHARED_SUBSCRIPTION_AVAILABLE = 0x2A,
};

static inline MQTTPropertyType propertyToPropertyType(MQTTProperty property) {
    switch(property) {
    case MQTTProperty::PAYLOAD_FORMAT_INDICATOR:
    case MQTTProperty::REQUEST_PROBLEM_INFORMATION:
    case MQTTProperty::MAXIMUM_QOS:
    case MQTTProperty::RETAIN_AVAILABLE:
    case MQTTProperty::REQUEST_RESPONSE_INFORMATION:
    case MQTTProperty::WILDCARD_SUBSCRIPTION_AVAILABLE:
    case MQTTProperty::SUBSCRIPTION_IDENTIFIER_AVAILABLE:
    case MQTTProperty::SHARED_SUBSCRIPTION_AVAILABLE:
        return MQTTPropertyType::Byte;
    case MQTTProperty::MESSAGE_EXPIRY_INTERVAL:
    case MQTTProperty::SESSION_EXPIRY_INTERVAL:
    case MQTTProperty::WILL_DELAY_INTERVAL:
    case MQTTProperty::MAXIMUM_PACKET_SIZE:
        return MQTTPropertyType::FourByteInt;
    case MQTTProperty::CONTENT_TYPE:
    case MQTTProperty::RESPONSE_TOPIC:
    case MQTTProperty::ASSIGNED_CLIENT_IDENTIFIER:
    case MQTTProperty::AUTHENTICATION_METHOD:
    case MQTTProperty::RESPONSE_INFORMATION:
    case MQTTProperty::SERVER_REFERENCE:
    case MQTTProperty::REASON_STRING:
        return MQTTPropertyType::UTF8String;
    case MQTTProperty::CORRELATION_DATA:
    case MQTTProperty::AUTHENTICATION_DATA:
        return MQTTPropertyType::BinaryData;
    case MQTTProperty::SUBSCRIPTION_IDENTIFIER:
        return MQTTPropertyType::VarByteInt;
    case MQTTProperty::USER_PROPERTY:
        return MQTTPropertyType::UTF8StringPair;
    case MQTTProperty::SERVER_KEEP_ALIVE:
    case MQTTProperty::RECEIVE_MAXIMUM:
    case MQTTProperty::TOPIC_ALIAS_MAXIMUM:
    case MQTTProperty::TOPIC_ALIAS:
        return MQTTPropertyType::TwoByteInt;
    }
    assert(0);
};

// FIXME check if the compiler optimizes this check out due to UB
static inline bool isValidProperty(uint8_t byte) {
    switch(static_cast<MQTTProperty>(byte)) {
        case MQTTProperty::PAYLOAD_FORMAT_INDICATOR:
        case MQTTProperty::MESSAGE_EXPIRY_INTERVAL:
        case MQTTProperty::CONTENT_TYPE:
        case MQTTProperty::RESPONSE_TOPIC:
        case MQTTProperty::CORRELATION_DATA:
        case MQTTProperty::SUBSCRIPTION_IDENTIFIER:
        case MQTTProperty::SESSION_EXPIRY_INTERVAL:
        case MQTTProperty::ASSIGNED_CLIENT_IDENTIFIER:
        case MQTTProperty::SERVER_KEEP_ALIVE:
        case MQTTProperty::AUTHENTICATION_METHOD:
        case MQTTProperty::AUTHENTICATION_DATA:
        case MQTTProperty::REQUEST_PROBLEM_INFORMATION:
        case MQTTProperty::WILL_DELAY_INTERVAL:
        case MQTTProperty::REQUEST_RESPONSE_INFORMATION:
        case MQTTProperty::RESPONSE_INFORMATION:
        case MQTTProperty::SERVER_REFERENCE:
        case MQTTProperty::REASON_STRING:
        case MQTTProperty::RECEIVE_MAXIMUM:
        case MQTTProperty::TOPIC_ALIAS_MAXIMUM:
        case MQTTProperty::TOPIC_ALIAS:
        case MQTTProperty::MAXIMUM_QOS:
        case MQTTProperty::RETAIN_AVAILABLE:
        case MQTTProperty::USER_PROPERTY:
        case MQTTProperty::MAXIMUM_PACKET_SIZE:
        case MQTTProperty::WILDCARD_SUBSCRIPTION_AVAILABLE:
        case MQTTProperty::SUBSCRIPTION_IDENTIFIER_AVAILABLE:
        case MQTTProperty::SHARED_SUBSCRIPTION_AVAILABLE:
            return true;
    }
    return false;
}

static inline MQTTProperty byteToMQTTProperty(uint8_t byte) {
    if(isValidProperty(byte))
        return static_cast<MQTTProperty>(byte);
    throw std::runtime_error{"Invalid property byte: " + std::to_string(byte)};
}

enum class MQTTVersion
{
    V4 = 4,
    V5 = 5
};


enum class WorkerThreadSleepLevel : int
{
    YIELD,
    MICROSECONDS,
    MILLISECONDS,
    TENS_OF_MILLISECONDS,
    $COUNT
};

static inline const char* workerThreadSleepLevelToString(WorkerThreadSleepLevel level) {
    switch(level) {
    case WorkerThreadSleepLevel::YIELD:
        return "yield";
    case WorkerThreadSleepLevel::MICROSECONDS:
        return "microseconds";
    case WorkerThreadSleepLevel::MILLISECONDS:
        return "milliseconds";
    case WorkerThreadSleepLevel::TENS_OF_MILLISECONDS:
        return "tens_of_milliseconds";
    case WorkerThreadSleepLevel::$COUNT:
        return "<count\?\?\?>";
    }
    assert(false);
    return "";
}

}