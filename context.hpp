#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <cli/clilocalsession.h> // include boost asio
#include <cli/cli.h>
#include <nlohmann/json.hpp>
#include "gql_connection_manager.hpp"


namespace metriffic
{

struct Context
{
    Context();
    Context(const Context&) = delete;
    
    void start_communication(const std::string& URI);

    void logged_in(const nlohmann::json& data);
    void logged_out();

    void on_connection_close(); 
    void on_connection_fail(const std::string& reason); 
    
#if BOOST_VERSION < 106600
    boost::asio::io_service ios;
#else
    boost::asio::io_context ios;
#endif    // command-line stuff
    std::unique_ptr<cli::Menu> root_menu;
    cli::Cli cli;
    cli::CliLocalTerminalSession session;

    // GQL/Metriffic service related
    metriffic::gql_connection_manager gql_manager;

private:
    std::thread gql_manager_thread;
};

} // namespace metriffic

#endif //CONTEXT_HPP