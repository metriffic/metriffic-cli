
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>
#include <chrono>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;


static const std::string GQL_CONNECTION_INIT = "connection_init"; // Client -> Server
static const std::string GQL_CONNECTION_ACK = "connection_ack"; // Server -> Client
static const std::string GQL_CONNECTION_ERROR = "connection_error"; // Server -> Client

// NOTE: The keep alive message type does not follow the standard due to connection optimizations
static const std::string GQL_CONNECTION_KEEP_ALIVE = "ka"; // Server -> Client

static const std::string GQL_CONNECTION_TERMINATE = "connection_terminate"; // Client -> Server
static const std::string GQL_START = "start"; // Client -> Server
static const std::string GQL_DATA = "data"; // Server -> Client
static const std::string GQL_ERROR = "error"; // Server -> Client
static const std::string GQL_COMPLETE = "complete"; // Server -> Client
static const std::string GQL_STOP = "stop"; // Client -> Server


client::connection_ptr blacon;

class perftest {
public:
    typedef perftest type;
    typedef std::chrono::duration<int,std::micro> dur_type;

    perftest () {
        m_endpoint.set_access_channels(websocketpp::log::alevel::none);
        m_endpoint.set_error_channels(websocketpp::log::elevel::none);

        // Initialize ASIO
        m_endpoint.init_asio();

        // Register our handlers
        m_endpoint.set_socket_init_handler(bind(&type::on_socket_init,this,::_1));
        //m_endpoint.set_tls_init_handler(bind(&type::on_tls_init,this,::_1));
        m_endpoint.set_message_handler(bind(&type::on_message,this,::_1,::_2));
        m_endpoint.set_open_handler(bind(&type::on_open,this,::_1));
        m_endpoint.set_close_handler(bind(&type::on_close,this,::_1));
        m_endpoint.set_fail_handler(bind(&type::on_fail,this,::_1));
    }

    void start(std::string uri) {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);
        std::cout<<"Z: on_start"<<std::endl;

        if (ec) {
            m_endpoint.get_alog().write(websocketpp::log::alevel::app,ec.message());
            return;
        }
        con->add_subprotocol("graphql-ws");

        blacon = con;

        m_endpoint.connect(con);

        // Start the ASIO io_service run loop
        m_start = std::chrono::high_resolution_clock::now();
        m_endpoint.run();
    }

    void on_socket_init(websocketpp::connection_hdl) {
        m_socket_init = std::chrono::high_resolution_clock::now();
    }

    context_ptr on_tls_init(websocketpp::connection_hdl) {
        m_tls_init = std::chrono::high_resolution_clock::now();
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);

        std::cout<<"Z: on_tlks_init"<<std::endl;
        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::no_sslv3 |
                             boost::asio::ssl::context::single_dh_use);
        } catch (std::exception& e) {
            std::cout<<"Z: on_tlks_init exception"<<std::endl;
            std::cout << e.what() << std::endl;
        }
        return ctx;
    }

    void on_fail(websocketpp::connection_hdl hdl) {
        client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);
        
        std::cout << "Fail handler" << std::endl;
        std::cout << con->get_state() << std::endl;
        std::cout << con->get_local_close_code() << std::endl;
        std::cout << con->get_local_close_reason() << std::endl;
        std::cout << con->get_remote_close_code() << std::endl;
        std::cout << con->get_remote_close_reason() << std::endl;
        std::cout << con->get_ec() << " - " << con->get_ec().message() << std::endl;
    }

    void on_open(websocketpp::connection_hdl hdl) {
        std::cout<<"Z: on_open"<<std::endl;
        m_open = std::chrono::high_resolution_clock::now();


        const std::string msg1 = "{\"type\":\"connection_init\",\"payload\":{}}";
        blacon->send(msg1, websocketpp::frame::opcode::text);

        //const std::string msg2 = "{\"id\":\"1\",\"type\":\"start\",\"payload\":{\"variables\":{},\"extensions\":{},\"operationName\":null,\"query\":\"subscription {\\n  boardAdded { hostname    platform {      id    }  }}\"}}";
        //const std::string msg2 = "{\"id\":\"1\",\"type\":\"start\",\"payload\":{\"variables\":{},\"extensions\":{},\"operationName\":null,\"query\":\" query{ platform(id:2) { id name spec } } \"}}";
        const std::string msg2 = "{\"id\":\"1\",\"type\":\"start\",\"payload\":{\"variables\":{},\"extensions\":{},\"operationName\":null,\"query\":\"mutation{createBoard(platformId:2, hostname:\\\"metriffic-coral-2\\\", spec:\\\"\\\") { id hostname, spec, platform {id} } }\"}}";
        blacon->send(msg2, websocketpp::frame::opcode::text);
    }
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
        std::cout<<"Z: on_message "<<msg->get_payload()<<std::endl;
        m_message = std::chrono::high_resolution_clock::now();
        //m_endpoint.close(hdl,websocketpp::close::status::going_away,"");
    }
    void on_close(websocketpp::connection_hdl) {
        std::cout<<"Z: on_close"<<std::endl;

        m_close = std::chrono::high_resolution_clock::now();

        std::cout << "Socket Init: " << std::chrono::duration_cast<dur_type>(m_socket_init-m_start).count() << std::endl;
        std::cout << "TLS Init: " << std::chrono::duration_cast<dur_type>(m_tls_init-m_start).count() << std::endl;
        std::cout << "Open: " << std::chrono::duration_cast<dur_type>(m_open-m_start).count() << std::endl;
        std::cout << "Message: " << std::chrono::duration_cast<dur_type>(m_message-m_start).count() << std::endl;
        std::cout << "Close: " << std::chrono::duration_cast<dur_type>(m_close-m_start).count() << std::endl;
    }
private:
    client m_endpoint;

    std::chrono::high_resolution_clock::time_point m_start;
    std::chrono::high_resolution_clock::time_point m_socket_init;
    std::chrono::high_resolution_clock::time_point m_tls_init;
    std::chrono::high_resolution_clock::time_point m_open;
    std::chrono::high_resolution_clock::time_point m_message;
    std::chrono::high_resolution_clock::time_point m_close;
};

int main(int argc, char* argv[]) {
    std::string uri = "http://localhost.org:4000/graphql";
    //std::string uri = "ws://localhost.org:8126/graphql";

    if (argc == 2) {
        uri = argv[1];
    }

    try {
        perftest endpoint;
        endpoint.start(uri);
                
        while(true) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(3000));
        }

    } catch (websocketpp::exception const & e) {
        std::cout << "A: " << e.what() << std::endl;
    } catch (std::exception const & e) {
        std::cout << "B: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "C: other exception" << std::endl;
    }
}
