/* $Id: cmd-link-window.c,v 1.28 2009-01-23 16:59:14 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include "tmux.h"

/*
 * Link a window into another session.
 */

int	cmd_link_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_link_window_entry = {
	"link-window", "linkw",
	"[-dk] " CMD_SRCDST_WINDOW_USAGE,
	CMD_DFLAG|CMD_KFLAG,
	cmd_srcdst_init,
	cmd_srcdst_parse,
	cmd_link_window_exec,
	cmd_srcdst_send,
	cmd_srcdst_recv,
	cmd_srcdst_free,
	cmd_srcdst_print
};

int
cmd_link_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_srcdst_data	*data = self->data;
	struct session		*dst;
	struct winlink		*wl_src, *wl_dst;
	char			*cause;
	int			 idx;

	if ((wl_src = cmd_find_window(ctx, data->src, NULL)) == NULL)
		return (-1);

	if (arg_parse_window(data->dst, &dst, &idx) != 0) {
		ctx->error(ctx, "bad window: %s", data->dst);
		return (-1);
	}
	if (dst == NULL)
		dst = ctx->cursession;
	if (dst == NULL)
		dst = cmd_current_session(ctx);
	if (dst == NULL) {
		ctx->error(ctx, "session not found: %s", data->dst);
		return (-1);
	}

	wl_dst = NULL;
	if (idx != -1)
		wl_dst = winlink_find_by_index(&dst->windows, idx);
	if (wl_dst != NULL) {
		if (wl_dst->window == wl_src->window)
			return (0);

		if (data->flags & CMD_KFLAG) {
			/*
			 * Can't use session_detach as it will destroy session
			 * if this makes it empty.
			 */
			session_alert_cancel(dst, wl_dst);
			winlink_stack_remove(&dst->lastw, wl_dst);
			winlink_remove(&dst->windows, wl_dst);

			/* Force select/redraw if current. */
			if (wl_dst == dst->curw) {
				data->flags &= ~CMD_DFLAG;
				dst->curw = NULL;
			}
		}
	}

	wl_dst = session_attach(dst, wl_src->window, idx, &cause);
	if (wl_dst == NULL) {
		ctx->error(ctx, "create session failed: %s", cause);
		xfree(cause);
		return (-1);
	}

	if (data->flags & CMD_DFLAG)
		server_status_session(dst);
	else {
		session_select(dst, wl_dst->idx);
		server_redraw_session(dst);
	}
	recalculate_sizes();

	return (0);
}
