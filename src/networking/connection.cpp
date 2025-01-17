#include "fmt/format.h"
#include "steamclientpublic.h"
#include "steamnetworkingtypes.h"
#include "steamtypes.h"
#include <networking/connection.hpp>
#include <stdexcept>


SteamConnection::SteamConnection(ISteamNetworkingSockets *networkingSockets, HSteamNetConnection conn) {
    m_NetworkingSockets = networkingSockets;
    m_Connection = conn;
}

SteamConnection::~SteamConnection() {
    m_NetworkingSockets->CloseConnection(m_Connection, 0, nullptr, true);
}

int64 SteamConnection::SendMessage(std::vector<std::byte> &message, int sendFlags) {
    int64 messageNumber;

    EResult result = m_NetworkingSockets->SendMessageToConnection(m_Connection, message.data(), message.size(), sendFlags, &messageNumber);

    if (result != k_EResultOK) {
        throw std::runtime_error(fmt::format("Steam Networking error: {}", (int)result));
    }

    return messageNumber;
}

std::vector<std::vector<std::byte>> SteamConnection::ReceiveMessages(int maxMessages) {
    std::vector<std::vector<std::byte>> messages;

    SteamNetworkingMessage_t *networkingMessages;

    int messageCount = m_NetworkingSockets->ReceiveMessagesOnConnection(m_Connection, &networkingMessages, maxMessages);

    if (messageCount < 0) {
        throw std::runtime_error(fmt::format("Failed to receive message on connection {}!", (uint32)m_Connection));
    }

    for (int i = 0; i < messageCount; i++) {
        SteamNetworkingMessage_t *networkingMessage = &networkingMessages[i];
        std::vector<std::byte> message{reinterpret_cast<const std::byte *>(networkingMessage->GetData()), reinterpret_cast<const std::byte *>(networkingMessage->GetData()) + networkingMessage->GetSize()};

        messages.push_back(message);

        networkingMessages->Release();
    }

    return messages;
}