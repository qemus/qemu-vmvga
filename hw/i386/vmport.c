/*
 * QEMU VMPort emulation
 *
 * Copyright (C) 2007 Hervé Poussineau
 *
 * Copyright (c) 2026 QEMU VMVGA (https://github.com/qemus/qemu-vmvga)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Guest code that interacts with this virtual device can be found
 * in VMware open-vm-tools open-source project:
 * https://github.com/vmware/open-vm-tools
 */

#include "qemu/osdep.h"
#include "hw/isa/isa.h"
#include "hw/i386/vmport.h"

#ifndef QEMU_VERSION_MAJOR
#error "qemu-vmvga requires QEMU_VERSION_MAJOR from QEMU's build configuration"
#endif

#if QEMU_VERSION_MAJOR == 7 || QEMU_VERSION_MAJOR == 9
#include "hw/qdev-properties.h"
#include "hw/boards.h"
#include "sysemu/sysemu.h"
#include "sysemu/hw_accel.h"
#include "sysemu/qtest.h"
#elif QEMU_VERSION_MAJOR == 11
#include "hw/core/qdev-properties.h"
#include "hw/core/boards.h"
#include "system/system.h"
#include "system/hw_accel.h"
#include "system/qtest.h"
#else
#error "qemu-vmvga supports QEMU major versions 7, 9 and 11"
#endif

#include "qemu/log.h"
#include "trace.h"
#include "qom/object.h"

#if QEMU_VERSION_MAJOR == 11
#define VMPORT_PROPERTY_QUALIFIER const
#define VMPORT_PROPERTY_END
#define VMPORT_CLASS_INIT_DATA const void *
#else
#define VMPORT_PROPERTY_QUALIFIER
#define VMPORT_PROPERTY_END DEFINE_PROP_END_OF_LIST(),
#define VMPORT_CLASS_INIT_DATA void *
#endif

#define VMPORT_MAGIC   0x564D5868

/* Low-bandwidth VMware GuestRPC protocol used by RPCI. */
#define VMPORT_GUESTRPC_CMD_MESSAGE    ((VMPortCommand)30)
#define VMPORT_GUESTRPC_PROTOCOL       0x49435052 /* "RPCI" */
#define VMPORT_GUESTRPC_COOKIE_FLAG    0x80000000
#define VMPORT_GUESTRPC_MAX_CHANNELS   128
#define VMPORT_GUESTRPC_MAX_MESSAGE    (64 * 1024)
#define VMPORT_GUESTRPC_TIMEOUT_US     (80 * G_USEC_PER_SEC)

typedef enum VMPortGuestRPCType {
    VMPORT_GUESTRPC_OPEN,
    VMPORT_GUESTRPC_SENDSIZE,
    VMPORT_GUESTRPC_SENDPAYLOAD,
    VMPORT_GUESTRPC_RECVSIZE,
    VMPORT_GUESTRPC_RECVPAYLOAD,
    VMPORT_GUESTRPC_RECVSTATUS,
    VMPORT_GUESTRPC_CLOSE,
} VMPortGuestRPCType;

#define VMPORT_GUESTRPC_SUCCESS        0x0001
#define VMPORT_GUESTRPC_DORECV         0x0002
#define VMPORT_GUESTRPC_CLOSED         0x0004

typedef struct VMPortGuestRPCChannelState {
    bool open;
    bool uses_cookie;
    uint32_t cookie;
    int64_t last_activity;
    uint32_t request_size;
    uint32_t request_pos;
    uint32_t reply_size;
    uint32_t reply_pos;
} VMPortGuestRPCChannelState;

typedef struct VMPortGuestRPCChannel {
    VMPortGuestRPCChannelState state;
    uint8_t *request;
    char *reply;
} VMPortGuestRPCChannel;

typedef struct VMPortGuestRPCRuntimeState {
    uint32_t next_cookie;
    bool debug_resolved;
    bool debug_enabled;
} VMPortGuestRPCRuntimeState;

