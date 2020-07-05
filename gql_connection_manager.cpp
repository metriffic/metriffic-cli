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
 : m_msg_id(0),
   m_should_stop(false)
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
    m_should_stop = true;
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
gql_connection_manager::set_authentication_data(const std::string& username, const std::string& token)
{
    m_username = username;
    m_token = token;
}

int 
gql_connection_manager::registr(const std::string& username, const std::string& email,
                                const std::string& password, const std::string& repassword)
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "mutation {register(username: \""
        << username
        << "\" email: \""
        << email
        << "\" password: \""
        << password
        << "\" cpassword: \""
        << repassword
        << "\") {id, username token}}";
    json register_msg = {
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
    m_connection->send(register_msg.dump(), websocketpp::frame::opcode::text);
    return id;
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
        << "\") {id username token}}";
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

int
gql_connection_manager::query_docker_images(const std::string& platform) 
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "query{ allDockerImages(platformName: \""<< platform <<"\") { id name platform{name} } }";
    json alldockerimages_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(alldockerimages_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

int
gql_connection_manager::query_sessions(const std::string& platform, 
                                       const std::vector<std::string>& statuses) 
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "query{ allSessions(platformName: \""<< platform <<"\" "
       << "status: [ ";
    for(const auto& s : statuses) {
        ss << "\"" << s << "\", ";
    }   
    ss << "]) { id name state } }";
    json allsessions_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(allsessions_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

int
gql_connection_manager::query_jobs(const std::string& platform, const std::string& session) 
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "query{ allJobs(platformName: \""<< platform <<"\" ";
    ss << "sessionName: \""<< session <<"\")";
    ss << " { id name } }";
    json alljobs_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(alljobs_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}


int
gql_connection_manager::subscribe_to_data_stream()
{
    int id = m_msg_id++;
    std::stringstream ss;
    ss << "subscription{ subsData {message} }";
    json allplatforms_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(allplatforms_msg.dump(), websocketpp::frame::opcode::text);
    return id;    
}



int 
gql_connection_manager::session_start(const std::string& name,
                                      const std::string& platform,
                                      const std::string& type,
                                      const std::vector<std::string>& datasets,
                                      int max_jobs,
                                      std::string& command)
{
    int id = m_msg_id++;

    std::stringstream ss;
    ss << "mutation{ sessionCreate (platformId: 2 name: \"" << name << "\"";
    ss << " type: \"" << type << "\"";
    ss << " dockerImageId: 1 max_jobs: " << max_jobs;
    ss << " datasets: \"[]\" command: \"[]\")";
    ss << " { name, id, user{username}, dockerImage{name} } }";
    json sstart_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(sstart_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

int 
gql_connection_manager::session_stop(const std::string& name)
{
    int id = m_msg_id++;

    std::stringstream ss;
    ss << "mutation{ sessionUpdate ( name: \"" << name << "\" state: \"CANCELED\" ) {id, name} }";
    
    json sstop_msg = {
        {"id", id},
        {"type", "start"},
        {"payload", {            
            {"authorization", m_token.empty() ? "" : "Bearer " + m_token}, 
            {"endpoint", "cli"},            
            {"variables", {}},
            {"extensions", {}},
            {"operationName", {}},
            {"query", ss.str()}
            }
        },
    };
    m_connection->send(sstop_msg.dump(), websocketpp::frame::opcode::text);
    return id;
}

std::pair<bool, nlohmann::json> 
gql_connection_manager::wait_for_response(int msg_id)
{
    nlohmann::json response;
    while(true) {
        if(m_should_stop) {
            m_should_stop = false;
            return std::make_pair(true, nlohmann::json()) ;
        }
        std::list<nlohmann::json> incoming_messages;
        pull_incoming_messages(incoming_messages);
        if(!incoming_messages.empty()) {
            for(auto msg : incoming_messages) {
                //std::cout<<"msg "<<msg.dump(4)<<std::endl;
                if(msg["id"] == msg_id) {                    
                    return std::make_pair(false, msg) ;
                }
            }                       
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // Note: it should never get here...
    return std::make_pair(false, nlohmann::json()) ;
}


} // namespace metriffic

