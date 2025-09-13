#ifndef NEUTRON_LIBS_JSON_NATIVE_H
#define NEUTRON_LIBS_JSON_NATIVE_H

#include "vm.h"

namespace neutron {

void register_json_functions(std::shared_ptr<Environment> env);

}

#endif // NEUTRON_LIBS_JSON_NATIVE_H