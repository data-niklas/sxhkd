/* Copyright (c) 2013, Bastien Dejean
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "sxhkd.h"
#include <xcb/xcb.h>

void warn(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

__attribute__((noreturn))
void err(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

/*
xcb_window_t get_active_window(){
	xcb_get_input_focus_cookie_t cookie;
	xcb_get_input_focus_reply_t *reply;

	cookie = xcb_get_input_focus(dpy);
	if ((reply = xcb_get_input_focus_reply(dpy, cookie, NULL))){
		xcb_window_t window = reply->focus;
		free(reply);
		return window;
	}
	else{
		free(reply);
		return NULL;
	}
}
*/

xcb_window_t get_active_window(){
	xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(dpy, 0, strlen("_NET_ACTIVE_WINDOW"), "_NET_ACTIVE_WINDOW");
	xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(dpy, atom_cookie, NULL);
	
	if (!atom_reply){
		free(atom_reply);
		return NULL;
	}
	xcb_atom_t atom = atom_reply->atom;
	free(atom_reply);

	xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(dpy)).data->root;
	xcb_get_property_reply_t *reply = get_window_property(root, atom, XCB_ATOM_CARDINAL);
	if (!reply){
		free(reply);
		return NULL;
	}
	xcb_window_t window = *(uint32_t*)xcb_get_property_value(reply);
	free(reply);
	return window;
}



xcb_get_property_reply_t *get_window_property(xcb_window_t window, xcb_atom_t property, xcb_atom_enum_t type){
	xcb_get_property_cookie_t cookie;
	//Only works with strings
	
	cookie = xcb_get_property(dpy, 0, window, property, type, 0, 0);
	return xcb_get_property_reply(dpy, cookie, NULL);
}

xcb_window_t getActiveWindow()
{
    xcb_get_input_focus_reply_t* focusReply;
    xcb_query_tree_cookie_t treeCookie;
    xcb_query_tree_reply_t* treeReply;

    focusReply = xcb_get_input_focus_reply(dpy, xcb_get_input_focus(dpy), NULL);
    xcb_window_t window = focusReply->focus;
    while (1) {
        treeCookie = xcb_query_tree(dpy, window);
        treeReply = xcb_query_tree_reply(dpy, treeCookie, NULL);
        if (!treeReply) {
            window = 0;
            break;
        }
        if (window == treeReply->root || treeReply->parent == treeReply->root) {
            break;
        } else {
            window = treeReply->parent;
        }
        free(treeReply);
    }
    free(treeReply);
    return window;
}

void run(char *command, bool sync)
{

	xcb_window_t window = get_active_window();
	xcb_get_property_reply_t *name_reply = get_window_property(window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING);

	if (name_reply) {
		int len = xcb_get_property_value_length(name_reply);
		if (len == 0) {
			printf("TODO\n");
			free(name_reply);
			return;
		}
		printf("WM_NAME is %.*s\n", len,
				(char*)xcb_get_property_value(name_reply));
	}
	else{
		printf("No Property reply\n");
	}
	free(name_reply);

    char *cmd[] = {shell, "-c", command, NULL};
    spawn(cmd, sync);
}

void spawn(char *cmd[], bool sync)
{
	if (fork() == 0) {
		if (dpy != NULL)
			close(xcb_get_file_descriptor(dpy));
		if (sync) {
			execute(cmd);
		} else {
			if (fork() == 0) {
				execute(cmd);
			}
			exit(EXIT_SUCCESS);
		}
	}
	wait(NULL);
}

void execute(char *cmd[])
{
	setsid();
	if (redir_fd != -1) {
		dup2(redir_fd, STDOUT_FILENO);
		dup2(redir_fd, STDERR_FILENO);
	}
	execvp(cmd[0], cmd);
	err("Spawning failed.\n");
}

char *lgraph(char *s)
{
	size_t len = strlen(s);
	unsigned int i = 0;
	while (i < len && !isgraph(s[i]))
		i++;
	if (i < len)
		return (s + i);
	else
		return NULL;
}

char *rgraph(char *s)
{
	int i = strlen(s) - 1;
	while (i >= 0 && !isgraph(s[i]))
		i--;
	if (i >= 0)
		return (s + i);
	else
		return NULL;
}
