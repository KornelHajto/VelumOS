#ifndef VELUM_MEMBERSHIP_H
#define VELUM_MEMBERSHIP_H

#include "types.h"

namespace velum {

class MembershipTable {
public:

    //insert or update membership information
    void update(const MemberRecord& rec);

    //get the record for a node
    std::optional<MemberRecord> get(NodeId id) const;

    //merge incoming records into our table
    void merge(const std::vector<MemberRecord>& incoming);

    //return all of the current member records
    std::vector<MemberRecord> all() const;

    //marking a node as suspected
    void suspect(NodeId id);

    //marking a node as dead
    void markDead(NodeId id);

private:
    std::unordered_map<NodeId, MemberRecord> table_;
}

} //namespace velum

#endif
