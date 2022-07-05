/*
 * @file auth-jwt.h
 */

#ifndef AUTH_JWT_H_
#define AUTH_JWT_H_	1

#include <string>
#include "jwt-cpp/jwt.h"

class AuthJWT {
private:
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> *verifier;
public:
    std::string realm;
    std::string issuer;
    std::string secret;
    AuthJWT(const std::string &realm, const std::string &issuer, const std::string &secret);
    virtual ~AuthJWT();
    bool verify(const std::string &token);
};
#endif