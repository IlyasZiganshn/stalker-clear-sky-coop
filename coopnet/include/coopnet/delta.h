#pragma once

#include "coopnet/snapshot.h"

namespace coopnet {

// Server-side delta: diffs the world against the snapshot the client has
// confirmed, so only changed fields go on the wire. Values are compared as
// they will be encoded, i.e. raw floats; quantization jitter can therefore
// send an unchanged field, which costs bandwidth but never correctness.
class SnapshotDelta {
public:
    // `base` is the snapshot with tick `base.tick` the client holds; an empty
    // base produces a full snapshot. Entities absent from `base` are sent with
    // their full present mask, changed ones with only the changed fields.
    static Snapshot makeDelta(const Snapshot& base, const Snapshot& current);
};

}  // namespace coopnet