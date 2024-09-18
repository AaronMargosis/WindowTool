# WindowTool

A window management tool.

```
Usage:

    WindowTool.exe all [-v]
    WindowTool.exe find <text> [-v]
    WindowTool.exe process <exeName> [-v]
    WindowTool.exe hwnd <hwnd> [-v]
    WindowTool.exe children <hwnd> [-v]
    WindowTool.exe settitle <hwnd> <title>
    WindowTool.exe show <hwnd>
    WindowTool.exe showall <hwnd>
    WindowTool.exe hide <hwnd>
    WindowTool.exe hideall <hwnd>
    WindowTool.exe top <hwnd>
    WindowTool.exe notop <hwnd>
    WindowTool.exe click <hwnd>
    WindowTool.exe close <hwnd>

all      : list information about all top-level windows in the current desktop
find     : list information about windows that contain the specified <text> in their titles (case-insensitive)
process  : list information about windows owned by a process with an image name containing <exeName> (case-insensitive)
hwnd     : list information about the specified hwnd
children : list information about child windows of the specified hwnd
settitle : change the window title of hwnd to the new <title>
show     : make the specified hwnd visible
showall  : make the specified hwnd and its child windows visible
hide     : make the specified hwnd hidden
hideall  : make the specified hwnd and its child windows hidden
top      : make the specified hwnd always-on-top
notop    : remove the always-on-top attribute from the specified hwnd
click    : simulate a mouse click on the specified hwnd
close    : close the specified hwnd

The "-v" option reports only visible windows.
```

TODO: show sample usage
