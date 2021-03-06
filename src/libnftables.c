/*
 * Copyright (c) 2017 Eric Leblond <eric@regit.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <nftables/libnftables.h>
#include <erec.h>
#include <mnl.h>
#include <parser.h>
#include <utils.h>
#include <iface.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int nft_netlink(struct nft_ctx *nft,
		       struct list_head *cmds, struct list_head *msgs,
		       struct mnl_socket *nf_sock)
{
	uint32_t batch_seqnum, seqnum = 0;
	struct nftnl_batch *batch;
	struct netlink_ctx ctx;
	struct cmd *cmd;
	struct mnl_err *err, *tmp;
	LIST_HEAD(err_list);
	int ret = 0;

	if (list_empty(cmds))
		return 0;

	batch = mnl_batch_init();

	batch_seqnum = mnl_batch_begin(batch, mnl_seqnum_alloc(&seqnum));
	list_for_each_entry(cmd, cmds, list) {
		memset(&ctx, 0, sizeof(ctx));
		ctx.msgs = msgs;
		ctx.seqnum = cmd->seqnum = mnl_seqnum_alloc(&seqnum);
		ctx.batch = batch;
		ctx.octx = &nft->output;
		ctx.nf_sock = nf_sock;
		ctx.cache = &nft->cache;
		ctx.debug_mask = nft->debug_mask;
		init_list_head(&ctx.list);
		ret = do_command(&ctx, cmd);
		if (ret < 0) {
			netlink_io_error(&ctx, &cmd->location,
					 "Could not process rule: %s",
					 strerror(errno));
			goto out;
		}
	}
	if (!nft->check)
		mnl_batch_end(batch, mnl_seqnum_alloc(&seqnum));

	if (!mnl_batch_ready(batch))
		goto out;

	ret = netlink_batch_send(&ctx, &err_list);

	list_for_each_entry_safe(err, tmp, &err_list, head) {
		list_for_each_entry(cmd, cmds, list) {
			if (err->seqnum == cmd->seqnum ||
			    err->seqnum == batch_seqnum) {
				netlink_io_error(&ctx, &cmd->location,
						 "Could not process rule: %s",
						 strerror(err->err));
				errno = err->err;
				if (err->seqnum == cmd->seqnum) {
					mnl_err_list_free(err);
					break;
				}
			}
		}
	}
out:
	mnl_batch_reset(batch);
	return ret;
}

static void nft_init(void)
{
	mark_table_init();
	realm_table_rt_init();
	devgroup_table_init();
	realm_table_meta_init();
	ct_label_table_init();
	gmp_init();
#ifdef HAVE_LIBXTABLES
	xt_init();
#endif
}

static void nft_exit(void)
{
	ct_label_table_exit();
	realm_table_rt_exit();
	devgroup_table_exit();
	realm_table_meta_exit();
	mark_table_exit();
}

int nft_ctx_add_include_path(struct nft_ctx *ctx, const char *path)
{
	char **tmp;
	int pcount = ctx->num_include_paths;

	tmp = realloc(ctx->include_paths, (pcount + 1) * sizeof(char *));
	if (!tmp)
		return -1;

	ctx->include_paths = tmp;

	if (asprintf(&ctx->include_paths[pcount], "%s", path) < 0)
		return -1;

	ctx->num_include_paths++;
	return 0;
}

void nft_ctx_clear_include_paths(struct nft_ctx *ctx)
{
	while (ctx->num_include_paths)
		xfree(ctx->include_paths[--ctx->num_include_paths]);

	xfree(ctx->include_paths);
	ctx->include_paths = NULL;
}

static void nft_ctx_netlink_init(struct nft_ctx *ctx)
{
	ctx->nf_sock = netlink_open_sock();
}

struct nft_ctx *nft_ctx_new(uint32_t flags)
{
	struct nft_ctx *ctx;

	nft_init();
	ctx = xzalloc(sizeof(struct nft_ctx));
	ctx->state = xzalloc(sizeof(struct parser_state));

	nft_ctx_add_include_path(ctx, DEFAULT_INCLUDE_PATH);
	ctx->parser_max_errors	= 10;
	init_list_head(&ctx->cache.list);
	ctx->flags = flags;
	ctx->output.output_fp = stdout;
	ctx->output.error_fp = stderr;

	if (flags == NFT_CTX_DEFAULT)
		nft_ctx_netlink_init(ctx);

	return ctx;
}

static ssize_t cookie_write(void *cptr, const char *buf, size_t buflen)
{
	struct cookie *cookie = cptr;

	if (!cookie->buflen) {
		cookie->buflen = buflen + 1;
		cookie->buf = xmalloc(cookie->buflen);
	} else if (cookie->pos + buflen >= cookie->buflen) {
		size_t newlen = cookie->buflen * 2;

		while (newlen <= cookie->pos + buflen)
			newlen *= 2;

		cookie->buf = xrealloc(cookie->buf, newlen);
		cookie->buflen = newlen;
	}
	memcpy(cookie->buf + cookie->pos, buf, buflen);
	cookie->pos += buflen;
	cookie->buf[cookie->pos] = '\0';

	return buflen;
}

static int init_cookie(struct cookie *cookie)
{
	cookie_io_functions_t cookie_fops = {
		.write = cookie_write,
	};

	if (cookie->orig_fp) { /* just rewind buffer */
		if (cookie->buflen) {
			cookie->pos = 0;
			cookie->buf[0] = '\0';
		}
		return 0;
	}

	cookie->orig_fp = cookie->fp;

	cookie->fp = fopencookie(cookie, "w", cookie_fops);
	if (!cookie->fp) {
		cookie->fp = cookie->orig_fp;
		return 1;
	}

	return 0;
}

