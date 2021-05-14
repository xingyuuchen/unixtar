#include "applicationlayer.h"


ApplicationProtocolParser::ApplicationProtocolParser(ApplicationPacket *_packet,
                                                     AutoBuffer *_buff)
        : application_packet_(_packet)
        , buffer_(_buff) {
}

ApplicationProtocolParser::~ApplicationProtocolParser() = default;


ApplicationPacket::ApplicationPacket() = default;

ApplicationPacket::~ApplicationPacket() = default;

bool ApplicationPacket::IsLongLink() const {
    TApplicationProtocol proto = ApplicationProtocol();
    if (proto == kWebSocket) {
        return true;
    }
    // we use http as short link protocol.
    return false;
}
