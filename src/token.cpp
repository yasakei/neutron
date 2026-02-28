#include "token.h"
#include <ostream>

namespace neutron {

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token(" << static_cast<int>(token.type) << ", " << token.lexeme << ", " << token.line << ")";
    return os;
}

} // namespace neutron