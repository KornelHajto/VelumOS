#ifndef VELUM_TYPES_H
#define VELUM_TYPES_H

#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <unordered_map>

namespace Velum {

// a node identifier in the cluster
using NodeId = uint32_t;

//logical clock for membership/versioning
using LogicalTime = uint64_t;

enum class NodeState {
     Alive,
     Suspected,
     Dead
};

struct MemberRecord {
       NodeId id;
       LogicalTime version;
       NodeState state;
};

} //namespace velum

#endif
