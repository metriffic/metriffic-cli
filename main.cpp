
#include <nlohmann/json.hpp>
#include <semver.hpp>
#include <cxxopts.hpp>
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

#include "session_commands.hpp"
#include "authentication_commands.hpp"
#include "query_commands.hpp"
#include "workspace_commands.hpp"
#include "admin_commands.hpp"
//#include "test_commands.hpp"
#include "context.hpp"

using namespace cli;

metriffic::Context context;

void sigint_callback_handler(int signum) 
{
    if(context.session.RunningCommand()) {        
        context.session.CancelRunningCommand();
        context.gql_manager.stop_waiting_for_response(); 
    } else
    if(context.session.CurrentMenu()->Parent()) {
        context.session.SetCurrentMenu(context.session.CurrentMenu()->Parent());
        context.session.OutStream()<<std::endl;
        context.session.Prompt();
    } else {
        context.session.OutStream()<<"\nuse exit to quit..."<<std::endl;
        context.session.reset_input();
        context.session.Prompt();   
    }             
}

void sigpipe_callback_handler(int signum) 
{
}

void validate_handshake()
{
    auto handshake = context.gql_manager.wait_for_handshake();
   
    const auto server_api_version = handshake["api_version"].is_null() ? "unknown" : handshake["api_version"].get<std::string>();
    if(server_api_version == "unknown" || context.api_version != semver::version(server_api_version)) {
        std::cout<<"\rsupported API version ("<<context.api_version.to_string()<<") doesn't match the version of the back-end ("
                 <<server_api_version<<"), please update the tool..."<<std::endl;
        context.session.Exit();        
    }
}

void process_command_line(int argc, char** argv)
{
    try {
        cxxopts::Options options(argv[0], " - command line options");
        options.add_options()
            ("v,version", "Print version information and exit.")
            ("g,generate-keys", "Generate pairs of keys for user authentication.", cxxopts::value<std::string>());

        auto result = options.parse(argc, argv);

        if (result.count("version")) {
            std::cout << "\rcli tool:    " << context.version << std::endl;
            // std::cout << "\rbackend-end: "<< context.api_version << std::endl;
            exit(0);
        }
        if (result.count("generate-keys")) {
            const std::string username = result["generate-keys"].as<std::string>();
            bool status = true;
            std::string error = "";
            std::tie(status, error) = context.settings.generate_keys(username);
            std::cout << "successfully generated bastion and user keys for username "<< username << ":" << std::endl;
            std::cout << "   bastion keys: " << context.settings.bastion_key_file(username)<<"{.pub}"<<std::endl;
            std::cout << "   user keys: " << context.settings.user_key_file(username)<<"{.pub}"<<std::endl;
            exit(0);
        }
    } 
    catch (const cxxopts::exceptions::exception& e) {
        std::cout << "\rerror parsing options: " << e.what() << std::endl;
        exit(1);
    }
}

void setup_logger() 
{
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
}


int main(int argc, char** argv)
{
    signal(SIGINT, sigint_callback_handler);
    signal(SIGPIPE, sigpipe_callback_handler);

    process_command_line(argc, argv);

#ifdef TEST_MODE
    const std::string URI = "ws://127.0.0.1:4000/graphql";
#else
    const std::string URI = "wss://api.metriffic.com/graphql";
#endif
    context.start_communication(URI);
    context.session.ExitAction(
        [](auto& out) // session exit action
        {
            context.gql_manager.stop();
            context.session.disable_input();
            context.ios.stop();
        }
    );

    validate_handshake();

    setup_logger();

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
    context.cli.RootMenu() -> Insert(session_cmds.create_interactive_cmd());
    context.cli.RootMenu() -> Insert(session_cmds.create_batch_cmd());

    metriffic::workspace_commands workspace_cmds(context);
    context.cli.RootMenu() -> Insert(workspace_cmds.create_sync_cmd());

    metriffic::admin_commands admin_cmds(context);
    context.cli.RootMenu() -> Insert(admin_cmds.create_admin_cmd());

#if BOOST_VERSION < 106600
    boost::asio::io_service::work work(context.ios);
#else
    auto work = boost::asio::make_work_guard(context.ios);
#endif    

    //metriffic::test_commands test_cmds(context);
    //context.cli.RootMenu() -> Insert(test_cmds.create_test_cmd());

    context.ios.run();

    return 0;
}

