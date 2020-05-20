#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512
#define DISCONNECT -1

#define LOGIN_SUCCESS_MSG "�α��� ����"
#define LOGIN_FAILED_MSG "�α��� ����"

enum STATE
{
	MENUSELECT_STATE = 0,
	LOGIN_STATE,
	DISCONNECT_STATE,
};

enum PROTOCOL
{
	LOGIN = 0,
	LOGIN_RESULT,
};

enum RESULT
{
	LOGIN_SUCCESS = 0,
	LOGIN_FAILED
};

typedef struct userinfo
{
	char id[10];
	char pw[10];
}UserInfo;

UserInfo UserList[3] =
{
	{"aa","11"},
	{"bb","22"},
	{"cc","33"},
};

// ���� ���� ������ ���� ����ü�� ����
struct SOCKETINFO
{
	SOCKET sock;
	UserInfo userInfo;
	STATE state;
	bool get4bytes;
	
	int recvbytes;
	int comp_recvbytes;

	int sendbytes;
	int comp_sendbytes;

	char recvbuf[BUFSIZE + 1];
	char sendbuf[BUFSIZE + 1];
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);

// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

PROTOCOL GetProtocol(const char* _ptr);

int Pack_loginResult(char* _buf, PROTOCOL _protocol, RESULT _result, const char* _str1);

void UnPack_userInfo(const char* _buf, char* id, char* pw);

