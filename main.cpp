
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

#include "session_commands.hpp"
#include "authentication_commands.hpp"
#include "context.hpp"

using namespace cli;

metriffic::Context context;

void signal_callback_handler(int signum) 
{
    if(context.session.IsRunningCommand()) {        
        context.gql_manager.stop();
    } else
    if(context.session.Current()->Parent()) {
        context.session.SetCurrent(context.session.Current()->Parent());
        context.session.OutStream()<<std::endl;
        context.session.Prompt();
    } else {
        std::cout<<"\rUse exit to quit..."<<std::endl;
    }
}


int main(int argc, char** argv)
{
    signal(SIGINT, signal_callback_handler);

    const std::string URI = "http://localhost:4000/graphql";
    context.start_communication(URI);

    context.cli.RootMenu() -> Insert(
            "show",
            [](std::ostream& out, const std::string& what){
                if(what == "platforms") {
                    int msg_id = context.gql_manager.query_platforms();
                    auto response = context.gql_manager.wait_for_response(msg_id);
                    nlohmann::json show_msg = response.second;
                    if(show_msg["payload"]["data"] != nullptr) {
                        std::cout<<"response: "<<show_msg["payload"]["data"]<<std::endl;
                    } else 
                    if(show_msg["payload"]["errors"] != nullptr ) {
                        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
                    }
                }
            },
            "Show supported platforms" );


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

    metriffic::session_commands current_session(context);
    context.cli.RootMenu() -> Insert(current_session.create_menu());

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

