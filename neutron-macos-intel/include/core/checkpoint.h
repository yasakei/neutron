#ifndef NEUTRON_CHECKPOINT_H
#define NEUTRON_CHECKPOINT_H

#include "vm.h"
#include <string>

namespace neutron {

class CheckpointManager {
public:
    static void saveCheckpoint(VM& vm, const std::string& path, Value returnValue = Value(true), int argsToPop = 1);
    static void loadCheckpoint(VM& vm, const std::string& path);
};

}

#endif // NEUTRON_CHECKPOINT_H
