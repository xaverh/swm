/**
 *      Copyright (c) 2014, Broseph <dcat (at) iotek (dot) org>
 *
 *      Permission to use, copy, modify, and/or distribute this software for any
 *      purpose with or without fee is hereby granted, provided that the above
 *      copyright notice and this permission notice appear in all copies.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *      WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *      MERCHANTABILITY AND FITNESS IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *      ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *      WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *      ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *      OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **/

#include "config.hxx"

#include <cstdlib>
#include <err.h>
#include <unistd.h>
#include <xcb/xcb.h>

namespace {
/* global variables */
xcb_connection_t* conn;
xcb_screen_t* scr;
xcb_window_t focuswin;

void cleanup()
{
	/* graceful exit */
	if (conn != nullptr) xcb_disconnect(conn);
}

int deploy()
{
	/* init xcb and grab events */
	uint32_t values[2];

	if (xcb_connection_has_error(conn = xcb_connect(nullptr, nullptr))) return -1;

	scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	focuswin = scr->root;

	xcb_grab_button(conn,
	                0,
	                scr->root,
	                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
	                XCB_GRAB_MODE_ASYNC,
	                XCB_GRAB_MODE_ASYNC,
	                scr->root,
	                XCB_NONE,
	                1,
	                MOD);

	xcb_grab_button(conn,
	                0,
	                scr->root,
	                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
	                XCB_GRAB_MODE_ASYNC,
	                XCB_GRAB_MODE_ASYNC,
	                scr->root,
	                XCB_NONE,
	                3,
	                MOD);

	int mask = XCB_CW_EVENT_MASK;
	values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
	xcb_change_window_attributes_checked(conn, scr->root, mask, values);

	xcb_flush(conn);

	return 0;
}

void focus(xcb_window_t win, bool active)
{
	uint32_t values[1];
#if OUTER > 0
	if (!win) return;

	xcb_get_geometry_reply_t* geom
	    = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), nullptr);

	if (geom == nullptr) return;

	short w = (short)geom->width;
	short h = (short)geom->height;
	short b = (unsigned short)geom->border_width;

	xcb_rectangle_t inner[] = {{w, 0, b - OUTER, h + b - OUTER},
	                           {w + b + OUTER, 0, b - OUTER, h + b - OUTER},
	                           {0, h, w + b - OUTER, b - OUTER},
	                           {0, h + b + OUTER, w + b - OUTER, b - OUTER},
	                           {w + b + OUTER, b + h + OUTER, b, b}};

	xcb_rectangle_t outer[] = {{w + b - OUTER, 0, OUTER, h + b * 2},
	                           {w + b, 0, OUTER, h + b * 2},
	                           {0, h + b - OUTER, w + b * 2, OUTER},
	                           {0, h + b, w + b * 2, OUTER},
	                           {1, 1, 1, 1}};

	xcb_pixmap_t pmap = xcb_generate_id(conn);
	xcb_create_pixmap(conn,
	                  scr->root_depth,
	                  pmap,
	                  win,
	                  geom->width + (geom->border_width * 2),
	                  geom->height + (geom->border_width * 2));
	xcb_gcontext_t gc = xcb_generate_id(conn);
	xcb_create_gc(conn, gc, pmap, 0, nullptr);

	values[0] = OUTERCOL;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, values);
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, outer);

	values[0] = mode ? FOCUSCOL : UNFOCUSCOL;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, values);
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, inner);

	values[0] = pmap;
	xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXMAP, values);

	xcb_free_pixmap(conn, pmap);
	xcb_free_gc(conn, gc);
#else
	values[0] = active ? FOCUSCOL : UNFOCUSCOL;
	xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, values);
#endif

	if (active) {
		xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
		if (win != focuswin) {
			focus(focuswin, false);
			focuswin = win;
		}
	}
}