/* Compatibility flags for migration */
#define VMPORT_COMPAT_READ_SET_EAX_BIT              0
#define VMPORT_COMPAT_SIGNAL_UNSUPPORTED_CMD_BIT    1
#define VMPORT_COMPAT_REPORT_VMX_TYPE_BIT           2
#define VMPORT_COMPAT_CMDS_V2_BIT                   3
#define VMPORT_COMPAT_READ_SET_EAX              \
    (1 << VMPORT_COMPAT_READ_SET_EAX_BIT)
#define VMPORT_COMPAT_SIGNAL_UNSUPPORTED_CMD    \
    (1 << VMPORT_COMPAT_SIGNAL_UNSUPPORTED_CMD_BIT)
#define VMPORT_COMPAT_REPORT_VMX_TYPE           \
    (1 << VMPORT_COMPAT_REPORT_VMX_TYPE_BIT)
#define VMPORT_COMPAT_CMDS_V2                   \
    (1 << VMPORT_COMPAT_CMDS_V2_BIT)

/* vCPU features reported by CMD_GET_VCPU_INFO */
#define VCPU_INFO_SLC64_BIT             0
#define VCPU_INFO_SYNC_VTSCS_BIT        1
#define VCPU_INFO_HV_REPLAY_OK_BIT      2
#define VCPU_INFO_LEGACY_X2APIC_BIT     3
#define VCPU_INFO_RESERVED_BIT          31

OBJECT_DECLARE_SIMPLE_TYPE(VMPortState, VMPORT)

struct VMPortState {
    ISADevice parent_obj;

    MemoryRegion io;
    VMPortReadFunc *func[VMPORT_ENTRIES];
    void *opaque[VMPORT_ENTRIES];

    uint32_t vmware_vmx_version;
    uint8_t vmware_vmx_type;

    uint32_t compat_flags;

    VMPortGuestRPCChannel guestrpc[VMPORT_GUESTRPC_MAX_CHANNELS];
    VMPortGuestRPCRuntimeState guestrpc_runtime;
    GHashTable *guestinfo;
};

static VMPortState *port_state;

void vmport_register(VMPortCommand command, VMPortReadFunc *func, void *opaque)
{
    assert(command < VMPORT_ENTRIES);
    assert(port_state);

    trace_vmport_register(command, func, opaque);
    port_state->func[command] = func;
    port_state->opaque[command] = opaque;
}

static uint64_t vmport_ioport_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    VMPortState *s = opaque;
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(cs);
    CPUX86State *env;
    unsigned char command;
    uint32_t eax;

    if (qtest_enabled()) {
        return -1;
    }

    env = &cpu->env;
    cpu_synchronize_state(cs);

    eax = env->regs[R_EAX];
    if (eax != VMPORT_MAGIC) {
        goto err;
    }

    command = env->regs[R_ECX];
    trace_vmport_command(command);
    if (command >= VMPORT_ENTRIES || !s->func[command]) {
        qemu_log_mask(LOG_UNIMP, "vmport: unknown command %x\n", command);
        goto err;
    }

    eax = s->func[command](s->opaque[command], addr);
    goto out;

err:
    if (s->compat_flags & VMPORT_COMPAT_SIGNAL_UNSUPPORTED_CMD) {
        eax = UINT32_MAX;
    }

out:
    /*
     * The call above to cpu_synchronize_state() gets vCPU registers values
     * to QEMU but also cause QEMU to write QEMU vCPU registers values to
     * vCPU implementation (e.g. Accelerator such as KVM) just before
     * resuming guest.
     *
     * Therefore, in order to make IOPort return value propagate to
     * guest EAX, we need to explicitly update QEMU EAX register value.
     */
    if (s->compat_flags & VMPORT_COMPAT_READ_SET_EAX) {
        cpu->env.regs[R_EAX] = eax;
    }

    return eax;
}

static void vmport_ioport_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    X86CPU *cpu = X86_CPU(current_cpu);

    if (qtest_enabled()) {
        return;
    }

    cpu->env.regs[R_EAX] = vmport_ioport_read(opaque, addr, 4);
}

static uint32_t vmport_guestrpc_status(X86CPU *cpu, uint16_t status)
{
    CPUX86State *env = &cpu->env;

    env->regs[R_ECX] = (uint32_t)((env->regs[R_ECX] & 0xffff) |
                                  ((uint32_t)status << 16));
    return env->regs[R_EAX];
}

