#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512

enum STATE
{
	MENU_STATE=0,
	LOGIN_STATE=1,
	REGISTER_STATE=2,
	EXIT_STATE=3,
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

typedef struct _userinfo
{
	char id[20];
	char password[20];
}UserInfo;

// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

PROTOCOL GetProtocol(const char* _ptr)
{
	PROTOCOL protocol;
	memcpy(&protocol, _ptr, sizeof(PROTOCOL));

	return protocol;
}

int Pack_UserInfo(char* _buf, PROTOCOL _protocol, UserInfo user)
{
	int size = 0;
	char* ptr = _buf;
	int strsize = strlen(user.id);

	ptr = ptr + sizeof(size);

	memcpy(ptr, &_protocol, sizeof(_protocol));
	ptr = ptr + sizeof(_protocol);
	size = size + sizeof(_protocol);

	memcpy(ptr, &strsize, sizeof(strsize));
	ptr = ptr + sizeof(strsize);
	size = size + sizeof(strsize);

	memcpy(ptr, user.id, strsize);
	ptr = ptr + strsize;
	size = size + strsize;

	strsize = strlen(user.password);

	memcpy(ptr, &strsize, sizeof(strsize));
	ptr = ptr + sizeof(strsize);
	size = size + sizeof(strsize);

	memcpy(ptr, user.password, strsize);
	ptr = ptr + strsize;
	size = size + strsize;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(size));

	size = size + sizeof(size);

	return size;
}

void UnPack_LoginResult(char* _buf, RESULT& _result, char* _str1)
{
	int strsize1;

	char* ptr = _buf + sizeof(PROTOCOL);

	memcpy(&_result, ptr, sizeof(_result));
	ptr = ptr + sizeof(_result);

	memcpy(&strsize1, ptr, sizeof(strsize1));
	ptr = ptr + sizeof(strsize1);

	memcpy(_str1, ptr, strsize1);
	ptr = ptr + strsize1;
}

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	// 데이터 통신에 사용할 변수
	char buf[BUFSIZE + 1];
	int len;
	int size = 0;
	PROTOCOL protocol;
	STATE state = STATE::MENU_STATE;
	UserInfo user;
	bool exitFlag = false;

	// 서버와 데이터 통신
	while(1){
		switch (state)
		{
		case STATE::MENU_STATE:
			system("cls");

			printf("메뉴\n");
			printf("1. 로그인\n");
			printf("2. 회원가입\n");
			printf("3. 종료\n");

			int select;
			printf("선택>> ");
			scanf("%d", &select);

			switch (select)
			{
			case STATE::LOGIN_STATE:
				state = STATE::LOGIN_STATE;
				break;
			case STATE::REGISTER_STATE:
				state = STATE::REGISTER_STATE;
				break;
			case STATE::EXIT_STATE:
				state = STATE::EXIT_STATE;
				break;
			}

			break;
		case STATE::LOGIN_STATE:
			system("cls");

			printf("아이디 : ");
			scanf("%s", user.id);
			printf("패스워드 : ");
			scanf("%s", user.password);

			protocol = LOGIN;
			size = Pack_UserInfo(buf, protocol, user);

			// 데이터 보내기
			retval = send(sock, buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				return 0;
			}

			// 데이터 받기
			if (!PacketRecv(sock, buf))
			{
				break;
			}

			protocol = GetProtocol(buf);

			switch (protocol)
			{
			case LOGIN:
			case LOGIN_RESULT:

				RESULT result;
				char msg[BUFSIZE + 1];
				memset(msg, 0, BUFSIZE+1);
				UnPack_LoginResult(buf, result, msg);

				// 받은 데이터 출력
				printf("%s\n", msg);

				system("pause");

				state = STATE::MENU_STATE;
				break;
			}
		case STATE::REGISTER_STATE:
			// 미구현
			break;
		case STATE::EXIT_STATE:
			exitFlag = true;
			break;
		}

		if (exitFlag)
			break;
	}

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}