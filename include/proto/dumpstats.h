/*
 * include/proto/dumpstats.h
 * This file contains definitions of some primitives to dedicated to
 * statistics output.
 *
 * Copyright (C) 2000-2011 Willy Tarreau - w@1wt.eu
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, version 2.1
 * exclusively.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _PROTO_DUMPSTATS_H
#define _PROTO_DUMPSTATS_H

#include <common/config.h>
#include <types/applet.h>
#include <types/stream_interface.h>

/* Flags for applet.ctx.stats.flags */
#define STAT_FMT_HTML   0x00000001      /* dump the stats in HTML format */
#define STAT_FMT_TYPED  0x00000002      /* use the typed output format */
#define STAT_HIDE_DOWN  0x00000008	/* hide 'down' servers in the stats page */
#define STAT_NO_REFRESH 0x00000010	/* do not automatically refresh the stats page */
#define STAT_ADMIN      0x00000020	/* indicate a stats admin level */
#define STAT_CHUNKED    0x00000040      /* use chunked encoding (HTTP/1.1) */
#define STAT_BOUND      0x00800000	/* bound statistics to selected proxies/types/services */

#define STATS_TYPE_FE  0
#define STATS_TYPE_BE  1
#define STATS_TYPE_SV  2
#define STATS_TYPE_SO  3

/* HTTP stats : applet.st0 */
enum {
	STAT_HTTP_DONE = 0,  /* finished */
	STAT_HTTP_HEAD,      /* send headers before dump */
	STAT_HTTP_DUMP,      /* dumping stats */
	STAT_HTTP_POST,      /* waiting post data */
	STAT_HTTP_LAST,      /* sending last chunk of response */
};

/* HTML form to limit output scope */
#define STAT_SCOPE_TXT_MAXLEN 20      /* max len for scope substring */
#define STAT_SCOPE_INPUT_NAME "scope" /* pattern form scope name <input> in html form */
#define STAT_SCOPE_PATTERN    "?" STAT_SCOPE_INPUT_NAME "="

/* This level of detail is needed to let the stats consumer know how to
 * aggregate them (eg: between processes or cluster nodes). Only a few
 * combinations are actually in use, though the mechanism tends to make
 * this easy to extend to future uses.
 *
 * Each reported stats element is typed based on 4 dimensions :
 *  - the field format : it indicates the validity range of the reported value,
 *    its limits and how to parse it. 6 types are currently supported :
 *    empty, signed 32-bit integer, unsigned 32-bit integer, signed 64-bit
 *    integer, unsigned 64-bit integer, string
 *
 *  - the field origin : how was the value retrieved and what it depends on.
 *    5 origins are currently defined : product (eg: haproxy version or
 *    release date), configuration (eg: a configured limit), key (identifier
 *    used to group values at a certain level), metric (a measure of something),
 *    status (something discrete which by definition cannot be averaged nor
 *    aggregated, such as "listening" versus "full").
 *
 *  - the field nature : what does the data represent, implying how to aggregate
 *    it. At least 9 different natures are expected : counter (an increasing
 *    positive counter that may wrap when its type is overflown such as a byte
 *    counter), gauge (a measure at any instant that may vary, such as a
 *    concurrent connection count), a limit (eg: maximum acceptable concurrent
 *    connections), a minimum (eg: minimum free memory over a period), a
 *    maximum (eg: highest queue length over a period), an event rate (eg:
 *    incoming connections per second), a duration that is often aggregated by
 *    taking the max (eg: service uptime), an age that generally reports the
 *    last time an event appeared and which generally is aggregated by taking
 *    the most recent event hence the smallest one, the time which reports a
 *    discrete instant and cannot obviously be averaged either, a name which
 *    will generally be the name of an entity (such as a server name or cookie
 *    name), an output which is mostly used for various unsafe strings that are
 *    retrieved (eg: last check output, product name, description, etc), and an
 *    average which indicates that the value is relative and meant to be averaged
 *    between all nodes (eg: response time, throttling, etc).
 *
 *  - the field scope : if the value is shared with other elements, which ones
 *    are expected to report the same value. The first scope with the least
 *    share is the process (most common one) where all data are only relevant
 *    to the process being consulted. The next one is the service, which is
 *    valid for all processes launched together (eg: shared SSL cache usage
 *    among processes). The next one is the system (such as the OS version)
 *    and which will report the same information for all instances running on
 *    the same node. The next one is the cluster, which indicates that the
 *    information are shared with other nodes being part of a same cluster.
 *    Stick-tables may carry such cluster-wide information. Larger scopes may
 *    be added in the future such as datacenter, country, continent, planet,
 *    galaxy, universe, etc.
 *
 * All these information will be encoded in the field as a bit field so that
 * it is easy to pass composite values by simply ORing elements above, and
 * to ease the definition of a few field types for the most common field
 * combinations.
 *
 * The enums try to be arranged so that most likely characteristics are
 * assigned the value zero, making it easier to add new fields.
 *
 * Field format has precedence over the other parts of the type. Please avoid
 * declaring extra formats unless absolutely needed. The first one, FF_EMPTY,
 * must absolutely have value zero so that it is what is returned after a
 * memset(0). Furthermore, the producer is responsible for ensuring that when
 * this format is set, all other bits of the type as well as the values in the
 * union only contain zeroes. This makes it easier for the consumer to use the
 * values as the expected type.
 */

