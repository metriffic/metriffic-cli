#include <fstream>

#include <iostream>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <vector>
#include <chrono>
#include <ctime>
#include <sstream>

#include <nlohmann/json.hpp>
#include <cxxopts.hpp>


#include "authentication_commands.hpp"
#include "utils.hpp"

namespace metriffic
{

class jwt_generator 
{
private:
    std::string base64url_encode(const std::string& input) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* bmem = BIO_new(BIO_s_mem());
        // Remove base64 line breaks
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_push(b64, bmem);
        BIO_write(b64, input.c_str(), input.length());
        BIO_flush(b64);
        BUF_MEM* bptr;
        BIO_get_mem_ptr(b64, &bptr);
        std::string encoded(bptr->data, bptr->length);
        BIO_free_all(b64);
        // Convert to base64url
        for (char& c : encoded) {
            switch (c) {
                case '+': c = '-'; break;
                case '/': c = '_'; break;
                case '=': c = 0; break; // Remove padding
            }
        }
        // Remove any null terminator
        encoded.erase(std::find(encoded.begin(), encoded.end(), 0), encoded.end());
        return encoded;
    }

    std::string sign_rs256(const std::string& message, EVP_PKEY* private_key) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create signing context");
        }
        if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, private_key) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize signing context");
        }
        if (EVP_DigestSignUpdate(ctx, message.c_str(), message.length()) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to update signing context");
        }
        size_t signature_length = 0;
        if (EVP_DigestSignFinal(ctx, nullptr, &signature_length) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to get signature length");
        }
        std::vector<unsigned char> signature(signature_length);
        if (EVP_DigestSignFinal(ctx, signature.data(), &signature_length) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to create signature");
        }
        EVP_MD_CTX_free(ctx);
        return base64url_encode(std::string(signature.begin(), signature.begin() + signature_length));
    }

public:
    std::string generate_jwt(const nlohmann::json& payload, 
                             const std::string& private_key_path, 
                             const std::string& key_passphrase = "") 
    {
        // Create header
        nlohmann::json header = {
            {"alg", "RS256"},
            {"typ", "JWT"}
        };
        // Serialize to strings first to ensure proper JSON formatting
        std::string header_str = header.dump();
        std::string payload_str = payload.dump();
        // Base64url encode header and payload
        std::string header_b64 = base64url_encode(header_str);
        std::string payload_b64 = base64url_encode(payload_str);
        // Create signing input
        std::string signing_input = header_b64 + "." + payload_b64;
        // Load private key
        BIO* bio = BIO_new_file(private_key_path.c_str(), "r");
        if (!bio) {
            throw std::runtime_error("Failed to open the private key file for this user (user doesn't exist?)");
        }
        EVP_PKEY* private_key = nullptr;
        if (!key_passphrase.empty()) {
            private_key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, 
                const_cast<char*>(key_passphrase.c_str()));
        } else {
            private_key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        }
        BIO_free(bio);
        if (!private_key) {
            throw std::runtime_error("Failed to load private key");
        }
        // Sign the input
        std::string signature = sign_rs256(signing_input, private_key);
        EVP_PKEY_free(private_key);
        // Create final JWT
        return signing_input + "." + signature;
    }
};


authentication_commands::authentication_commands(Context& c)
 : m_context(c)
{}

std::string 
authentication_commands::generate_token(const std::string& username) 
{
    nlohmann::json payload = {
            {"iss", "metriffic_cli"},
            {"username", username},
            {"iat", std::time(nullptr)},
            {"exp", std::time(nullptr) + 24*3600} // Token expires in 1 hour
    };

    jwt_generator jwt_gen;
    std::string jwt = jwt_gen.generate_jwt(payload, m_context.settings.user_key_file(username));
    return jwt;
}


std::shared_ptr<cli::Command> 
authentication_commands::create_login_cmd()
{
    return create_cmd_helper(
        CMD_LOGIN_NAME,
        [this](std::ostream& out, int argc, char** argv){ 
            cxxopts::Options options(CMD_LOGIN_NAME, CMD_LOGIN_HELP);
            options.add_options()("username", CMD_LOGIN_PARAMDESC[0], cxxopts::value<std::string>());
            options.parse_positional({"username"});

            try {
                auto result = options.parse(argc, argv);
                if(result.count("username") != 1) {
                    out << CMD_LOGIN_PARAMDESC[0] << std::endl;
                    return;
                }
                auto username = result["username"].as<std::string>();
                
                auto token = generate_token(username);
                int msg_id = m_context.gql_manager.login(username, token);
                auto response = m_context.gql_manager.wait_for_response(msg_id);
                nlohmann::json& login_msg  = response.second;
                if(login_msg["payload"]["data"] != nullptr) {
                    std::cout<<"login successful!"<<std::endl;
                    auto& data = login_msg["payload"]["data"]["login"];
                    const auto& username = data["username"];
                    const auto& token = data["token"];
                    m_context.logged_in(username, token);
                    if(!m_context.settings.user_config_exists(username)) {
                        m_context.settings.create_user(username);
                    }
                    m_context.settings.set_active_user(m_context.username, token);
                } else 
                if(login_msg["payload"].contains("errors") ) {
                    std::cout<<"login failed: "<<login_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
                    m_context.logged_out();
                }

            } catch (std::exception& e) {
                out << CMD_LOGIN_NAME << ": " << e.what() << std::endl;
                return;
            }        
        },
        CMD_LOGIN_HELP,
        CMD_LOGIN_PARAMDESC
    );
}

std::shared_ptr<cli::Command> 
authentication_commands::create_logout_cmd()
{
    return create_cmd_helper(
        CMD_LOGOUT_NAME,
        [this](std::ostream& out, int, char**){
            int msg_id = m_context.gql_manager.logout();
            auto response = m_context.gql_manager.wait_for_response(msg_id);
            nlohmann::json& logout_msg  = response.second;

            if(logout_msg["payload"]["data"] != nullptr) {
                std::cout<<"User "<<logout_msg["payload"]["data"]["logout"].get<std::string>()<<" has successfully logged out..."<<std::endl;
                m_context.logged_out();
                m_context.settings.clear_active_user();
            } else 
            if(logout_msg["payload"].contains("errors") ) {
                std::cout<<"Failed to log out: "<<logout_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            }
        },
        CMD_LOGOUT_HELP,
        CMD_LOGOUT_PARAMDESC
    );

}

} // namespace metriffic
