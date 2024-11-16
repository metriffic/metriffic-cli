#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <cli/clilocalsession.h> // include boost asio
#include <cli/cli.h>
#include <semver.hpp>
#include <nlohmann/json.hpp>
#include "gql_connection_manager.hpp"
#include "settings_manager.hpp"
#include "ssh_manager.hpp"


namespace metriffic
{

struct app_context
{
    static constexpr semver::version version{0, 0, 2, semver::prerelease::rc, 2};
    static constexpr semver::version api_version{0, 0, 1};

    app_context();
    app_context(const app_context&) = delete;
    
    void start_communication(const std::string& URI);

    void logged_in(const std::string& username, const std::string& password);
    void logged_out();
    bool is_logged_in() const;

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

    metriffic::settings_manager settings;

    metriffic::ssh_manager ssh;

    std::string username;
    std::string token;

private:
    std::thread gql_manager_thread;
};

} // namespace metriffic

#endif //CONTEXT_HPP