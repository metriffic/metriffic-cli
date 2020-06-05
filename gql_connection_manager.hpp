#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <list>

namespace metriffic
{

class gql_connection_manager
{
public:
    typedef gql_connection_manager type;
    typedef websocketpp::client<websocketpp::config::asio_client> client;
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
    typedef client::connection_ptr connection_ptr;
    typedef std::function<void(const std::string& msg)> ext_handler_type;

    gql_connection_manager();
    void start(const std::string& uri);
    void stop();
    void pull_incoming_messages(std::list<nlohmann::json>& messages);

    void on_socket_init(websocketpp::connection_hdl);
    void on_fail(websocketpp::connection_hdl hdl);
    void on_open(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
    void on_close(websocketpp::connection_hdl);

    void set_ext_on_close_handler(ext_handler_type ext_on_close);
    void set_ext_on_fail_handler(ext_handler_type ext_on_fail);

private:
    void init_connection();    

public:
    void set_jwt_token(const std::string& token);
    int login(const std::string& username, const std::string& password);
    int query_platforms();

private:
    ext_handler_type ext_on_close_cb;
    ext_handler_type ext_on_fail_cb;

private:
    client m_endpoint;
    client::connection_ptr m_connection;
    
    std::string     m_token;

    std::mutex      m_mutex;
    int             m_msg_id;
    std::list<nlohmann::json> m_incoming_messages;
};

} // namespace metriffic
