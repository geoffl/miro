/*
# Miro - an RSS based video player application
# Copyright (C) 2011
# Participatory Culture Foundation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
# In addition, as a special exception, the copyright holders give
# permission to link the code of portions of this program with the OpenSSL
# library.
#
# You must obey the GNU General Public License in all respects for all of
# the code used other than OpenSSL. If you modify file(s) with this
# exception, you may extend this exception to your version of the file(s),
# but you are not obligated to do so. If you do not wish to do so, delete
# this exception statement from your version. If you delete this exception
# statement from all source files in the program, then also delete it here.
*/

/* embeddingwindow -- Create Windows for embedding
 *
 * This module handles creating HWNDs to use to embed other components like
 * gstreamer.
 *
 * To keep things simple on the C side, we don't define an extension type.
 * Instead, most functions either input a HWND to work on.  We cast HWNDs to
 * python int objects.
 *
 */

#include <windows.h>
#include <windowsx.h>
#include <Python.h>

static int initialized = 0;
static HWND hidden_window = 0;
static TCHAR window_class_name[] = TEXT("Miro Embedding Window");

/*
 * WindowInfo -- Data that we attach to each window
 */
typedef struct {
    unsigned char enable_motion;
    PyObject* event_handler;
    int last_mouse_x;
    int last_mouse_y;
} WindowInfo;

/*
 * call_event_handler -- Call a method on the event handler for a window
 */
void call_event_handler(WindowInfo* window_info, const char* method_name,
                        PyObject *args)
{
    PyObject* method = NULL;
    PyObject* result = NULL;
    PyGILState_STATE gstate;

    /* Before anything, make sure we have the GIL */
    gstate = PyGILState_Ensure();

    /* Try to fetch our event_handler python object */
    if(!window_info->event_handler) {
        PyErr_SetString(PyExc_TypeError, "event_handler is NULL");
        goto error;
    }

    /* Call the method for this event */
    method = PyObject_GetAttrString(window_info->event_handler, method_name);
    if(!method) goto error;

    result = PyObject_CallObject(method, args);
    if(!result) goto error;

    goto finally;

error:
    /* Error handling code */
    PyErr_Print();
    PyErr_Clear();

finally:
    /* cleanup code */
    Py_XDECREF(method);
    Py_XDECREF(result);
    PyGILState_Release(gstate);
}


LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    PyObject* arglist;
    WindowInfo* window_info;
    int x, y;

    switch (msg) {
        case WM_MOUSEMOVE:
            window_info = (WindowInfo*) GetWindowLongPtr((HWND)hwnd,
                                                         GWLP_USERDATA);
            x = GET_X_LPARAM(lparam);
            y = GET_Y_LPARAM(lparam);
            if(window_info && window_info->enable_motion) {
                /* Check that we actually have a new coordinate.  When we
                 * map/unmap windows, we get an extra WM_MOUSEMOVE message.
                 */
                if(x == window_info->last_mouse_x &&
                   y == window_info->last_mouse_y) {
                    break;
                }
                window_info->last_mouse_x = x;
                window_info->last_mouse_y = y;
                arglist = Py_BuildValue("(ii)", GET_X_LPARAM(lparam),
                                        GET_Y_LPARAM(lparam));
                call_event_handler(window_info, "on_mouse_move", arglist);
                Py_DECREF(arglist);
            }
            break;

        case WM_PAINT:
            window_info = (WindowInfo*) GetWindowLongPtr((HWND)hwnd,
                                                         GWLP_USERDATA);
            if(window_info) {
                arglist = Py_BuildValue("()");
                call_event_handler(window_info, "on_paint", arglist);
                Py_DECREF(arglist);
            }
            break;

        case WM_LBUTTONDBLCLK:
            window_info = (WindowInfo*) GetWindowLongPtr((HWND)hwnd,
                                                         GWLP_USERDATA);
            if(window_info) {
                arglist = Py_BuildValue("(ii)", GET_X_LPARAM(lparam),
                                        GET_Y_LPARAM(lparam));
                call_event_handler(window_info, "on_double_click", arglist);
                Py_DECREF(arglist);
            }
            break;

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
}


static int ensure_initialized()
{
    WNDCLASSEX wcex;
    HANDLE hInstance;

    if(initialized) return 1;

    /* Register the window class */
    hInstance = GetModuleHandle(NULL);

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = wnd_proc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = window_class_name;
    wcex.hIconSm        = NULL;
    if (!RegisterClassEx(&wcex)) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }

    /* Set double-click time to match GTK's */
    SetDoubleClickTime(250);

    /* Create a hidden toplevel window to for detached windows */
    hidden_window = CreateWindow(window_class_name, window_class_name,
                                 WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT,
                                 1, 1, NULL, NULL, hInstance, NULL);
    if(!hidden_window) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }

    /* Success!  Set global initialized variable and return */

    initialized = 1;
    return 1;
}

static PyObject* embeddingwindow_create(PyObject* self, PyObject* args)
{
    PyObject* rv = NULL;
    WindowInfo* window_info = NULL;
    HWND hwnd = NULL;

    if(!ensure_initialized()) return NULL;

    /* Allocate memory for our WindowInfo object */
    window_info = PyMem_New(WindowInfo, 1);
    if(!window_info) {
        PyErr_NoMemory();
        goto error;
    }

    /* Parse are args, and stuff the event handler object into window_info */
    if (!PyArg_ParseTuple(args, "O:embeddingwindow.create",
                          &window_info->event_handler)) {
        goto error;
    }
    window_info->enable_motion = 0;


    /* Create Window, use hidden_window as it's parent. */
    hwnd = CreateWindow(window_class_name, window_class_name,
                        WS_CHILD, CW_USEDEFAULT, CW_USEDEFAULT,
                        1, 1, hidden_window, NULL, GetModuleHandle(NULL), NULL);
    if(!hwnd) {
        PyErr_SetFromWindowsErr(0);
        goto error;
    }

    /* Set window user data to point to our WindowInfo
     * NOTE: a return value of 0 may or may not mean failure.  We have to call
     * SetError(0), then SetWindowLongPtr, then GetError() to check if the
     * function fails.  Let's just assume it works.
     * */
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window_info);

    /* convert our HWND to an int for use in python */
    rv = PyInt_FromLong((long)hwnd);
    if(!rv) goto error;

    /* Success!  Incref objects we use and return */
    Py_INCREF(window_info->event_handler);
    return rv;