static int exit_cookie(struct cookie *cookie)
{
	if (!cookie->orig_fp)
		return 1;

	fclose(cookie->fp);
	cookie->fp = cookie->orig_fp;
	free(cookie->buf);
	cookie->buf = NULL;
	cookie->buflen = 0;
	cookie->pos = 0;
	return 0;
}

int nft_ctx_buffer_output(struct nft_ctx *ctx)
{
	return init_cookie(&ctx->output.output_cookie);
}

int nft_ctx_unbuffer_output(struct nft_ctx *ctx)
{
	return exit_cookie(&ctx->output.output_cookie);
}

int nft_ctx_buffer_error(struct nft_ctx *ctx)
{
	return init_cookie(&ctx->output.error_cookie);
}

int nft_ctx_unbuffer_error(struct nft_ctx *ctx)
{
	return exit_cookie(&ctx->output.error_cookie);
}

static const char *get_cookie_buffer(struct cookie *cookie)
{
	fflush(cookie->fp);

	/* This is a bit tricky: Rewind the buffer for future use and return
	 * the old content at the same time. Therefore return an empty string
	 * if buffer position is zero, otherwise just rewind buffer position
	 * and return the unmodified buffer. */

	if (!cookie->pos)
		return "";

	cookie->pos = 0;
	return cookie->buf;
}

const char *nft_ctx_get_output_buffer(struct nft_ctx *ctx)
{
	return get_cookie_buffer(&ctx->output.output_cookie);
}

const char *nft_ctx_get_error_buffer(struct nft_ctx *ctx)
{
	return get_cookie_buffer(&ctx->output.error_cookie);
}

void nft_ctx_free(struct nft_ctx *ctx)
{
	if (ctx->nf_sock)
		netlink_close_sock(ctx->nf_sock);

	exit_cookie(&ctx->output.output_cookie);
	exit_cookie(&ctx->output.error_cookie);
	iface_cache_release();
	cache_release(&ctx->cache);
	nft_ctx_clear_include_paths(ctx);
	xfree(ctx->state);
	xfree(ctx);
	nft_exit();
}

FILE *nft_ctx_set_output(struct nft_ctx *ctx, FILE *fp)
{
	FILE *old = ctx->output.output_fp;

	if (!fp || ferror(fp))
		return NULL;

	ctx->output.output_fp = fp;

	return old;
}

FILE *nft_ctx_set_error(struct nft_ctx *ctx, FILE *fp)
{
	FILE *old = ctx->output.error_fp;

	if (!fp || ferror(fp))
		return NULL;

	ctx->output.error_fp = fp;

	return old;
}

bool nft_ctx_get_dry_run(struct nft_ctx *ctx)
{
	return ctx->check;
}

void nft_ctx_set_dry_run(struct nft_ctx *ctx, bool dry)
{
	ctx->check = dry;
}

enum nft_numeric_level nft_ctx_output_get_numeric(struct nft_ctx *ctx)
{
	return ctx->output.numeric;
}

void nft_ctx_output_set_numeric(struct nft_ctx *ctx,
				enum nft_numeric_level level)
{
	ctx->output.numeric = level;
}

bool nft_ctx_output_get_stateless(struct nft_ctx *ctx)
{
	return ctx->output.stateless;
}

void nft_ctx_output_set_stateless(struct nft_ctx *ctx, bool val)
{
	ctx->output.stateless = val;
}

bool nft_ctx_output_get_ip2name(struct nft_ctx *ctx)
{
	return ctx->output.ip2name;
}

void nft_ctx_output_set_ip2name(struct nft_ctx *ctx, bool val)
{
	ctx->output.ip2name = val;
}

unsigned int nft_ctx_output_get_debug(struct nft_ctx *ctx)
{
	return ctx->debug_mask;
}
void nft_ctx_output_set_debug(struct nft_ctx *ctx, unsigned int mask)
{
	ctx->debug_mask = mask;
}

bool nft_ctx_output_get_handle(struct nft_ctx *ctx)
{
	return ctx->output.handle;
}

void nft_ctx_output_set_handle(struct nft_ctx *ctx, bool val)
{
	ctx->output.handle = val;
}

bool nft_ctx_output_get_echo(struct nft_ctx *ctx)
{
	return ctx->output.echo;
}

void nft_ctx_output_set_echo(struct nft_ctx *ctx, bool val)
{
	ctx->output.echo = val;
}