static void vmport_guestrpc_reset_channel(VMPortGuestRPCChannel *channel)
{
    g_free(channel->request);
    g_free(channel->reply);

    memset(channel, 0, sizeof(*channel));
}

static void vmport_guestrpc_reset_runtime(VMPortState *s)
{
    uint32_t i;

    for (i = 0; i < VMPORT_GUESTRPC_MAX_CHANNELS; i++) {
        vmport_guestrpc_reset_channel(&s->guestrpc[i]);
    }

    memset(&s->guestrpc_runtime, 0, sizeof(s->guestrpc_runtime));
    s->guestrpc_runtime.next_cookie = 1;
}

static uint32_t vmport_guestrpc_next_cookie(VMPortState *s)
{
    uint32_t cookie = s->guestrpc_runtime.next_cookie;

    if (!cookie) {
        cookie = 1;
    }

    s->guestrpc_runtime.next_cookie = cookie + 1;
    if (!s->guestrpc_runtime.next_cookie) {
        s->guestrpc_runtime.next_cookie = 1;
    }

    return cookie;
}

static void vmport_guestrpc_sweep_channels(VMPortState *s, int64_t now)
{
    uint32_t i;

    for (i = 0; i < VMPORT_GUESTRPC_MAX_CHANNELS; i++) {
        VMPortGuestRPCChannel *channel = &s->guestrpc[i];

        if (channel->state.open &&
            now - channel->state.last_activity >=
            VMPORT_GUESTRPC_TIMEOUT_US) {
            vmport_guestrpc_reset_channel(channel);
        }
    }
}

static VMPortGuestRPCChannel *vmport_guestrpc_channel(VMPortState *s,
                                                       uint16_t id,
                                                       uint32_t cookie,
                                                       int64_t now)
{
    VMPortGuestRPCChannel *channel;

    if (id == 0 || id > VMPORT_GUESTRPC_MAX_CHANNELS) {
        return NULL;
    }

    channel = &s->guestrpc[id - 1];
    if (!channel->state.open ||
        (channel->state.uses_cookie && channel->state.cookie != cookie)) {
        return NULL;
    }

    channel->state.last_activity = now;
    return channel;
}

static bool vmport_guestrpc_debug_enabled(VMPortState *s)
{
    Object *vmvga;

    if (s->guestrpc_runtime.debug_resolved) {
        return s->guestrpc_runtime.debug_enabled;
    }

    vmvga = object_resolve_path_type("", "vmvga", NULL);
    if (!vmvga) {
        return false;
    }

    s->guestrpc_runtime.debug_enabled =
        object_property_get_bool(vmvga, "debug", NULL);
    s->guestrpc_runtime.debug_resolved = true;

    return s->guestrpc_runtime.debug_enabled;
}

static const bool vmport_guestrpc_trace_rpc = false;

static GString *vmport_guestrpc_format_line(const char *prefix,
                                             const uint8_t *message,
                                             size_t size)
{
    GString *line = g_string_new(prefix);
    size_t i;

    for (i = 0; i < size; i++) {
        uint8_t c = message[i];

        switch (c) {
        case '\r':
            g_string_append(line, "\\r");
            break;
        case '\n':
            g_string_append(line, "\\n");
            break;
        case '\t':
            g_string_append(line, "\\t");
            break;
        default:
            if (c < 0x20 || c == 0x7f) {
                g_string_append_printf(line, "\\x%02x", c);
            } else {
                g_string_append_c(line, c);
            }
            break;
        }
    }

    return line;
}

static void vmport_guestrpc_print_log(VMPortState *s,
                                        const uint8_t *message, size_t size)
{
    if (!vmport_guestrpc_debug_enabled(s)) {
        return;
    }

    GString *line = vmport_guestrpc_format_line("vmport-log: ",
                                                 message, size);

    fprintf(stderr, "%s\n", line->str);
    fflush(stderr);
    g_string_free(line, true);
}

