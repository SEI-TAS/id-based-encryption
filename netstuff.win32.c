#include <winsock.h>

void netstartupjunk()
{
    WSADATA winthing;
    WSAStartup(MAKEWORD(2, 0), &winthing);
}
