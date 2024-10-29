#ifndef KEYGEN_HPP
#define KEYGEN_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <arpa/inet.h> 

namespace metriffic
{


class key_generator 
{
private:
    static void handle_openssl_error() 
    {
        char error_buf[256];
        ERR_error_string_n(ERR_get_error(), error_buf, sizeof(error_buf));
        throw std::runtime_error(std::string("OpenSSL Error: ") + error_buf);
    }

    static std::string bio_to_string(BIO* bio) 
    {
        std::string result;
        char buffer[256];
        int bytes;
        while ((bytes = BIO_read(bio, buffer, sizeof(buffer))) > 0) {
            result.append(buffer, bytes);
        }
        return result;
    }

    // RAII wrapper for OpenSSL BIO
    class bio_wrapper 
    {
    private:
        BIO* bio_;
    public:
        bio_wrapper() : bio_(BIO_new(BIO_s_mem())) {}
        ~bio_wrapper() { if (bio_) BIO_free_all(bio_); }
        BIO* get() { return bio_; }
        bool is_valid() const { return bio_ != nullptr; }
    };

    static std::string generate_openssh_public_key(EVP_PKEY* pkey, 
                                                   const std::string& comment) 
    {
        BIGNUM* n = nullptr;
        BIGNUM* e = nullptr;

        // Get modulus 'n' and exponent 'e' from EVP_PKEY
        if (!EVP_PKEY_get_bn_param(pkey, "n", &n) ||
            !EVP_PKEY_get_bn_param(pkey, "e", &e)) {
            handle_openssl_error();
        }

        if (!n || !e) {
            throw std::runtime_error("Failed to get RSA key components");
        }

        // Key type
        const char* key_type = "ssh-rsa";

        // Prepare the key blob
        std::vector<unsigned char> key_blob;

        // Helper lambda to write data with length prefix
        auto write_with_length = [&](const unsigned char* data, size_t length) {
            uint32_t len = htonl(static_cast<uint32_t>(length));
            key_blob.insert(key_blob.end(), reinterpret_cast<unsigned char*>(&len), reinterpret_cast<unsigned char*>(&len) + 4);
            key_blob.insert(key_blob.end(), data, data + length);
        };

        // Helper lambda to write mpint (multiple precision integer)
        auto write_mpint = [&](const BIGNUM* bn) {
            int bn_len = BN_num_bytes(bn);
            std::vector<unsigned char> bn_bytes(bn_len + 1); // +1 for potential leading zero
            BN_bn2bin(bn, bn_bytes.data() + 1); // Leave space for potential leading zero

            size_t actual_len = bn_len;
            // If MSB is set, add leading zero
            if (bn_bytes[1] & 0x80) {
                bn_bytes[0] = 0x00;
                actual_len = bn_len + 1;
            } else {
                // Remove the extra space if no leading zero is needed
                bn_bytes.erase(bn_bytes.begin());
            }

            uint32_t len = htonl(static_cast<uint32_t>(actual_len));
            key_blob.insert(key_blob.end(), reinterpret_cast<unsigned char*>(&len), reinterpret_cast<unsigned char*>(&len) + 4);
            key_blob.insert(key_blob.end(), bn_bytes.data(), bn_bytes.data() + actual_len);
        };

        // Write key type
        write_with_length(reinterpret_cast<const unsigned char*>(key_type), strlen(key_type));

        // Write exponent 'e' as mpint
        write_mpint(e);

        // Write modulus 'n' as mpint
        write_mpint(n);

        // Base64 encode the key blob
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);

        // Disable newlines
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        BIO_write(b64, key_blob.data(), key_blob.size());
        BIO_flush(b64);

        BUF_MEM* bptr;
        BIO_get_mem_ptr(b64, &bptr);

        // Construct the public key string
        std::string public_key = std::string(key_type) + " " + std::string(bptr->data, bptr->length);

        if (!comment.empty()) {
            public_key += " " + comment;
        }

        // Clean up
        BIO_free_all(b64);
        BN_free(n);
        BN_free(e);

        return public_key;
    }

public:
    static void generate_key_pair(const std::string& key_path, 
                                  int bits, 
                                  const std::string& comment) 
    {
        // Initialize OpenSSL
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();

        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
            EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
            EVP_PKEY_CTX_free
        );

        if (!ctx) {
            handle_openssl_error();
        }

        // Initialize key generation
        if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
            handle_openssl_error();
        }

        // Set the RSA key length
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), bits) <= 0) {
            handle_openssl_error();
        }

        // Generate the key pair
        EVP_PKEY* pkey_raw = nullptr;
        if (EVP_PKEY_keygen(ctx.get(), &pkey_raw) <= 0) {
            handle_openssl_error();
        }

        // Wrap pkey in unique_ptr for automatic cleanup
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(pkey_raw, EVP_PKEY_free);

        // Create BIO for the private key
        bio_wrapper priv_bio;

        if (!priv_bio.is_valid()) {
            handle_openssl_error();
        }

        // Write private key to BIO
        if (!PEM_write_bio_PrivateKey(priv_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
            handle_openssl_error();
        }

        // Convert private key BIO to string
        std::string private_key_str = bio_to_string(priv_bio.get());

        // Generate OpenSSH public key with comment
        std::string public_key_str = generate_openssh_public_key(pkey.get(), comment);

        // Write private key to file
        std::ofstream priv_key_file(key_path, std::ios::out | std::ios::binary);
        if (!priv_key_file) {
            throw std::runtime_error("Failed to open private key file for writing");
        }
        priv_key_file.write(private_key_str.c_str(), private_key_str.size());
        priv_key_file.close();

        // Write public key to file
        std::string pub_key_path = key_path + ".pub";
        std::ofstream pub_key_file(pub_key_path, std::ios::out | std::ios::binary);
        if (!pub_key_file) {
            throw std::runtime_error("Failed to open public key file for writing");
        }
        pub_key_file.write(public_key_str.c_str(), public_key_str.size());
        pub_key_file.close();

        // Cleanup is handled automatically by smart pointers and RAII wrappers
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
        ERR_free_strings();
    }
};

} // namespace metriffic

#endif //KEYGEN_HPP
