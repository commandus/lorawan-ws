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
    std::string issuer;
    AuthJWT(const std::string &issuer, const std::string &secret);
    virtual ~AuthJWT();
    bool verify(const std::string &token);
};
#endif