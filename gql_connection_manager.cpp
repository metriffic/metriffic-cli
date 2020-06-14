#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

#include "gql_connection_manager.hpp"

#include <nlohmann/json.hpp>

namespace metriffic 
{

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using nlohmann::json;

namespace gql_consts
{
    static const std::string CONNECTION_INIT = "connection_init"; // Client -> Server
    static const std::string CONNECTION_ACK = "connection_ack"; // Server -> Client
    static const std::string CONNECTION_ERROR = "connection_error"; // Server -> Client

    // NOTE: The keep alive message type does not follow the standard due to connection optimizations
    static const std::string CONNECTION_KEEP_ALIVE = "ka"; // Server -> Client

    static const std::string CONNECTION_TERMINATE = "connection_terminate"; // Client -> Server
    static const std::string START = "start"; // Client -> Server
    static const std::string DATA = "data"; // Server -> Client
    static const std::string ERROR = "error"; // Server -> Client
    static const std::string COMPLETE = "complete"; // Server -> Client
    static const std::string STOP = "stop"; // Client -> Server
};

gql_connection_manager::gql_connection_manager() 
 : m_msg_id(0)
{
    m_endpoint.set_access_channels(websocketpp::log::alevel::none);
    m_endpoint.set_error_channels(websocketpp::log::elevel::none);

    // Initialize ASIO
    m_endpoint.init_asio();

    // Register our handlers
    m_endpoint.set_socket_init_handler(bind(&type::on_socket_init,this,::_1));
    m_endpoint.set_message_handler(bind(&type::on_message,this,::_1,::_2));
    m_endpoint.set_open_handler(bind(&type::on_open,this,::_1));
    m_endpoint.set_close_handler(bind(&type::on_close,this,::_1));
    m_endpoint.set_fail_handler(bind(&type::on_fail,this,::_1));
}

void 
gql_connection_manager::set_ext_on_close_handler(ext_handler_type ext_on_close)
{
    ext_on_close_cb = ext_on_close;
}

void 
gql_connection_manager::set_ext_on_fail_handler(ext_handler_type ext_on_fail)
{
    ext_on_fail_cb = ext_on_fail;
}


void 
gql_connection_manager::start(const std::string& uri) 
{
    websocketpp::lib::error_code ec;
    m_connection = m_endpoint.get_connection(uri, ec);
    //std::cout<<"Z: on_start"<<std::endl;

    if (ec) {
        m_endpoint.get_alog().write(websocketpp::log::alevel::app,ec.message());
        return;
    }
    m_connection->add_subprotocol("graphql-ws");
    m_endpoint.connect(m_connection);
    // start the ASIO io_service run loop
    m_endpoint.run();
}

void
gql_connection_manager::stop()
{
}

void 
gql_connection_manager::pull_incoming_messages(std::list<json>& messages)
{
    messages.clear();
    if(m_mutex.try_lock()) {
        m_incoming_messages.swap(messages);
        m_mutex.unlock();
    }
}

void 
gql_connection_manager::on_socket_init(websocketpp::connection_hdl) 
{
}

void 
gql_connection_manager::on_fail(websocketpp::connection_hdl hdl) 
{
    client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

    if(ext_on_fail_cb) {
        ext_on_fail_cb(con->get_local_close_reason());
    }
}

void 
gql_connection_manager::on_open(websocketpp::connection_hdl hdl) 
{
    init_connection();
}

void 
gql_connection_manager::on_message(websocketpp::connection_hdl hdl, message_ptr msg) 
{
    json jmsg = json::parse(msg->get_payload());
    //std::cout<<"msg: "<<jmsg.dump(4)<<std::endl;
    std::lock_guard<std::mutex> guard(m_mutex);
    m_incoming_messages.push_back(std::move(jmsg));
}

void 
gql_connection_manager::on_close(websocketpp::connection_hdl) 
{
    if(ext_on_close_cb) {
        ext_on_close_cb("");
    }
}

void 
gql_connection_manager::init_connection() 
{
    json init_msg = {
        {"type", "connection_init"},
        {"payload", {}},
    };
    m_connection->send(init_msg.dump(), websocketpp::frame::opcode::text);
}

void 
gql_connection_manager::set_jwt_token(const std::string& token)
{
    m_token = token;
}

int 
gql_connection_manager::login(const std::string& username, const std::string& password)
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "mutation {login(username: \""
        << username
        << "\" password: \""
        << password
        << "\") {username email role createdAt isEnabled lastLoggedInAt currentState token}}";
    json login_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {
            {"variables" , {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(login_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

int 
gql_connection_manager::logout()
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "mutation {logout}";
    json logout_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token},  
            {"endpoint", "cli"},
            {"variables" , {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(logout_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}


int
gql_connection_manager::query_platforms() 
{
    int id = m_msg_id++;
    json allplatforms_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", "query{ allPlatforms { id name description } }"}
            }
        },
    };
    m_connection->send(allplatforms_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

} // namespace metriffic