static void vmport_guestrpc_print_rpc(VMPortState *s,
                                        const uint8_t *message, size_t size)
{
    if (!vmport_guestrpc_trace_rpc || !vmport_guestrpc_debug_enabled(s)) {
        return;
    }

    GString *line = vmport_guestrpc_format_line("vmport-rpc: ",
                                                 message, size);

    fprintf(stderr, "%s\n", line->str);
    fflush(stderr);
    g_string_free(line, true);
}

static void vmport_guestrpc_print_reply(VMPortState *s,
                                          const char *reply, size_t size)
{
    if (!vmport_guestrpc_trace_rpc || !vmport_guestrpc_debug_enabled(s)) {
        return;
    }

    GString *line = vmport_guestrpc_format_line("vmport-rpc-reply: ",
                                                 (const uint8_t *)reply, size);

    fprintf(stderr, "%s\n", line->str);
    fflush(stderr);
    g_string_free(line, true);
}

static void vmport_guestrpc_set_reply(VMPortGuestRPCChannel *channel,
                                       char *reply)
{
    g_free(channel->reply);
    channel->reply = reply;
    channel->state.reply_size = strlen(reply);
    channel->state.reply_pos = 0;
}

static const char *vmport_guestrpc_guestinfo_override(const char *key)
{
    if (!strcmp(key, "guestinfo.svga.wddm.buildType")) {
        return "release";
    }

    return NULL;
}

static void vmport_guestrpc_finish_request(VMPortState *s,
                                            VMPortGuestRPCChannel *channel)
{
    static const char info_get_prefix[] = "info-get ";
    static const char info_set_prefix[] = "info-set ";
    char *request;
    const char *key;
    const char *value;
    const char *separator;

    if (channel->state.request_size) {
        request = g_strndup((const char *)channel->request,
                            channel->state.request_size);
    } else {
        request = g_strdup("");
    }

    if (!g_str_has_prefix(request, "log ")) {
        vmport_guestrpc_print_rpc(s, channel->request,
                                  channel->state.request_size);
    }

    if (g_str_has_prefix(request, info_get_prefix)) {
        key = request + sizeof(info_get_prefix) - 1;
        if (g_str_has_prefix(key, "guestinfo.")) {
            value = vmport_guestrpc_guestinfo_override(key);
            if (!value && s->guestinfo) {
                value = g_hash_table_lookup(s->guestinfo, key);
            }
            if (value) {
                vmport_guestrpc_set_reply(channel,
                                          g_strdup_printf("1 %s", value));
            } else {
                vmport_guestrpc_set_reply(channel,
                                          g_strdup("0 No value found"));
            }
        } else {
            vmport_guestrpc_set_reply(channel,
                                      g_strdup("0 Unknown command"));
        }
    } else if (g_str_has_prefix(request, info_set_prefix)) {
        key = request + sizeof(info_set_prefix) - 1;
        separator = strchr(key, ' ');
        if (separator && separator != key &&
            g_str_has_prefix(key, "guestinfo.")) {
            char *stored_key = g_strndup(key, separator - key);

            if (!s->guestinfo) {
                s->guestinfo = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                      g_free, g_free);
            }
            g_hash_table_replace(s->guestinfo, stored_key,
                                 g_strdup(separator + 1));
            vmport_guestrpc_set_reply(channel, g_strdup("1 "));
        } else {
            vmport_guestrpc_set_reply(channel,
                                      g_strdup("0 Unknown command"));
        }
    } else if (channel->state.request_size >= 4 &&
               !memcmp(channel->request, "log ", 4)) {
        vmport_guestrpc_print_log(s, channel->request + 4,
                                  channel->state.request_size - 4);
        vmport_guestrpc_set_reply(channel, g_strdup("1 "));
    } else {
        vmport_guestrpc_set_reply(channel, g_strdup("0 Unknown command"));
    }

    if (!g_str_has_prefix(request, "log ")) {
        vmport_guestrpc_print_reply(s, channel->reply,
                                    channel->state.reply_size);
    }
    g_free(request);
}

/*
 * Implement only the low-bandwidth form of BDOOR_CMD_MESSAGE.  In particular,
 * never advertise MESSAGE_STATUS_HB, so guests send payloads as 4-byte chunks
 * through the normal 0x5658 port and do not switch to the 0x5659 HB port.
 */