enum field_format {
	FF_EMPTY    = 0x00000000,
	FF_S32      = 0x00000001,
	FF_U32      = 0x00000002,
	FF_S64      = 0x00000003,
	FF_U64      = 0x00000004,
	FF_STR      = 0x00000005,
	FF_MASK     = 0x000000FF,
};

enum field_origin {
	FO_METRIC   = 0x00000000,
	FO_STATUS   = 0x00000100,
	FO_KEY      = 0x00000200,
	FO_CONFIG   = 0x00000300,
	FO_PRODUCT  = 0x00000400,
	FO_MASK     = 0x0000FF00,
};

enum field_nature {
	FN_GAUGE    = 0x00000000,
	FN_LIMIT    = 0x00010000,
	FN_MIN      = 0x00020000,
	FN_MAX      = 0x00030000,
	FN_RATE     = 0x00040000,
	FN_COUNTER  = 0x00050000,
	FN_DURATION = 0x00060000,
	FN_AGE      = 0x00070000,
	FN_TIME     = 0x00080000,
	FN_NAME     = 0x00090000,
	FN_OUTPUT   = 0x000A0000,
	FN_AVG      = 0x000B0000,
	FN_MASK     = 0x00FF0000,
};

enum field_scope {
	FS_PROCESS  = 0x00000000,
	FS_SERVICE  = 0x01000000,
	FS_SYSTEM   = 0x02000000,
	FS_CLUSTER  = 0x03000000,
	FS_MASK     = 0xFF000000,
};

struct field {
	uint32_t type;
	union {
		int32_t     s32; /* FF_S32 */
		uint32_t    u32; /* FF_U32 */
		int64_t     s64; /* FF_S64 */
		uint64_t    u64; /* FF_U64 */
		const char *str; /* FF_STR */
	} u;
};

static inline enum field_format field_format(const struct field *f, int e)
{
	return f[e].type & FF_MASK;
}

static inline enum field_origin field_origin(const struct field *f, int e)
{
	return f[e].type & FO_MASK;
}

static inline enum field_scope field_scope(const struct field *f, int e)
{
	return f[e].type & FS_MASK;
}

static inline enum field_nature field_nature(const struct field *f, int e)
{
	return f[e].type & FN_MASK;
}

static inline const char *field_str(const struct field *f, int e)
{
	return (field_format(f, e) == FF_STR) ? f[e].u.str : "";
}

static inline struct field mkf_s32(uint32_t type, int32_t value)
{
	struct field f = { .type = FF_S32 | type, .u.s32 = value };
	return f;
}

static inline struct field mkf_u32(uint32_t type, uint32_t value)
{
	struct field f = { .type = FF_U32 | type, .u.u32 = value };
	return f;
}

static inline struct field mkf_s64(uint32_t type, int64_t value)
{
	struct field f = { .type = FF_S64 | type, .u.s64 = value };
	return f;
}

static inline struct field mkf_u64(uint32_t type, uint64_t value)
{
	struct field f = { .type = FF_U64 | type, .u.u64 = value };
	return f;
}

static inline struct field mkf_str(uint32_t type, const char *value)
{
	struct field f = { .type = FF_STR | type, .u.str = value };
	return f;
}

extern struct applet http_stats_applet;

void stats_io_handler(struct stream_interface *si);
int stats_emit_raw_data_field(struct chunk *out, const struct field *f);
int stats_emit_typed_data_field(struct chunk *out, const struct field *f);
int stats_emit_field_tags(struct chunk *out, const struct field *f, char delim);


#endif /* _PROTO_DUMPSTATS_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 */
