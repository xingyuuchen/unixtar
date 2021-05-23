#include "applicationlayer.h"
#include <utility>


ApplicationProtocolParser::ApplicationProtocolParser(ApplicationPacket::Ptr _packet,
                                                     AutoBuffer *_buff)
        : application_packet_(std::move(_packet))
        , buffer_(_buff) {
}

void ApplicationProtocolParser::SetPacketToParse(ApplicationPacket::Ptr _packet) {
    application_packet_ = std::move(_packet);
    OnApplicationPacketChanged(application_packet_);
}

bool ApplicationProtocolParser::IsUpgradeProtocol() const {
    return false;
}

TApplicationProtocol ApplicationProtocolParser::ProtocolUpgradeTo() {
    return TApplicationProtocol::kNone;
}

void ApplicationProtocolParser::OnApplicationPacketChanged(
        const ApplicationPacket::Ptr& _new) {
}

void ApplicationProtocolParser::Reset() {
    // implemented by long link application protocol.
}

ApplicationProtocolParser::~ApplicationProtocolParser() = default;


ApplicationPacket::ApplicationPacket() = default;

ApplicationPacket::~ApplicationPacket() = default;

bool ApplicationPacket::IsLongLink() const {
    TApplicationProtocol proto = Protocol();
    if (proto == kWebSocket) {
        return true;
    }
    // we use http as short link protocol.
    return false;
}

ApplicationPacket::Ptr ApplicationPacket::AllocNewPacket() {
    /**
     * Implemented by long link application protocol,
     * because we cannot reuse the Packet object in such condition:
     *
     *      The new packet has been parsed successfully while the
     *      last parsed packet has not been processed in
     *      the same tcp connection.
     *
     * Otherwise, A collision between to packets occurs.
     */
    return nullptr;
}