static uint32_t vmport_cmd_message(void *opaque, uint32_t addr)
{
    VMPortState *s = opaque;
    X86CPU *cpu = X86_CPU(current_cpu);
    CPUX86State *env = &cpu->env;
    uint16_t type = env->regs[R_ECX] >> 16;
    uint16_t id = env->regs[R_EDX] >> 16;
    uint32_t cookie = (uint32_t)(env->regs[R_EDI] & 0xffff) |
                      (uint32_t)((env->regs[R_ESI] & 0xffff) << 16);
    int64_t now = g_get_monotonic_time();
    VMPortGuestRPCChannel *channel;
    uint32_t value;
    uint32_t remaining;
    uint32_t count;
    uint32_t i;

    vmport_guestrpc_sweep_channels(s, now);

    switch (type) {
    case VMPORT_GUESTRPC_OPEN:

        value = env->regs[R_EBX];
        if ((value & ~VMPORT_GUESTRPC_COOKIE_FLAG) !=
            VMPORT_GUESTRPC_PROTOCOL) {
            return vmport_guestrpc_status(cpu, 0);
        }

        for (i = 0; i < VMPORT_GUESTRPC_MAX_CHANNELS; i++) {
            if (!s->guestrpc[i].state.open) {
                channel = &s->guestrpc[i];
                vmport_guestrpc_reset_channel(channel);
                channel->state.open = true;
                channel->state.uses_cookie =
                    !!(value & VMPORT_GUESTRPC_COOKIE_FLAG);
                channel->state.last_activity = now;
                if (channel->state.uses_cookie) {
                    channel->state.cookie = vmport_guestrpc_next_cookie(s);
                }
                env->regs[R_EDX] = (uint32_t)((env->regs[R_EDX] & 0xffff) |
                                              ((i + 1) << 16));
                env->regs[R_EDI] = channel->state.cookie & 0xffff;
                env->regs[R_ESI] = (channel->state.cookie >> 16) & 0xffff;
                return vmport_guestrpc_status(cpu,
                                              VMPORT_GUESTRPC_SUCCESS);
            }
        }
        return vmport_guestrpc_status(cpu, 0);

    case VMPORT_GUESTRPC_SENDSIZE:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        if (!channel) {
            return vmport_guestrpc_status(cpu, 0);
        }

        value = env->regs[R_EBX];
        if (value > VMPORT_GUESTRPC_MAX_MESSAGE) {
            return vmport_guestrpc_status(cpu, 0);
        }

        g_free(channel->request);
        g_free(channel->reply);
        channel->request = NULL;
        channel->state.request_size = value;
        channel->state.request_pos = 0;
        channel->reply = NULL;
        channel->state.reply_size = 0;
        channel->state.reply_pos = 0;

        if (value) {
            channel->request = g_try_malloc(value);
            if (!channel->request) {
                channel->state.request_size = 0;
                return vmport_guestrpc_status(cpu, 0);
            }
        } else {
            vmport_guestrpc_finish_request(s, channel);
        }
        return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS);

    case VMPORT_GUESTRPC_SENDPAYLOAD:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        if (!channel || !channel->request ||
            channel->state.request_pos >= channel->state.request_size) {
            return vmport_guestrpc_status(cpu, 0);
        }

        value = env->regs[R_EBX];
        remaining = channel->state.request_size - channel->state.request_pos;
        count = MIN(remaining, 4u);
        for (i = 0; i < count; i++) {
            channel->request[channel->state.request_pos++] = value >> (i * 8);
        }
        if (channel->state.request_pos == channel->state.request_size) {
            vmport_guestrpc_finish_request(s, channel);
        }
        return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS);

    case VMPORT_GUESTRPC_RECVSIZE:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        if (!channel) {
            return vmport_guestrpc_status(cpu, 0);
        }
        if (!channel->reply) {
            return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS);
        }

        channel->state.reply_pos = 0;
        env->regs[R_EBX] = channel->state.reply_size;
        env->regs[R_EDX] = (uint32_t)((env->regs[R_EDX] & 0xffff) |
                                      (VMPORT_GUESTRPC_SENDSIZE << 16));
        return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS |
                                      VMPORT_GUESTRPC_DORECV);

    case VMPORT_GUESTRPC_RECVPAYLOAD:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        if (!channel || !channel->reply ||
            channel->state.reply_pos >= channel->state.reply_size) {
            return vmport_guestrpc_status(cpu, 0);
        }

        remaining = channel->state.reply_size - channel->state.reply_pos;
        count = MIN(remaining, 4u);
        value = 0;
        for (i = 0; i < count; i++) {
            value |= (uint32_t)(uint8_t)channel->reply[channel->state.reply_pos++]
                     << (i * 8);
        }
        env->regs[R_EBX] = value;
        env->regs[R_EDX] = (uint32_t)((env->regs[R_EDX] & 0xffff) |
                                      (VMPORT_GUESTRPC_SENDPAYLOAD << 16));
        return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS);

    case VMPORT_GUESTRPC_RECVSTATUS:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        return vmport_guestrpc_status(cpu, channel ?
                                      VMPORT_GUESTRPC_SUCCESS : 0);

    case VMPORT_GUESTRPC_CLOSE:

        channel = vmport_guestrpc_channel(s, id, cookie, now);
        if (!channel) {
            return vmport_guestrpc_status(cpu, 0);
        }
        vmport_guestrpc_reset_channel(channel);
        return vmport_guestrpc_status(cpu, VMPORT_GUESTRPC_SUCCESS |
                                      VMPORT_GUESTRPC_CLOSED);

    default:
        return vmport_guestrpc_status(cpu, 0);
    }
}

