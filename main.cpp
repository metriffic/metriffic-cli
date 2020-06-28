
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

#include "session_manager.hpp"
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

int capture_char() {
    int ch;
    struct termios t_old, t_new;
    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

int main()
{
    signal(SIGINT, signal_callback_handler);

    const std::string URI = "http://localhost:4000/graphql";
    context.start_communication(URI);

    context.cli.RootMenu() -> Insert(
            "login",
            [](std::ostream& out){ 
                context.session.disable_input();                
                std::string username;
                std::cout << "Enter login: ";
                std::cin >> username; 
                std::cout << "Enter password: ";
                // read the spurious return-char at the end 
                unsigned char ch = getchar();
                std::string password;
                while((ch = capture_char()) != 10) {
                    if(ch == 127) {
                        if(password.length() != 0) {
                            std::cout << "\b \b";
                            password.resize(password.length()-1);
                        }
                    } else {
                        password += ch;
                        std::cout << '*';
                    }
                }
                std::cout << std::endl;
                int msg_id = context.gql_manager.login(username, password);
                context.session.enable_input();

                auto response = context.gql_manager.wait_for_response(msg_id);
                nlohmann::json& login_msg  = response.second;
               if(login_msg["payload"]["data"] != nullptr) {
                    std::cout<<"Login successful!"<<std::endl;
                    context.logged_in(login_msg["payload"]["data"]["login"]);
                } else 
                if(login_msg["payload"]["errors"] != nullptr ) {
                    std::cout<<"Login failed: "<<login_msg["payload"]["errors"][0]["message"]<<std::endl;
                    context.logged_out();
                }
            },
            "Log in to metriffic service" );

    context.cli.RootMenu() -> Insert(
            "logout",
            [](std::ostream& out){
                    int msg_id = context.gql_manager.logout();
                    auto response = context.gql_manager.wait_for_response(msg_id);
                    nlohmann::json& logout_msg  = response.second;

                    if(logout_msg["payload"]["data"] != nullptr) {
                        std::cout<<"User "<<logout_msg["payload"]["data"]["logout"]<<" has successfully logged out..."<<std::endl;
                        context.logged_out();
                    } else 
                    if(logout_msg["payload"]["errors"] != nullptr ) {
                        std::cout<<"Failed to log out: "<<logout_msg["payload"]["errors"][0]["message"]<<std::endl;
                    }
            },
            "Log out from the service" );

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
    
    metriffic::session_manager current_session(context);
    auto session_menu = current_session.create_menu();

    context.cli.RootMenu() -> Insert(session_menu);

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

