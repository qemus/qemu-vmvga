/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Begin a packed VMware SVGA protocol structure.
 *
 * This file intentionally has no include guard: VMware's protocol headers
 * include it around each structure that is part of the on-wire ABI.
 */
#if defined(_MSC_VER)
#pragma pack(push, 1)
#elif defined(__GNUC__) || defined(__clang__)
/* The packed attribute is emitted by vmware_pack_end.h. */
#else
#error Unsupported compiler for VMware SVGA structure packing
#endif