static uint32_t vmport_cmd_get_version(void *opaque, uint32_t addr)
{
    X86CPU *cpu = X86_CPU(current_cpu);

    if (qtest_enabled()) {
        return -1;
    }

    cpu->env.regs[R_EBX] = VMPORT_MAGIC;
    if (port_state->compat_flags & VMPORT_COMPAT_REPORT_VMX_TYPE) {
        cpu->env.regs[R_ECX] = port_state->vmware_vmx_type;
    }

    return port_state->vmware_vmx_version;
}

static uint32_t vmport_cmd_get_bios_uuid(void *opaque, uint32_t addr)
{
    X86CPU *cpu = X86_CPU(current_cpu);
    uint32_t *uuid_parts = (uint32_t *)(qemu_uuid.data);

    cpu->env.regs[R_EAX] = le32_to_cpu(uuid_parts[0]);
    cpu->env.regs[R_EBX] = le32_to_cpu(uuid_parts[1]);
    cpu->env.regs[R_ECX] = le32_to_cpu(uuid_parts[2]);
    cpu->env.regs[R_EDX] = le32_to_cpu(uuid_parts[3]);

    return cpu->env.regs[R_EAX];
}

static uint32_t vmport_cmd_ram_size(void *opaque, uint32_t addr)
{
    X86CPU *cpu = X86_CPU(current_cpu);

    if (qtest_enabled()) {
        return -1;
    }

    cpu->env.regs[R_EBX] = 0x1177;
    return current_machine->ram_size;
}

static uint32_t vmport_cmd_get_hz(void *opaque, uint32_t addr)
{
    X86CPU *cpu = X86_CPU(current_cpu);

    if (cpu->env.tsc_khz && cpu->env.apic_bus_freq) {
        uint64_t tsc_freq = (uint64_t)cpu->env.tsc_khz * 1000;

        cpu->env.regs[R_ECX] = cpu->env.apic_bus_freq;
        cpu->env.regs[R_EBX] = (uint32_t)(tsc_freq >> 32);
        cpu->env.regs[R_EAX] = (uint32_t)tsc_freq;
    } else {
        /* Signal cmd as not supported */
        cpu->env.regs[R_EBX] = UINT32_MAX;
    }

    return cpu->env.regs[R_EAX];
}

static uint32_t vmport_cmd_get_vcpu_info(void *opaque, uint32_t addr)
{
    X86CPU *cpu = X86_CPU(current_cpu);
    uint32_t ret = 0;

    if (cpu->env.features[FEAT_1_ECX] & CPUID_EXT_X2APIC) {
        ret |= 1 << VCPU_INFO_LEGACY_X2APIC_BIT;
    }

    return ret;
}

