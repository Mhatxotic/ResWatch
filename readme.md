# Resolution Watcher

## About…
This is a very simple program to set a resolution of your choice and keep that resolution until the program closes. An ideal scenario is for when you have a network share on a managed desktop which you could login to a computer with an undesirable resolution. You could put this program in your startup folder and it will set the resolution when you log in and reset it back when you log out.

## Requirements…
You need Windows 2000 or later to run this program. Compatibility with earlier Windows versions is not known and unsupported regardless.

## Files…
* `reswatch.cfg` - Configuration file
* `reswatch32.exe` - For 32-bit Windows 2000, XP, Vista and 7.
* `reswatch64.exe` - for 64-bit Windows XP, Vista and 7.

## Usage…
Run the program. You have 10 seconds to act before the requested resolution (as described in the configuration file) is set. You can not wait and set the resolution, cancel the set or view information about the program.

## Configuration settings…
* Resolution pixel width to set (`640`,`800`,`1024`,etc.) -> `desktop_width = number`
* Resolution pixel height to set (`480`,`600`,`768`,etc.) -> `desktop_height = number`
* Resolution pixel depth to set (`2`,`4`,`8`,`15`,`16`,`24`,`32`) -> `desktop_depth = number`
* Resolution refresh rate to set (`60`,`75`,`100`,etc.) -> `desktop_refresh = number`
