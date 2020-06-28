#include <cli/clilocalsession.h> // include boost asio
#include <cli/cli.h>
#include <cli/filehistorystorage.h>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

#include "context.hpp"

namespace metriffic
{

Context::Context() 
 : root_menu(std::make_unique< cli::Menu >( "metriffic" )),
   cli(std::move(root_menu), std::make_unique<cli::FileHistoryStorage>(".cli")),
   session(cli, ios, std::cout, 200)
{     
    cli.ExitAction([this](auto& out){ 
                        session.disable_prompt();
                        std::cout << "Disconnecting from the service...\n"; 
                    });
    gql_manager.set_ext_on_close_handler([this](const std::string&) { 
                            this->on_connection_close(); 
                        });
    gql_manager.set_ext_on_fail_handler([this](const std::string& msg) { 
                            this->on_connection_fail(msg); 
                        });
}

void 
Context::start_communication(const std::string& URI)
{
    gql_manager_thread = std::thread([this, &URI](){
                                        gql_manager.start(URI); 
                                    });
    gql_manager_thread.detach();
}

void 
Context::logged_in(const nlohmann::json& data) 
{
    gql_manager.set_authentication_data(data["username"], data["token"]);
}

void 
Context::logged_out()
{
    gql_manager.set_authentication_data("", "");
}

void 
Context::on_connection_close() 
{
    session.disable_input();
    ios.stop();
    gql_manager.stop();
    std::cout<<"\nConnection is closed by the server."<<std::endl;
}

void 
Context::on_connection_fail(const std::string& reason) 
{
    session.disable_input();
    ios.stop();
    gql_manager.stop();
    std::cout<<"\nFailed to connect to the server: \""<<reason<<"\""<<std::endl;
}

} // namespace metriffic