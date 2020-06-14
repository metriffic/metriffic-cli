#include <cli/clilocalsession.h> // include boost asio
#include <cli/remotecli.h>
// TODO. NB: remotecli.h and clilocalsession.h both includes boost asio, 
// so in Windows it should appear before cli.h that include rang
#include <cli/cli.h>
#include <cli/filehistorystorage.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <functional> 

#include "gql_connection_manager.hpp"

using namespace cli;

struct Context
{
    Context() 
     : root_menu(std::make_unique< Menu >( "metriffic" )),
       cli(std::move(root_menu), std::make_unique<FileHistoryStorage>(".cli")),
       session(cli, ios, std::cout, 200),
       login_data({nlohmann::json(), false})
    {     
        cli.ExitAction([this](auto& out){ 
                           session.disable_prompt();
                           std::cout << "Ending the session...\n"; 
                       });
        gql_manager.set_ext_on_close_handler([this](const std::string&) { 
                                this->on_connection_close(); 
                            });
        gql_manager.set_ext_on_fail_handler([this](const std::string& msg) { 
                                this->on_connection_fail(msg); 
                            });
    }
    
    void start_communication(const std::string& URI)
    {
        gql_manager_thread = std::thread([this, &URI](){
                                            gql_manager.start(URI); 
                                        });
        gql_manager_thread.detach();
    }

    void logged_in(const nlohmann::json& data) 
    {
        login_data = {data, true};
        gql_manager.set_jwt_token(data["token"]);
    }
    void logged_out()
    {
        login_data = {nlohmann::json(), false};
    }

    void on_connection_close() 
    {
        session.disable_input();
        ios.stop();
        gql_manager.stop();
        should_stop = true;
        std::cout<<"\nConnection is closed by the server."<<std::endl;
        exit(0);
    }
    void on_connection_fail(const std::string& reason) 
    {
        session.disable_input();
        ios.stop();
        gql_manager.stop();
        should_stop = true;
        
        std::cout<<"\nFailed to connect to the server: \""<<reason<<"\""<<std::endl;
        exit(0);
    }
    


    bool should_stop = false;

#if BOOST_VERSION < 106600
    boost::asio::io_service ios;
#else
    boost::asio::io_context ios;
#endif    // command-line stuff
    std::unique_ptr<Menu> root_menu;
    Cli cli;
    CliLocalTerminalSession session;

    // GQL/Metriffic service related
    metriffic::gql_connection_manager gql_manager;
private:
    std::thread gql_manager_thread;
private:
    std::pair<nlohmann::json, bool> login_data;
};

Context context;

void signal_callback_handler(int signum) 
{
    if(context.session.IsRunningCommand()) {        
        context.should_stop = true;
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

nlohmann::json wait_for_response(int msg_id)
{
    nlohmann::json response;
    while(!context.should_stop) {
        std::list<nlohmann::json> incoming_messages;
        context.gql_manager.pull_incoming_messages(incoming_messages);
        if(!incoming_messages.empty()) {
            for(auto msg : incoming_messages) {
                //std::cout<<"msg "<<msg.dump(4)<<std::endl;
                if(msg["id"] == msg_id) {                    
                    return msg;
                }
            }                       
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    context.should_stop = false;
    return nlohmann::json();
}

int main()
{
    signal(SIGINT, signal_callback_handler);

    const std::string URI = "http://localhost.org:4000/graphql";
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

                nlohmann::json login_msg = wait_for_response(msg_id);

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
                    nlohmann::json response = wait_for_response(msg_id);
                    if(response["payload"]["data"] != nullptr) {
                        std::cout<<"User "<<response["payload"]["data"]["logout"]<<" has successfully logged out..."<<std::endl;
                        context.logged_out();
                    } else 
                    if(response["payload"]["errors"] != nullptr ) {
                        std::cout<<"Failed to log out: "<<response["payload"]["errors"][0]["message"]<<std::endl;
                    }
            },
            "Log out from the service" );

    context.cli.RootMenu() -> Insert(
            "show",
            [](std::ostream& out, const std::string& what){
                if(what == "platforms") {
                    int msg_id = context.gql_manager.query_platforms();
                    nlohmann::json response = wait_for_response(msg_id);
                    if(response["payload"]["data"] != nullptr) {
                        std::cout<<"response: "<<response["payload"]["data"]<<std::endl;
                    } else 
                    if(response["payload"]["errors"] != nullptr ) {
                        std::cout<<"Query failed: "<<response["payload"]["errors"][0]["message"]<<std::endl;
                    }
                }
            },
            "Show supported platforms" );

    auto subMenu = std::make_unique< Menu >( "sub" );    
    subMenu -> Insert(
            "demo",
            [](std::ostream& out, const std::vector<std::string>& cmdline){ 
                out << "params: "<<cmdline.size()<<std::endl;     
            },
            "Print a demo string" );

    auto subSubMenu = std::make_unique< Menu >( "subsub" );
        subSubMenu -> Insert(
            "hello",
            [](std::ostream& out){ out << "Hello, subsubmenu world\n"; },
            "Print hello world in the sub-submenu" );
    subMenu -> Insert( std::move(subSubMenu));

    context.cli.RootMenu() -> Insert( std::move(subMenu) );


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

