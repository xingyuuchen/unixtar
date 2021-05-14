#include "applicationpacket.h"


ApplicationProtocolParser::ApplicationProtocolParser() = default;

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
