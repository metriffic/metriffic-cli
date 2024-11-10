#ifndef GQL_CONNECTION_MANAGER_HPP
#define GQL_CONNECTION_MANAGER_HPP

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <list>

namespace metriffic
{

class gql_connection_manager
{
public:
    typedef gql_connection_manager type;
#ifdef TEST_MODE
    typedef websocketpp::client<websocketpp::config::asio_client> client;
#else
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
#endif
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
    typedef client::connection_ptr connection_ptr;
    typedef std::function<void(const std::string& msg)> ext_handler_type;

    gql_connection_manager();
    void start(const std::string& uri);
    void stop();
    void pull_incoming_messages(std::list<nlohmann::json>& messages);

    void on_socket_init(websocketpp::connection_hdl);
#ifndef TEST_MODE
    context_ptr on_tls_init(websocketpp::connection_hdl);
#endif
    void on_fail(websocketpp::connection_hdl hdl);
    void on_open(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
    void on_close(websocketpp::connection_hdl);

    void set_ext_on_close_handler(ext_handler_type ext_on_close);
    void set_ext_on_fail_handler(ext_handler_type ext_on_fail);

private:
    void init_connection();    

public:
    void send_handshake();
    nlohmann::json wait_for_handshake();

    void set_authentication_data(const std::string& token);

    int registr(const std::string& username, const std::string& email,
                const std::string& password, const std::string& repassword);
    int login(const std::string& username, const std::string& token);
    int logout();

    int query_platforms();
    int query_docker_images(const std::string& platform);
    int query_sessions(const std::string& platform, 
                       const std::vector<std::string>& statuses);
    int query_jobs(const std::string& platform, const std::string& session);

    int session_start(const std::string& name,
                      const std::string& platform,
                      const std::string& type,
                      const std::string& dockerimage,
                      const std::vector<std::string>& datasets,
                      int max_jobs,
                      const std::string& command);
    int session_join(const std::string& name);
    int session_stop(const std::string& name);
    int session_save(const std::string& name, const std::string& dockerimage, const std::string& comment);
    int session_status(const std::string& name);

    int admin_diagnostics();

    int sync_request();
    int subscribe_to_data_stream();

    void stop_waiting_for_response();
    std::pair<bool, nlohmann::json> wait_for_response(int msg_id);
    std::pair<bool, std::list<nlohmann::json>> wait_for_response(const std::set<int>& msg_ids);

private:
    ext_handler_type ext_on_close_cb;
    ext_handler_type ext_on_fail_cb;

private:
    client m_endpoint;
    client::connection_ptr m_connection;
    
    std::string     m_token;

    std::mutex      m_mutex;

    const int       m_handshake_msg_id = 0;
    int             m_msg_id;
    
    std::list<nlohmann::json> m_incoming_messages;

    bool m_should_stop;
};

} // namespace metriffic

#endif // GQL_CONNECTION_MANAGER_HPP
