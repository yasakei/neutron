#ifndef native_time_h
#define native_time_h

#include <memory>

namespace neutron {
    class Environment;
}

void register_time_lib(std::shared_ptr<neutron::Environment> env);

#endif