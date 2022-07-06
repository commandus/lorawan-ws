#include "auth-jwt.h"

AuthJWT::AuthJWT(
    const std::string &aIssuer,
    const std::string &secret
)
    : issuer(aIssuer)
{
    verifier = &jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{ secret })
        .with_issuer(issuer);
}

AuthJWT::~AuthJWT()
{
    delete verifier;
}

bool AuthJWT::verify(
    const std::string &token
)
{
    const jwt::decoded_jwt<jwt::traits::kazuho_picojson> s = jwt::decode(token);
    std::error_code ec;
    verifier->verify(s, ec);
    return ec.value() == 0;
}