void subscribe(xcb_window_t win)
{
	/* subscribe to events */
	uint32_t values[2] = {XCB_EVENT_MASK_ENTER_WINDOW, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	xcb_change_window_attributes(conn, win, XCB_CW_EVENT_MASK, values);

	/* border width */
	values[0] = BORDERWIDTH;
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
}

void events_loop()
{
	xcb_generic_event_t* ev = nullptr;
	uint32_t values[3];
	xcb_get_geometry_reply_t* geom = nullptr;
	xcb_window_t win = 0;

	/* loop */
	for (;;) {
		ev = xcb_wait_for_event(conn);

		if (!ev) errx(EXIT_FAILURE, "xcb connection broken");

		switch (((ev->response_type & ~0x80))) {
		case XCB_CREATE_NOTIFY: {
			auto e = reinterpret_cast<xcb_create_notify_event_t*>(ev);
			if (!e->override_redirect) {
				subscribe(e->window);
				focus(e->window, true);
			}
		} break;

		case XCB_DESTROY_NOTIFY: {
			auto e = reinterpret_cast<xcb_destroy_notify_event_t*>(ev);
			xcb_kill_client(conn, e->window);
		} break;

		case XCB_ENTER_NOTIFY: {
			auto e = reinterpret_cast<xcb_enter_notify_event_t*>(ev);
			focus(e->event, true);
		} break;

		case XCB_MAP_NOTIFY: {
			auto e = reinterpret_cast<xcb_map_notify_event_t*>(ev);
			if (!e->override_redirect) {
				xcb_map_window(conn, e->window);
				focus(e->window, true);
			}
		} break;

		case XCB_BUTTON_PRESS: {
			auto e = reinterpret_cast<xcb_button_press_event_t*>(ev);
			win = e->child;

			if (!win || win == scr->root) break;

			values[0] = XCB_STACK_MODE_ABOVE;
			xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
			geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), nullptr);
			if (e->detail == 1) {
				values[2] = 1;
				xcb_warp_pointer(conn,
				                 XCB_NONE,
				                 win,
				                 0,
				                 0,
				                 0U,
				                 0U,
				                 geom->width / 2,
				                 geom->height / 2);
			} else {
				values[2] = 3;
				xcb_warp_pointer(conn, XCB_NONE, win, 0, 0, 0U, 0U, geom->width, geom->height);
			}
			xcb_grab_pointer(conn,
			                 0,
			                 scr->root,
			                 XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION
			                     | XCB_EVENT_MASK_POINTER_MOTION_HINT,
			                 XCB_GRAB_MODE_ASYNC,
			                 XCB_GRAB_MODE_ASYNC,
			                 scr->root,
			                 XCB_NONE,
			                 XCB_CURRENT_TIME);
			xcb_flush(conn);
		} break;

		case XCB_MOTION_NOTIFY: {
			xcb_query_pointer_reply_t* pointer
			    = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, scr->root), nullptr);
			if (values[2] == 1) {
				geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), nullptr);
				if (!geom) break;

				values[0]
				    = (pointer->root_x + geom->width / 2 > scr->width_in_pixels - (BORDERWIDTH * 2))
				          ? scr->width_in_pixels - geom->width - (BORDERWIDTH * 2)
				          : pointer->root_x - geom->width / 2;
				values[1] = (pointer->root_y + geom->height / 2
				             > scr->height_in_pixels - (BORDERWIDTH * 2))
				                ? (scr->height_in_pixels - geom->height - (BORDERWIDTH * 2))
				                : pointer->root_y - geom->height / 2;

				if (pointer->root_x < geom->width / 2) values[0] = 0;
				if (pointer->root_y < geom->height / 2) values[1] = 0;

				xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(conn);
			} else if (values[2] == 3) {
				geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), nullptr);
				values[0] = pointer->root_x - geom->x;
				values[1] = pointer->root_y - geom->y;
				xcb_configure_window(conn,
				                     win,
				                     XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
				                     values);
				xcb_flush(conn);
			}
		} break;

		case XCB_BUTTON_RELEASE:
			focus(win, true);
			xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
			break;

		case XCB_CONFIGURE_NOTIFY: {
			auto e = reinterpret_cast<xcb_configure_notify_event_t*>(ev);

			if (e->window != focuswin) focus(e->window, true);

			focus(focuswin, true);
		} break;
		}

		xcb_flush(conn);
		free(ev);
	}
}
} // namespace

int main()
{
	/* graceful exit */
	atexit(cleanup);

	if (deploy() < 0) errx(EXIT_FAILURE, "error connecting to X");

	for (;;) events_loop();

	return EXIT_FAILURE;
}