error:
    if(window_info) PyMem_Del(window_info);
    if(hwnd) DestroyWindow(hwnd);

    return NULL;
}

static PyObject* embeddingwindow_set_motion_events(PyObject* self, PyObject* args)
{
    PyObject* enable;
    long hwnd;
    WindowInfo* window_info;

    if(!ensure_initialized()) return NULL;

    /* parse args to find our HWND */
    if (!PyArg_ParseTuple(args, "lO:embeddingwindow.set_motion_events",
                         &hwnd, &enable)) {
        return NULL;
    }

    /* get the window user data to find our WindowInfo */
    window_info = (WindowInfo*) GetWindowLongPtr((HWND)hwnd, GWLP_USERDATA);
    if(!window_info) return PyErr_SetFromWindowsErr(0);

    /* Update enable_motion attribute */
    window_info->enable_motion = PyObject_IsTrue(enable);

    Py_RETURN_NONE;
}

static PyObject* embeddingwindow_attach(PyObject* self, PyObject* args)
{
    long hwnd, parent_hwnd;
    long x, y, width, height;

    if(!ensure_initialized()) return NULL;

    if (!PyArg_ParseTuple(args, "llllll:embeddingwindow.attach",
                         &hwnd, &parent_hwnd, &x, &y, &width, &height)) {
        return NULL;
    }

    SetParent((HWND)hwnd, (HWND)parent_hwnd);
    MoveWindow((HWND)hwnd, x, y, width, height, FALSE);
    ShowWindow((HWND)hwnd, SW_SHOW);

    Py_RETURN_NONE;
}

static PyObject* embeddingwindow_reposition(PyObject* self, PyObject* args)
{
    long hwnd;
    long x, y, width, height;

    if(!ensure_initialized()) return NULL;

    if (!PyArg_ParseTuple(args, "lllll:embeddingwindow.reposition",
                         &hwnd, &x, &y, &width, &height)) {
        return NULL;
    }

    MoveWindow((HWND)hwnd, x, y, width, height, TRUE);

    Py_RETURN_NONE;
}

static PyObject* embeddingwindow_detach(PyObject* self, PyObject* args)
{
    long hwnd;

    if(!ensure_initialized()) return NULL;

    if (!PyArg_ParseTuple(args, "l:embeddingwindow.detach", &hwnd)) {
        return NULL;
    }

    ShowWindow((HWND)hwnd, SW_HIDE);
    SetParent((HWND)hwnd, hidden_window);

    Py_RETURN_NONE;
}

static PyObject* embeddingwindow_destroy(PyObject* self, PyObject* args)
{
    long hwnd;
    WindowInfo* window_info;

    if(!ensure_initialized()) return NULL;

    if (!PyArg_ParseTuple(args, "l:embeddingwindow.destroy", &hwnd)) {
        return NULL;
    }

    /* Fetch our WindowInfo before we destroy the window */
    window_info = (WindowInfo*)GetWindowLongPtr((HWND)hwnd, GWLP_USERDATA);
    if(!window_info)
        return PyErr_SetFromWindowsErr(0);

    /* Destroy the window */
    if(!DestroyWindow((HWND)hwnd))
        return PyErr_SetFromWindowsErr(0);

    /* Free our WindowInfo */
    Py_DECREF(window_info->event_handler);
    PyMem_Del(window_info);


    Py_RETURN_NONE;
}

static PyObject* embeddingwindow_paint_black(PyObject* self, PyObject* args)
{
    long hwnd;
    PAINTSTRUCT ps;
    HDC hdc;

    if(!ensure_initialized()) return NULL;

    if (!PyArg_ParseTuple(args, "l:embeddingwindow.paint_black", &hwnd)) {
        return NULL;
    }

    hdc = BeginPaint((HWND)hwnd, &ps);
    FillRect(hdc, &ps.rcPaint, (HBRUSH) GetStockObject(BLACK_BRUSH));
    EndPaint((HWND)hwnd, &ps);

    Py_RETURN_NONE;
}


static PyMethodDef EmbeddingWindowMethods[] = 
{
    { "create", embeddingwindow_create, METH_VARARGS,
            "create a new window" },
    { "set_motion_events", embeddingwindow_set_motion_events, METH_VARARGS,
            "enable/disable motion events on a windows"},
    { "attach", embeddingwindow_attach, METH_VARARGS,
            "attach window to a parent window and show it" },
    { "reposition", embeddingwindow_reposition, METH_VARARGS,
            "Move window inside its parent window" },
    { "detach", embeddingwindow_detach, METH_VARARGS,
            "Detach window to a parent window and hide it" },
    { "destroy", embeddingwindow_destroy, METH_VARARGS,
            "destroy a window" },
    { "paint_black", embeddingwindow_paint_black, METH_VARARGS,
            "Fill a window with black" },
    { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC initembeddingwindow(void)
{
    Py_InitModule("embeddingwindow", EmbeddingWindowMethods);
}
