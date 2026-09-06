/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * End a packed VMware SVGA protocol structure.
 *
 * This file intentionally has no include guard; see vmware_pack_begin.h.
 */
#if defined(_MSC_VER)
#pragma pack(pop)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((__packed__))
#else
#error Unsupported compiler for VMware SVGA structure packing
#endif
