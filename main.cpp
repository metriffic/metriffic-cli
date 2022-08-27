
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

#include "session_commands.hpp"
#include "authentication_commands.hpp"
#include "query_commands.hpp"
#include "workspace_commands.hpp"
#include "admin_commands.hpp"
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
        std::cout<<"\nuse exit to quit..."<<std::endl;
        //context.session.reset_input();
        context.session.Prompt();   
    }             
}

void sigpipe_callback_handler(int signum) 
{
}

int main(int argc, char** argv)
{
    signal(SIGINT, sigint_callback_handler);
    signal(SIGPIPE, sigpipe_callback_handler);

    struct log_formatter
    {
    public:
        static plog::util::nstring header() {
            return plog::util::nstring();
        }
        static plog::util::nstring format(const plog::Record& record) {
            tm t;
            plog::util::localtime_s(&t, &record.getTime().time);
            plog::util::nostringstream ss;
            ss << t.tm_year + 1900 << "-" << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_mon + 1 << PLOG_NSTR("-") << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_mday << PLOG_NSTR(" ");
            ss << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_hour << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_min << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_sec << PLOG_NSTR(" ");
            ss << std::setfill(PLOG_NSTR(' ')) << std::setw(5) << std::left << severityToString(record.getSeverity()) << PLOG_NSTR(" ");
            ss << record.getMessage() << PLOG_NSTR("\n");

            return ss.str();
        }
    };
    std::string log_file =  context.settings.log_file();
    std::string token = context.settings.active_user().second;
    context.gql_manager.set_authentication_data(token);

    plog::init<log_formatter>(plog::verbose, log_file.c_str(), 1000000, 2); 
    PLOGV << "starting metriffic cli.";

#ifdef TEST_MODE
    const std::string URI = "ws://127.0.0.1:4000/graphql";
#else
    const std::string URI = "wss://api.metriffic.com/graphql";
#endif
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
                        out<<"data stream error..."<<std::endl;
                    } else {
                        break;
                    }
                }
            },
            "stream debug messages from the server..." );

    metriffic::authentication_commands auth_cmds(context);
    context.cli.RootMenu() -> Insert(auth_cmds.create_login_cmd());
    context.cli.RootMenu() -> Insert(auth_cmds.create_logout_cmd());

    metriffic::query_commands query_cmds(context);
    context.cli.RootMenu() -> Insert(query_cmds.create_show_cmd());

    metriffic::session_commands session_cmds(context);
    context.cli.RootMenu() -> Insert(session_cmds.create_session_cmd());

    metriffic::workspace_commands workspace_cmds(context);
    context.cli.RootMenu() -> Insert(workspace_cmds.create_sync_cmd());

    metriffic::admin_commands admin_cmds(context);
    context.cli.RootMenu() -> Insert(admin_cmds.create_admin_cmd());

    context.session.ExitAction(
        [](auto& out) // session exit action
        {
            context.gql_manager.stop();
            context.session.disable_input();
            context.ios.stop();
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