int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// �ͺ��ŷ �������� ��ȯ
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if(retval == SOCKET_ERROR) err_display("ioctlsocket()");

	// ������ ��ſ� ����� ����
	FD_SET rset, wset;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen, i;
	PROTOCOL protocol;

	while(1){
		// ���� �� �ʱ�ȭ
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		FD_SET(listen_sock, &rset);
		for(i=0; i<nTotalSockets; i++)
		{
			SOCKETINFO* ptr = SocketInfoArray[i];

			FD_SET(ptr->sock, &rset);

			switch (ptr->state)
			{
			case STATE::LOGIN_STATE:
				FD_SET(ptr->sock, &wset);
				break;
			}
		}

		// select()
		retval = select(0, &rset, &wset, NULL, NULL);
		if(retval == SOCKET_ERROR) err_quit("select()");

		// ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
		if(FD_ISSET(listen_sock, &rset))
		{
			addrlen = sizeof(clientaddr);
			client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
			if(client_sock == INVALID_SOCKET)
			{
				err_display("accept()");
			}
			else
			{
				printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
					inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
				// ���� ���� �߰�
				AddSocketInfo(client_sock);
			}
		}

		// ���� �� �˻�(2): ������ ���
		for(i=0; i<nTotalSockets; i++)
		{
			SOCKETINFO *ptr = SocketInfoArray[i];
			if(FD_ISSET(ptr->sock, &rset))
			{
				//4����Ʈ ���� �ޱ�
				if (!ptr->get4bytes)
				{
					ptr->recvbytes = sizeof(int);

					retval = recv(ptr->sock, ptr->recvbuf + ptr->comp_recvbytes, ptr->recvbytes - ptr->comp_recvbytes, 0);

					if (retval == SOCKET_ERROR)
					{
						err_display("recv()");
						ptr->state = STATE::DISCONNECT_STATE;
					}
					else if (retval == 0)
					{
						ptr->state = STATE::DISCONNECT_STATE;
					}
					else
					{
						ptr->comp_recvbytes += retval;

						if (ptr->recvbytes == ptr->comp_recvbytes)
						{
							ptr->recvbytes = 0;
							ptr->comp_recvbytes = 0;
							memcpy(&ptr->recvbytes, ptr->recvbuf, sizeof(int));
							ptr->get4bytes = true;
						}
					}
				}
				else // 4����Ʈ �ް� ���� ������ ������ �ޱ�
				{
					
					retval = recv(ptr->sock, ptr->recvbuf + ptr->comp_recvbytes, ptr->recvbytes - ptr->comp_recvbytes, 0);

					if (retval == SOCKET_ERROR)
					{
						err_display("recv()");
						ptr->state = STATE::DISCONNECT_STATE;
					}
					else if (retval == 0)
					{
						ptr->state = STATE::DISCONNECT_STATE;
					}
					else
					{
						ptr->comp_recvbytes += retval;

						if (ptr->recvbytes == ptr->comp_recvbytes)
						{
							ptr->recvbytes = 0;
							ptr->comp_recvbytes = 0;
							ptr->get4bytes = false;
						}
					}

					if (!ptr->get4bytes)
					{
						switch (ptr->state)
						{
						case STATE::MENUSELECT_STATE:
							protocol = GetProtocol(ptr->recvbuf);

							switch (protocol)
							{
							case LOGIN:
								UnPack_userInfo(ptr->recvbuf, ptr->userInfo.id, ptr->userInfo.pw);
								ptr->state = STATE::LOGIN_STATE;
								break;
							}
							break;
						}
					}
				}
			}

			if(FD_ISSET(ptr->sock, &wset))
			{
				char msg[BUFSIZE+1];
				RESULT result;
				memset(msg, 0, BUFSIZE + 1);

				switch (ptr->state)
				{
				case STATE::LOGIN_STATE:
					
					//ó�� ������
					if (ptr->sendbytes == 0)
					{
						//���̵�, ��� ��ġ�˻�
						for (int i = 0; i < 3; i++)
						{
							if (strcmp(ptr->userInfo.id, UserList[i].id) == 0
								&& strcmp(ptr->userInfo.pw, UserList[i].pw) == 0)
							{
								strcpy(msg, LOGIN_SUCCESS_MSG);
								result = RESULT::LOGIN_SUCCESS;
								break;
							}

							strcpy(msg, LOGIN_FAILED_MSG);
							result = RESULT::LOGIN_FAILED;
						}

						// �α��� ��� ��ŷ
						ptr->sendbytes = Pack_loginResult(ptr->sendbuf, PROTOCOL::LOGIN_RESULT, result, msg);
					}


					retval = send(ptr->sock, ptr->sendbuf + ptr->comp_sendbytes, ptr->sendbytes - ptr->comp_sendbytes, 0);

					ptr->comp_sendbytes += retval;

					if (retval == SOCKET_ERROR)
					{
						err_display("send()");
						ptr->state = STATE::DISCONNECT_STATE;
					}

					// �ٺ����� ��
					if (ptr->sendbytes == ptr->comp_sendbytes)
					{
						ptr->recvbytes = ptr->sendbytes = 0;
						ptr->state = STATE::MENUSELECT_STATE;

						printf("���� %s �α���\n", ptr->userInfo.id);
					}

					break;
				}
			}

			if(ptr->state==STATE::DISCONNECT_STATE)
			{
				RemoveSocketInfo(i);
				i--;
				continue;
			}
		}
	}

	// ���� ����
	WSACleanup();
	return 0;
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock)
{
	if(nTotalSockets >= FD_SETSIZE){
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL){
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	ptr->sock = sock;
	memset(&ptr->userInfo, 0, sizeof(UserInfo));
	ptr->state = STATE::MENUSELECT_STATE;
	ptr->get4bytes = false;
	ptr->recvbytes = 0;
	ptr->comp_recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->comp_sendbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];

	// Ŭ���̾�Ʈ ���� ���
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	closesocket(ptr->sock);
	delete ptr;

	if(nIndex != (nTotalSockets-1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];

	--nTotalSockets;
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// �������� ���
PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

int Pack_loginResult(char* _buf, PROTOCOL _protocol, RESULT _result, const char* msg)
{
	char* ptr = _buf;
	int strsize1 = strlen(msg);
	int size = 0;

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &_result, sizeof(_result));
	ptr = ptr + sizeof(_result);
	size = size + sizeof(_result);

	memcpy(ptr, &strsize1, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);
	size = size + sizeof(strsize1);

	memcpy(ptr, msg, strsize1);
	ptr = ptr + strsize1;
	size = size + strsize1;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

void UnPack_userInfo(const char* _buf, char* id, char* pw)
{
	const char* ptr = _buf + sizeof(PROTOCOL);
	int strsize;

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(id, ptr, strsize);
	ptr = ptr + strsize;

	memcpy(&strsize, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(pw, ptr, strsize);
	ptr = ptr + strsize;
}