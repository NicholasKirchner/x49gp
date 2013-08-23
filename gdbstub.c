/*
 * gdb server stub
 * 
 * Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <x49gp.h>
#include "gdbstub.h"

#ifdef _WIN32
/* XXX: these constants may be independent of the host ones even for Unix */
#ifndef SIGTRAP
#define SIGTRAP 5
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#else
#include <signal.h>
#endif

#define DEBUG_GDB

enum RSState {
    RS_IDLE,
    RS_GETLINE,
    RS_CHKSUM1,
    RS_CHKSUM2,
    RS_SYSCALL,
};

typedef struct GDBState {
    CPUState *env; /* current CPU */
    enum RSState state; /* parsing state */
    char line_buf[4096];
    int line_buf_index;
    int line_csum;
    char last_packet[4100];
    int last_packet_len;
    int fd;
    int running_state;
} GDBState;

/* XXX: This is not thread safe.  Do we care?  */
static int gdbserver_fd = -1;

/* XXX: remove this hack.  */
static GDBState gdbserver_state;

static int get_char(GDBState *s)
{
    uint8_t ch;
    int ret;

    for(;;) {
        ret = recv(s->fd, &ch, 1, 0);
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN)
                return -1;
        } else if (ret == 0) {
            return -1;
        } else {
            break;
        }
    }
    return ch;
}

/* GDB stub state for use by semihosting syscalls.  */
static GDBState *gdb_syscall_state;
static gdb_syscall_complete_cb gdb_current_syscall_cb;

enum {
    GDB_SYS_UNKNOWN,
    GDB_SYS_ENABLED,
    GDB_SYS_DISABLED,
} gdb_syscall_mode;

/* If gdb is connected when the first semihosting syscall occurs then use
   remote gdb syscalls.  Otherwise use native file IO.  */
int use_gdb_syscalls(void)
{
    if (gdb_syscall_mode == GDB_SYS_UNKNOWN) {
        gdb_syscall_mode = (gdb_syscall_state ? GDB_SYS_ENABLED
                                              : GDB_SYS_DISABLED);
    }
    return gdb_syscall_mode == GDB_SYS_ENABLED;
}

static void put_buffer(GDBState *s, const uint8_t *buf, int len)
{
    int ret;

    while (len > 0) {
        ret = send(s->fd, buf, len, 0);
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN)
                return;
        } else {
            buf += ret;
            len -= ret;
        }
    }
}

static inline int fromhex(int v)
{
    if (v >= '0' && v <= '9')
        return v - '0';
    else if (v >= 'A' && v <= 'F')
        return v - 'A' + 10;
    else if (v >= 'a' && v <= 'f')
        return v - 'a' + 10;
    else
        return 0;
}

static inline int tohex(int v)
{
    if (v < 10)
        return v + '0';
    else
        return v - 10 + 'a';
}

static void memtohex(char *buf, const uint8_t *mem, int len)
{
    int i, c;
    char *q;
    q = buf;
    for(i = 0; i < len; i++) {
        c = mem[i];
        *q++ = tohex(c >> 4);
        *q++ = tohex(c & 0xf);
    }
    *q = '\0';
}

static void hextomem(uint8_t *mem, const char *buf, int len)
{
    int i;

    for(i = 0; i < len; i++) {
        mem[i] = (fromhex(buf[0]) << 4) | fromhex(buf[1]);
        buf += 2;
    }
}

/* return -1 if error, 0 if OK */
static int put_packet(GDBState *s, char *buf)
{
    int len, csum, i;
    char *p;

#ifdef DEBUG_GDB
    printf("reply='%s'\n", buf);
#endif

    for(;;) {
        p = s->last_packet;
        *(p++) = '$';
        len = strlen(buf);
        memcpy(p, buf, len);
        p += len;
        csum = 0;
        for(i = 0; i < len; i++) {
            csum += buf[i];
        }
        *(p++) = '#';
        *(p++) = tohex((csum >> 4) & 0xf);
        *(p++) = tohex((csum) & 0xf);

        s->last_packet_len = p - s->last_packet;
        put_buffer(s, (uint8_t *) s->last_packet, s->last_packet_len);

        i = get_char(s);
        if (i < 0)
            return -1;
        if (i == '+')
            break;
    }
    return 0;
}

