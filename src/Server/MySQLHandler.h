#pragma once

#include <Core/MySQL/Authentication.h>
#include <Core/MySQL/PacketsConnection.h>
#include <Core/MySQL/PacketsGeneric.h>
#include <Core/MySQL/PacketsProtocolText.h>
#include <base/getFQDNOrHostName.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Common/CurrentMetrics.h>
#include "IServer.h"

#include "config.h"

#if USE_SSL
#    include <Poco/Net/SecureStreamSocket.h>
#endif

#include <memory>

namespace CurrentMetrics
{
    extern const Metric MySQLConnection;
}

namespace DB
{
class ReadBufferFromPocoSocket;
class TCPServer;

/// Handler for MySQL wire protocol connections. Allows to connect to ClickHouse using MySQL client.
class MySQLHandler : public Poco::Net::TCPServerConnection
{
public:
    MySQLHandler(
        IServer & server_,
        TCPServer & tcp_server_,
        const Poco::Net::StreamSocket & socket_,
        bool ssl_enabled,
        uint32_t connection_id_);

    void run() final;

protected:
    CurrentMetrics::Increment metric_increment{CurrentMetrics::MySQLConnection};

    /// Enables SSL, if client requested.
    void finishHandshake(MySQLProtocol::ConnectionPhase::HandshakeResponse &);

    void comQuery(ReadBuffer & payload, bool use_binary_protocol_result_set);

    void comFieldList(ReadBuffer & payload);

    void comPing();

    void comInitDB(ReadBuffer & payload);

    void authenticate(const String & user_name, const String & auth_plugin_name, const String & auth_response);

    void comStmtPrepare(ReadBuffer & payload);

    void comStmtExecute(ReadBuffer & payload);

    void comStmtClose(ReadBuffer & payload);

    virtual void authPluginSSL();
    virtual void finishHandshakeSSL(size_t packet_size, char * buf, size_t pos, std::function<void(size_t)> read_bytes, MySQLProtocol::ConnectionPhase::HandshakeResponse & packet);

    IServer & server;
    TCPServer & tcp_server;
    Poco::Logger * log;
    uint32_t connection_id = 0;

    uint32_t server_capabilities = 0;
    uint32_t client_capabilities = 0;
    size_t max_packet_size = 0;
    uint8_t sequence_id = 0;

    MySQLProtocol::PacketEndpointPtr packet_endpoint;
    std::unique_ptr<Session> session;

    using ReplacementFn = std::function<String(const String & query)>;
    using Replacements = std::unordered_map<std::string, ReplacementFn>;
    Replacements replacements;

    uint32_t current_prepared_statement_id = 0;
    using PreparedStatementsMap = std::unordered_map<uint32_t, String>;
    PreparedStatementsMap prepared_statements_map;

    std::unique_ptr<MySQLProtocol::Authentication::IPlugin> auth_plugin;
    std::shared_ptr<ReadBufferFromPocoSocket> in;
    std::shared_ptr<WriteBuffer> out;
    bool secure_connection = false;
};

#if USE_SSL
class MySQLHandlerSSL : public MySQLHandler
{
public:
    MySQLHandlerSSL(
        IServer & server_,
        TCPServer & tcp_server_,
        const Poco::Net::StreamSocket & socket_,
        bool ssl_enabled,
        uint32_t connection_id_,
        RSA & public_key_,
        RSA & private_key_);

private:
    void authPluginSSL() override;

    void finishHandshakeSSL(
        size_t packet_size, char * buf, size_t pos,
        std::function<void(size_t)> read_bytes, MySQLProtocol::ConnectionPhase::HandshakeResponse & packet) override;

    RSA & public_key;
    RSA & private_key;
    std::shared_ptr<Poco::Net::SecureStreamSocket> ss;
};
#endif

}
