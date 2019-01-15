#pragma comment(linker, "/OPT:NOWIN98")
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, const char **argv)
{
	int i;
	char buf[4096];
	HANDLE pipe;
	DWORD bytes;
	int next_update;
	unsigned int start_time;
	FILETIME ft;
	LARGE_INTEGER li;
	HWND hwnd;
	
	for (;;) {
		Sleep(1000);

		strcpy(buf, "\\\\.\\pipe\\discord-ipc-0");
		for (i = 0; i < 10; ++i) {
			pipe = CreateFile(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (pipe != INVALID_HANDLE_VALUE) {
				break;
			}
			++buf[21]; // increase digit
		}

		if (pipe == INVALID_HANDLE_VALUE) {
			continue;
		}

		next_update = 2147483647;
		start_time = 0;

		// handshake
		strcpy(&buf[8], "{\"v\":1,\"client_id\":\"534758708496957520\"}");
		*(int *)buf = 0;
		*(int *)&buf[4] = strlen(&buf[8]);
		WriteFile(pipe, buf, *(int *)&buf[4] + 8, NULL, NULL);

		for (;;) {
			Sleep(100);
			
			if (!PeekNamedPipe(pipe, buf, 8, NULL, &bytes, NULL)) {
				break;
			}
			
			if (bytes >= 8) {
				if (*(int *)&buf[4] >= sizeof(buf) - 8) { // too large frame
					break;
				}
				buf[*(int *)&buf[4] + 8] = 0;
				if (!ReadFile(pipe, buf, *(int *)&buf[4] + 8, NULL, NULL)) {
					break;
				}
				if (*(int *)buf != 1) { // not frame
					if (*(int *)buf == 3) { // ping
						*(int *)buf = 4; // pong
						WriteFile(pipe, buf, *(int *)&buf[4] + 8, NULL, NULL);
						continue;
					}
					break;
				}
				if (strstr(&buf[8], "evt\":\"READY\"")) {
					next_update = 0;
				}
			}

			if (--next_update <= 0) {
				next_update = 30;

				// find VS6
				i = 0;
				for (hwnd = GetWindow(GetDesktopWindow(), GW_CHILD); hwnd; hwnd = GetWindow(hwnd, GW_HWNDNEXT)) {
					if (IsWindowVisible(hwnd) &&
						GetWindowText(hwnd, buf, sizeof(buf)) &&
						strstr(buf, "Microsoft Visual C++")) {
						if ((hwnd = FindWindowEx(hwnd, NULL, "MDIClient", NULL)) &&
							(hwnd = GetWindow(hwnd, GW_CHILD)) &&
							GetWindowText(hwnd, buf, sizeof(buf))) {
							if (strstr(buf, ".cpp")) {
								i = (int)"cpp";
							} else if (strstr(buf, ".c")) {
								i = (int)"c";
							} else if (strstr(buf, ".hpp")) {
								i = (int)"hpp";
							} else if (strstr(buf, ".h")) {
								i = (int)"h";
							} else {
								i = (int)"text";
							}
						} else {
							i = (int)"project";
						}
						break;
					}
				}

				// update presence
				if (i) {
					if (!start_time) {
						GetSystemTimeAsFileTime(&ft);
						li.LowPart = ft.dwLowDateTime;
						li.HighPart = ft.dwHighDateTime;
						start_time = (unsigned int)((li.QuadPart - 0x19DB1DED53E8000) / 0x989680);
					}
					wsprintf(&buf[8], "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":1,\"args\":{\"pid\":%u,\"activity\":{\"timestamps\":{\"start\":%u},\"assets\":{\"large_image\":\"%s\",\"small_image\":\"visualstudio\"}}}}", GetCurrentProcessId(), start_time, (char *)i);
				} else {
					start_time = 0;
					wsprintf(&buf[8], "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":1,\"args\":{\"pid\":%u}}", GetCurrentProcessId());
				}
				*(int *)buf = 1;
				*(int *)&buf[4] = strlen(&buf[8]);
				WriteFile(pipe, buf, *(int *)&buf[4] + 8, NULL, NULL);
			}
		}

		CloseHandle(pipe);
	}

	return 0;
}