static int cpu_gdb_read_registers(CPUState *env, uint8_t *mem_buf)
{
    int i;
    uint8_t *ptr;

    ptr = mem_buf;
    /* 16 core integer registers (4 bytes each).  */
    for (i = 0; i < 16; i++)
      {
        *(uint32_t *)ptr = tswapl(env->regs[i]);
        ptr += 4;
      }
    /* 8 FPA registers (12 bytes each), FPS (4 bytes).
       Not yet implemented.  */
    memset (ptr, 0, 8 * 12 + 4);
    ptr += 8 * 12 + 4;
    /* CPSR (4 bytes).  */
    *(uint32_t *)ptr = tswapl (cpsr_read(env));
    ptr += 4;

    return ptr - mem_buf;
}

static void cpu_gdb_write_registers(CPUState *env, uint8_t *mem_buf, int size)
{
    int i;
    uint8_t *ptr;

    ptr = mem_buf;
    /* Core integer registers.  */
    for (i = 0; i < 16; i++)
      {
        env->regs[i] = tswapl(*(uint32_t *)ptr);
        ptr += 4;
      }
    /* Ignore FPA regs and scr.  */
    ptr += 8 * 12 + 4;
    cpsr_write (env, tswapl(*(uint32_t *)ptr), 0xffffffff);
}

