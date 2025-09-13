#ifndef NEUTRON_LIBS_HTTP_NATIVE_H
#define NEUTRON_LIBS_HTTP_NATIVE_H

#include "vm.h"

namespace neutron {

void register_http_functions(std::shared_ptr<Environment> env);

}

#endif // NEUTRON_LIBS_HTTP_NATIVE_H