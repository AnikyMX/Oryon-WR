// Oryon -- display-list recording interface. ORY_DL(fn, args...) is the first statement of every compilable
// entry point: one predictable branch when no list is being compiled.
#pragma once
namespace ory {
typedef void (*DlThunk)(const void *args);
void dl_push_call(DlThunk fn, const void *args, size_t size);
} // namespace ory
#include "gen/dlist_gen.hpp"
#define ORY_DL(fn, ...) do { if (UNLIKELY(::ory::g.dl.mode) && ::ory::dlr_##fn(__VA_ARGS__)) return; } while (0)
