// License: please see LICENSE1 file for more details.
#pragma once

#define CONSTANT constant
#define KERNEL kernel
#define LOCAL local
#define GLOBAL global


size_t getGlobalId(	const uint dimindx) {
  return get_global_id(dimindx);
}
size_t getGroupId(	const uint dimindx) {
  return get_group_id(dimindx);
}
size_t getLocalId(	const uint dimindx) {
  return get_local_id(dimindx);
}

inline void localMemoryFence() {
	barrier(CLK_LOCAL_MEM_FENCE);
}

