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

void warn(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

__attribute__((noreturn)) void err(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

Window get_focus_window(Display *d)
{
	Window w;
	int revert_to;
	XGetInputFocus(d, &w, &revert_to); // see man
	return w;
}

// get the top window.
// a top window have the following specifications.
//  * the start window is contained the descendent windows.
//  * the parent window is the root window.
Window get_top_window(Display *d, Window start)
{
	Window w = start;
	Window parent = start;
	Window root = None;
	Window *children;
	unsigned int nchildren;
	Status s;

	while (parent != root)
	{
		w = parent;
		s = XQueryTree(d, w, &root, &parent, &children, &nchildren); // see man

		if (s)
			XFree(children);
	}

	return w;
}

// search a named window (that has a WM_STATE prop)
// on the descendent windows of the argment Window.
Window get_named_window(Display *d, Window start)
{
	Window w;
	w = XmuClientWindow(d, start); // see man
	return w;
}

// (XFetchName cannot get a name with multi-byte chars)
char *get_window_name(Display *d, Window w)
{
	XTextProperty prop;
	Status s;

	s = XGetWMName(d, w, &prop); // see man
	if (s)
	{
		int count = 0, result;
		char **list = NULL;
		result = XmbTextPropertyToTextList(d, &prop, &list, &count); // see man
		if (result == Success)
		{
			return list[0];
		}
		else
		{
			return "";
		}
	}
	else
	{
		return "";
	}
}

XClassHint *get_window_class(Display *d, Window w)
{
	Status s;
	XClassHint *class;

	class = XAllocClassHint(); // see man

	s = XGetClassHint(d, w, class); // see man
	if (s)
	{
		return class;
	}
	return NULL;
}

void get_window_bounds(Display *d, Window w, int *x, int *y, unsigned int *width, unsigned int *height)
{
	unsigned int depth;
	unsigned int border;
	Window dummy;
	if (XGetGeometry(d, w, &dummy, x, y, width, height, &border, &depth) == BadWindow)
	{
		*x = 0;
		*y = 0;
		*width = 0;
		*height = 0;
	}
}

char* itoa(int val){
	
	static char buf[32] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= 10)
	
		buf[i] = "0123456789"[val % 10];
	
	return &buf[i+1];
	
}

void run(char *command, bool sync)
{
	Window w = get_focus_window(d);
	setenv("SXHKD_NAME", get_window_name(d, w), 1);
	XClassHint *class = get_window_class(d, w);
	if (class)
	{
		setenv("SXHKD_CLASS_NAME", class->res_name, 1);
		setenv("SXHKD_CLASS", class->res_class, 1);
	}
	else
	{
		setenv("SXHKD_CLASS_NAME", "", 1);
		setenv("SXHKD_CLASS", "", 1);
	}
	int x, y;
	unsigned int width, height;
	get_window_bounds(d, w, &x, &y, &width, &height);
	//And then set them as env variables
	setenv("SXHKD_X", itoa(x), 1);
	setenv("SXHKD_Y", itoa(y), 1);
	setenv("SXHKD_WIDTH", itoa(width), 1);
	setenv("SXHKD_HEIGHT", itoa(height), 1);

	char *cmd[] = {shell, "-c", command, NULL};
	spawn(cmd, sync);
}

void spawn(char *cmd[], bool sync)
{
	if (fork() == 0)
	{
		if (dpy != NULL)
			close(xcb_get_file_descriptor(dpy));
		if (sync)
		{
			execute(cmd);
		}
		else
		{
			if (fork() == 0)
			{
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
	if (redir_fd != -1)
	{
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
