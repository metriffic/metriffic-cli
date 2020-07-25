
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

#include "session_commands.hpp"
#include "authentication_commands.hpp"
#include "query_commands.hpp"
#include "workspace_commands.hpp"
#include "context.hpp"

using namespace cli;

metriffic::Context context;

void sigint_callback_handler(int signum) 
{
    if(context.session.RunningCommand()) {        
        context.session.CancelRunningCommand();
        context.gql_manager.stop();
    } else
    if(context.session.CurrentMenu()->Parent()) {
        context.session.SetCurrentMenu(context.session.CurrentMenu()->Parent());
        context.session.OutStream()<<std::endl;
        context.session.Prompt();
    } else {
        std::cout<<"\rUse exit to quit..."<<std::endl;
    }
}
void sigpipe_callback_handler(int signum) 
{
}

int main(int argc, char** argv)
{
    signal(SIGINT, sigint_callback_handler);
    signal(SIGPIPE, sigpipe_callback_handler);

    const std::string URI = "wss://graphql.metriffic.com/graphql";
    context.start_communication(URI);

    context.cli.RootMenu() -> Insert(
            "message_stream",
            [](std::ostream& out, int argc, char** argv){ 

                cxxopts::Options options("message_stream", "connect to the server and stream debug messages...");
                options.add_options()
                    ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"));
                auto result = options.parse(argc, argv);

                int msg_id = context.gql_manager.subscribe_to_data_stream();
                while(true) {
                    auto response = context.gql_manager.wait_for_response(msg_id);
                    nlohmann::json data_msg = response.second;
                    out<<data_msg.dump(4)<<std::endl;
                    if(data_msg["payload"]["data"] != nullptr) {
                        out<<data_msg["payload"]["data"]["subsData"]["message"].get<std::string>()<<std::endl;
                    } else 
                    if(response.first) {
                        out<<"Got error in the data stream..."<<std::endl;
                    } else {
                        break;
                    }
                }
            },
            "Stream debug messages from the server..." );

    metriffic::authentication_commands auth_cmds(context);
    context.cli.RootMenu() -> Insert(auth_cmds.create_register_cmd());
    context.cli.RootMenu() -> Insert(auth_cmds.create_login_cmd());
    context.cli.RootMenu() -> Insert(auth_cmds.create_logout_cmd());

    metriffic::query_commands query_cmds(context);
    context.cli.RootMenu() -> Insert(query_cmds.create_show_cmd());

    metriffic::session_commands session_cmds(context);
    context.cli.RootMenu() -> Insert(session_cmds.create_session_cmd());

    metriffic::workspace_commands workspace_cmds(context);
    context.cli.RootMenu() -> Insert(workspace_cmds.create_sync_cmd());

    context.session.ExitAction(
        [](auto& out) // session exit action
        {
            context.session.disable_input();
            context.ios.stop();
            context.gql_manager.stop();
        }
    );

#if BOOST_VERSION < 106600
    boost::asio::io_service::work work(context.ios);
#else
    auto work = boost::asio::make_work_guard(context.ios);
#endif    

    context.ios.run();

    return 0;
}

