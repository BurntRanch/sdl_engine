#pragma once
#include "isteamnetworkingsockets.h"
#include "steamnetworkingtypes.h"
#include <cstddef>
#include <steam/isteamnetworkingsockets.h>
#include <vector>

/* This class holds information about a connection that was formed through Steam Networking.
 * Sending, Receiving, Disconnecting, etc. are all handled by this class.
 * Steam Networking is expected to be initialized prior to creating this class.
 *
 * This connection will automatically be closed when it goes out of scope.
 */
class SteamConnection {
public:
    SteamConnection(ISteamNetworkingSockets *networkingSockets, HSteamNetConnection conn);
    ~SteamConnection();

    bool operator==(SteamConnection const &r) const { return m_Connection == r.m_Connection; };
    bool operator==(HSteamNetConnection const &conn) const { return m_Connection == conn; };

    /* Sends a message to this connection
     * Returns: a 64-bit integer that stores the Message ID.
     * Throws: runtime_error if the message could not be sent.
     */
    int64 SendMessage(std::vector<std::byte> &message, int sendFlags);

    /* Receives `maxMessages` messages from this connection
     * Returns: a 2D array that contains all of the messages.
     * Throws: runtime_error if the message could not be received for some reason.
     */
    std::vector<std::vector<std::byte>> ReceiveMessages(int maxMessages = 1);
private:
    ISteamNetworkingSockets *m_NetworkingSockets;
    HSteamNetConnection m_Connection = k_HSteamNetConnection_Invalid;
};