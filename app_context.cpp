#include <cli/clilocalsession.h> // include boost asio
#include <cli/cli.h>
#include <cli/filehistorystorage.h>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

#include "app_context.hpp"

namespace metriffic
{

app_context::app_context() 
 : root_menu(std::make_unique< cli::Menu >( "metriffic" )),
   cli(std::move(root_menu), std::make_unique<cli::FileHistoryStorage>(".cli")),
   session(cli, ios, std::cout, 200)
{     
    cli.ExitAction([this](auto& out){ 
                        session.disable_prompt();
                        std::cout << "disconnecting from the service...\n"; 
                    });
    gql_manager.set_ext_on_close_handler([this](const std::string&) { 
                            this->on_connection_close(); 
                        });
    gql_manager.set_ext_on_fail_handler([this](const std::string& msg) { 
                            this->on_connection_fail(msg); 
                        });
}

void 
app_context::start_communication(const std::string& URI)
{
    gql_manager_thread = std::thread([this, &URI](){
                                        gql_manager.start(URI); 
                                    });
    gql_manager_thread.detach();
}

void 
app_context::logged_in(const std::string& uname, const std::string& tkn) 
{
    username = uname;
    token = tkn;
    gql_manager.set_authentication_data(token);
}

void 
app_context::logged_out()
{
    gql_manager.set_authentication_data("");
    username = "";
    token = "";
}

bool 
app_context::is_logged_in() const 
{
    return username != "";
}

void 
app_context::on_connection_close() 
{
    session.disable_input();
    ios.stop();
    gql_manager.stop();
    std::cout<<"\nconnection is closed by the server."<<std::endl;
}

void 
app_context::on_connection_fail(const std::string& reason) 
{
    session.disable_input();
    ios.stop();
    gql_manager.stop();
    std::cout<<"\nfailed to connect to the server: \""<<reason<<"\""<<std::endl;
}

} // namespace metriffic