bool nft_ctx_output_get_json(struct nft_ctx *ctx)
{
#ifdef HAVE_LIBJANSSON
	return ctx->output.json;
#else
	return false;
#endif
}

void nft_ctx_output_set_json(struct nft_ctx *ctx, bool val)
{
#ifdef HAVE_LIBJANSSON
	ctx->output.json = val;
#endif
}

static const struct input_descriptor indesc_cmdline = {
	.type	= INDESC_BUFFER,
	.name	= "<cmdline>",
};

static int nft_parse_bison_buffer(struct nft_ctx *nft, char *buf, size_t buflen,
				  struct list_head *msgs, struct list_head *cmds)
{
	struct cmd *cmd;
	int ret;

	parser_init(nft, nft->state, msgs, cmds);
	nft->scanner = scanner_init(nft->state);
	scanner_push_buffer(nft->scanner, &indesc_cmdline, buf);

	ret = nft_parse(nft, nft->scanner, nft->state);
	if (ret != 0 || nft->state->nerrs > 0)
		return -1;

	list_for_each_entry(cmd, cmds, list)
		nft_cmd_expand(cmd);

	return 0;
}

static int nft_parse_bison_filename(struct nft_ctx *nft, const char *filename,
				    struct list_head *msgs, struct list_head *cmds)
{
	struct cmd *cmd;
	void *scanner;
	int ret;

	parser_init(nft, nft->state, msgs, cmds);
	scanner = scanner_init(nft->state);
	if (scanner_read_file(scanner, filename, &internal_location) < 0)
		return -1;

	ret = nft_parse(nft, scanner, nft->state);
	if (ret != 0 || nft->state->nerrs > 0)
		return -1;

	list_for_each_entry(cmd, cmds, list)
		nft_cmd_expand(cmd);

	return 0;
}

int nft_run_cmd_from_buffer(struct nft_ctx *nft, char *buf, size_t buflen)
{
	struct cmd *cmd, *next;
	LIST_HEAD(msgs);
	LIST_HEAD(cmds);
	size_t nlbuflen;
	char *nlbuf;
	int rc = -EINVAL;

	nlbuflen = max(buflen + 1, strlen(buf) + 2);
	nlbuf = xzalloc(nlbuflen);
	snprintf(nlbuf, nlbuflen, "%s\n", buf);

	if (nft->output.json)
		rc = nft_parse_json_buffer(nft, nlbuf, nlbuflen, &msgs, &cmds);
	if (rc == -EINVAL)
		rc = nft_parse_bison_buffer(nft, nlbuf, nlbuflen, &msgs, &cmds);
	if (rc)
		goto err;

	if (nft_netlink(nft, &cmds, &msgs, nft->nf_sock) != 0)
		rc = -1;
err:
	list_for_each_entry_safe(cmd, next, &cmds, list) {
		list_del(&cmd->list);
		cmd_free(cmd);
	}
	erec_print_list(&nft->output, &msgs, nft->debug_mask);
	iface_cache_release();
	if (nft->scanner) {
		scanner_destroy(nft->scanner);
		nft->scanner = NULL;
	}
	free(nlbuf);

	return rc;
}

int nft_run_cmd_from_filename(struct nft_ctx *nft, const char *filename)
{
	struct cmd *cmd, *next;
	LIST_HEAD(msgs);
	LIST_HEAD(cmds);
	int rc;

	rc = cache_update(nft->nf_sock, &nft->cache, CMD_INVALID, &msgs,
			  nft->debug_mask, &nft->output);
	if (rc < 0)
		return -1;

	if (!strcmp(filename, "-"))
		filename = "/dev/stdin";

	rc = -EINVAL;
	if (nft->output.json)
		rc = nft_parse_json_filename(nft, filename, &msgs, &cmds);
	if (rc == -EINVAL)
		rc = nft_parse_bison_filename(nft, filename, &msgs, &cmds);
	if (rc)
		goto err;

	if (nft_netlink(nft, &cmds, &msgs, nft->nf_sock) != 0)
		rc = -1;
err:
	list_for_each_entry_safe(cmd, next, &cmds, list) {
		list_del(&cmd->list);
		cmd_free(cmd);
	}
	erec_print_list(&nft->output, &msgs, nft->debug_mask);
	iface_cache_release();
	if (nft->scanner) {
		scanner_destroy(nft->scanner);
		nft->scanner = NULL;
	}

	return rc;
}

int nft_print(struct output_ctx *octx, const char *fmt, ...)
{
	int ret;
	va_list arg;

	va_start(arg, fmt);
	ret = vfprintf(octx->output_fp, fmt, arg);
	va_end(arg);
	fflush(octx->output_fp);

	return ret;
}

int nft_gmp_print(struct output_ctx *octx, const char *fmt, ...)
{
	int ret;
	va_list arg;

	va_start(arg, fmt);
	ret = gmp_vfprintf(octx->output_fp, fmt, arg);
	va_end(arg);
	fflush(octx->output_fp);

	return ret;
}