static const MemoryRegionOps vmport_ops = {
    .read = vmport_ioport_read,
    .write = vmport_ioport_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void vmport_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    VMPortState *s = VMPORT(dev);

    memory_region_init_io(&s->io, OBJECT(s), &vmport_ops, s, "vmport", 1);
    isa_register_ioport(isadev, &s->io, 0x5658);

    port_state = s;

    /* Register some generic port commands */
    vmport_guestrpc_reset_runtime(s);
    vmport_register(VMPORT_CMD_GETVERSION, vmport_cmd_get_version, NULL);
    vmport_register(VMPORT_CMD_GETRAMSIZE, vmport_cmd_ram_size, NULL);
    vmport_register(VMPORT_GUESTRPC_CMD_MESSAGE, vmport_cmd_message, s);
    if (s->compat_flags & VMPORT_COMPAT_CMDS_V2) {
        vmport_register(VMPORT_CMD_GETBIOSUUID, vmport_cmd_get_bios_uuid, NULL);
        vmport_register(VMPORT_CMD_GETHZ, vmport_cmd_get_hz, NULL);
        vmport_register(VMPORT_CMD_GET_VCPU_INFO, vmport_cmd_get_vcpu_info,
                        NULL);
    }
}

static VMPORT_PROPERTY_QUALIFIER Property vmport_properties[] = {
    /* Used to enforce compatibility for migration */
    DEFINE_PROP_BIT("x-read-set-eax", VMPortState, compat_flags,
                    VMPORT_COMPAT_READ_SET_EAX_BIT, true),
    DEFINE_PROP_BIT("x-signal-unsupported-cmd", VMPortState, compat_flags,
                    VMPORT_COMPAT_SIGNAL_UNSUPPORTED_CMD_BIT, true),
    DEFINE_PROP_BIT("x-report-vmx-type", VMPortState, compat_flags,
                    VMPORT_COMPAT_REPORT_VMX_TYPE_BIT, true),
    DEFINE_PROP_BIT("x-cmds-v2", VMPortState, compat_flags,
                    VMPORT_COMPAT_CMDS_V2_BIT, true),

    /* Default value taken from open-vm-tools code VERSION_MAGIC definition */
    DEFINE_PROP_UINT32("vmware-vmx-version", VMPortState,
                       vmware_vmx_version, 6),
    /*
     * Value determines which VMware product type host report itself to guest.
     *
     * Most guests are fine with exposing host as VMware ESX server.
     * Some legacy/proprietary guests hard-code a given type.
     *
     * For a complete list of values, refer to enum VMXType at open-vm-tools
     * project (Defined at lib/include/vm_vmx_type.h).
     *
     * Reasonable options:
     * 0 - Unset
     * 1 - VMware Express (deprecated)
     * 2 - VMware ESX Server
     * 3 - VMware Server (Deprecated)
     * 4 - VMware Workstation
     * 5 - ACE 1.x (Deprecated)
     */
    DEFINE_PROP_UINT8("vmware-vmx-type", VMPortState, vmware_vmx_type, 2),

    VMPORT_PROPERTY_END
};

static void vmport_reset(DeviceState *dev)
{
    VMPortState *s = VMPORT(dev);

    vmport_guestrpc_reset_runtime(s);
}

static void vmport_class_initfn(ObjectClass *klass, VMPORT_CLASS_INIT_DATA data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = vmport_realizefn;
#if QEMU_VERSION_MAJOR == 7 || \
    (QEMU_VERSION_MAJOR == 9 && QEMU_VERSION_MINOR < 2)
    dc->reset = vmport_reset;
#else
    device_class_set_legacy_reset(dc, vmport_reset);
#endif

    /* Reason: realize sets global port_state */
    dc->user_creatable = false;
    device_class_set_props(dc, vmport_properties);
}

static const TypeInfo vmport_info = {
    .name          = TYPE_VMPORT,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(VMPortState),
    .class_init    = vmport_class_initfn,
};

static void vmport_register_types(void)
{
    type_register_static(&vmport_info);
}

type_init(vmport_register_types)