static int gdb_handle_packet(GDBState *s, CPUState *env, const char *line_buf)
{
    const char *p;
    int ch, reg_size, type;
    char buf[4096];
    uint8_t mem_buf[2000];
    uint32_t *registers;
    target_ulong addr, len;
    
#ifdef DEBUG_GDB
    printf("command='%s'\n", line_buf);
#endif
    p = line_buf;
    ch = *p++;
    switch(ch) {
    case '?':
        /* TODO: Make this return the correct value for user-mode.  */
        snprintf(buf, sizeof(buf), "S%02x", SIGTRAP);
        put_packet(s, buf);
        break;
    case 'c':
        if (*p != '\0') {
            addr = strtoull(p, (char **)&p, 16);
            env->regs[15] = addr;
        }
        s->running_state = 1;
	return RS_IDLE;
    case 's':
        if (*p != '\0') {
            addr = strtoul(p, (char **)&p, 16);
            env->regs[15] = addr;
        }
        cpu_single_step(env, 1);
        s->running_state = 1;
	return RS_IDLE;
    case 'F':
        {
            target_ulong ret;
            target_ulong err;

            ret = strtoull(p, (char **)&p, 16);
            if (*p == ',') {
                p++;
                err = strtoull(p, (char **)&p, 16);
            } else {
                err = 0;
            }
            if (*p == ',')
                p++;
            type = *p;
            if (gdb_current_syscall_cb)
                gdb_current_syscall_cb(s->env, ret, err);
            if (type == 'C') {
                put_packet(s, "T02");
            } else {
                s->running_state = 1;
            }
        }
        break;
    case 'g':
        reg_size = cpu_gdb_read_registers(env, mem_buf);
        memtohex(buf, mem_buf, reg_size);
        put_packet(s, buf);
        break;
    case 'G':
        registers = (void *)mem_buf;
        len = strlen(p) / 2;
        hextomem((uint8_t *)registers, p, len);
        cpu_gdb_write_registers(env, mem_buf, len);
        put_packet(s, "OK");
        break;
    case 'm':
        addr = strtoull(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        len = strtoull(p, NULL, 16);
        if (cpu_memory_rw_debug(env, addr, mem_buf, len, 0) != 0) {
            put_packet (s, "E14");
        } else {
            memtohex(buf, mem_buf, len);
            put_packet(s, buf);
        }
        break;
    case 'M':
        addr = strtoull(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        len = strtoull(p, (char **)&p, 16);
        if (*p == ':')
            p++;
        hextomem(mem_buf, p, len);
        if (cpu_memory_rw_debug(env, addr, mem_buf, len, 1) != 0)
            put_packet(s, "E14");
        else
            put_packet(s, "OK");
        break;
    case 'Z':
        type = strtoul(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        addr = strtoull(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        len = strtoull(p, (char **)&p, 16);
        if (type == 0 || type == 1) {
            if (
#ifdef QEMU_OLD
                cpu_breakpoint_insert(env, addr) < 0
#else
                cpu_breakpoint_insert(env, addr, BP_GDB, NULL) < 0
#endif
                )
                goto breakpoint_error;
            put_packet(s, "OK");
        } else {
        breakpoint_error:
            put_packet(s, "E22");
        }
        break;
    case 'z':
        type = strtoul(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        addr = strtoull(p, (char **)&p, 16);
        if (*p == ',')
            p++;
        len = strtoull(p, (char **)&p, 16);
        if (type == 0 || type == 1) {
#ifdef QEMU_OLD
            cpu_breakpoint_remove(env, addr);
#else
            cpu_breakpoint_remove(env, addr, BP_GDB);
#endif
            put_packet(s, "OK");
        } else {
            goto breakpoint_error;
        }
        break;
    default:
        //        unknown_command:
        /* put empty packet */
        buf[0] = '\0';
        put_packet(s, buf);
        break;
    }
    return RS_IDLE;
}

extern void tb_flush(CPUState *env);

/* Send a gdb syscall request.
   This accepts limited printf-style format specifiers, specifically:
    %x - target_ulong argument printed in hex.
    %s - string pointer (target_ulong) and length (int) pair.  */
void gdb_do_syscall(gdb_syscall_complete_cb cb, char *fmt, ...)
{
    va_list va;
    char buf[256];
    char *p;
    target_ulong addr;
    GDBState *s;

    s = gdb_syscall_state;
    if (!s)
        return;
    gdb_current_syscall_cb = cb;
    s->state = RS_IDLE;
    va_start(va, fmt);
    p = buf;
    *(p++) = 'F';
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt++) {
            case 'x':
                addr = va_arg(va, target_ulong);
                p += sprintf(p, TARGET_FMT_lx, addr);
                break;
            case 's':
                addr = va_arg(va, target_ulong);
                p += sprintf(p, TARGET_FMT_lx "/%x", addr, va_arg(va, int));
                break;
            default:
                fprintf(stderr, "gdbstub: Bad syscall format string '%s'\n",
                        fmt - 1);
                break;
            }
        } else {
            *(p++) = *(fmt++);
        }
    }
    va_end(va);
    put_packet(s, buf);
#ifdef QEMU_OLD
    cpu_interrupt(s->env, CPU_INTERRUPT_EXIT);
#else
    cpu_exit(s->env);
#endif
}

static void gdb_read_byte(GDBState *s, int ch)
{
    CPUState *env = s->env;
  char buf[256];
    int i, csum;
    char reply[1];

printf("%s: state %u, byte %02x (%c)\n", __FUNCTION__, s->state, ch, ch);
fflush(stdout);

        switch(s->state) {
        case RS_IDLE:
            if (ch == '$') {
                s->line_buf_index = 0;
                s->state = RS_GETLINE;
            } else if (ch == 0x03) {
      		snprintf(buf, sizeof(buf), "S%02x", SIGINT);
		put_packet(s, buf);
	    }
            break;
        case RS_GETLINE:
            if (ch == '#') {
            s->state = RS_CHKSUM1;
            } else if (s->line_buf_index >= sizeof(s->line_buf) - 1) {
                s->state = RS_IDLE;
            } else {
            s->line_buf[s->line_buf_index++] = ch;
            }
            break;
        case RS_CHKSUM1:
            s->line_buf[s->line_buf_index] = '\0';
            s->line_csum = fromhex(ch) << 4;
            s->state = RS_CHKSUM2;
            break;
        case RS_CHKSUM2:
            s->line_csum |= fromhex(ch);
            csum = 0;
            for(i = 0; i < s->line_buf_index; i++) {
                csum += s->line_buf[i];
            }
            if (s->line_csum != (csum & 0xff)) {
                reply[0] = '-';
                put_buffer(s, (uint8_t *) reply, 1);
                s->state = RS_IDLE;
            } else {
                reply[0] = '+';
                put_buffer(s, (uint8_t *) reply, 1);
                s->state = gdb_handle_packet(s, env, s->line_buf);
            }
            break;
        default:
            abort();
        }
}

int
gdb_handlesig (CPUState *env, int sig)
{
  GDBState *s;
  char buf[256];
  int n;

  if (gdbserver_fd < 0)
    return sig;

  s = &gdbserver_state;

printf("%s: sig: %u\n", __FUNCTION__, sig);
fflush(stdout);

  /* disable single step if it was enabled */
  cpu_single_step(env, 0);
  tb_flush(env);

  if (sig != 0)
    {
      snprintf(buf, sizeof(buf), "S%02x", sig);
      put_packet(s, buf);
    }

  sig = 0;
  s->state = RS_IDLE;
  s->running_state = 0;
  while (s->running_state == 0) {
      n = read (s->fd, buf, 256);
      if (n > 0)
        {
          int i;

printf("%s: read: %d\n", __FUNCTION__, n);
fflush(stdout);

          for (i = 0; i < n; i++)
            gdb_read_byte (s, buf[i]);
        }
      else if (n == 0 || errno != EAGAIN)
        {
          /* XXX: Connection closed.  Should probably wait for annother
             connection before continuing.  */
          return sig;
        }
  }
  return sig;
}

int
gdb_poll (CPUState *env)
{
	GDBState *s;
	struct pollfd pfd;

	if (gdbserver_fd < 0)
		return 0;

	s = &gdbserver_state;

	pfd.fd = s->fd;
	pfd.events = POLLIN | POLLHUP;

	if (poll(&pfd, 1, 0) <= 0) {
		if (errno != EAGAIN)
			return 0;
		return 0;
	}

printf("%s: revents: %08x\n", __FUNCTION__, pfd.revents);
fflush(stdout);

	if (pfd.revents & (POLLIN | POLLHUP))
		return 1;

	return 0;
}

/* Tell the remote gdb that the process has exited.  */
void gdb_exit(CPUState *env, int code)
{
  GDBState *s;
  char buf[4];

  if (gdbserver_fd < 0)
    return;

  s = &gdbserver_state;

  snprintf(buf, sizeof(buf), "W%02x", code);
  put_packet(s, buf);
}


static void gdb_accept(void *opaque)
{
    GDBState *s;
    struct sockaddr_in sockaddr;
    socklen_t len;
    int val, fd;

    for(;;) {
        len = sizeof(sockaddr);
        fd = accept(gdbserver_fd, (struct sockaddr *)&sockaddr, &len);
        if (fd < 0 && errno != EINTR) {
            perror("accept");
            return;
        } else if (fd >= 0) {
            break;
        }
    }

    /* set short latency */
    val = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));
    
    s = &gdbserver_state;
    memset (s, 0, sizeof (GDBState));
    s->env = first_cpu; /* XXX: allow to change CPU */
    s->fd = fd;

    gdb_syscall_state = s;

    fcntl(fd, F_SETFL, O_NONBLOCK);
}

static int gdbserver_open(int port)
{
    struct sockaddr_in sockaddr;
    int fd, val, ret;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* allow fast reuse */
    val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = 0;
    ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }
    ret = listen(fd, 0);
    if (ret < 0) {
        perror("listen");
        return -1;
    }
    return fd;
}

int gdbserver_start(int port)
{
    gdbserver_fd = gdbserver_open(port);
    if (gdbserver_fd < 0)
        return -1;
    /* accept connections */
    gdb_accept (NULL);
    return 0;
}
