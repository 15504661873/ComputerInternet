#include <tchar.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define IN 
#define OUT
#pragma comment(lib,"Ws2_32.lib")
static int fob_flag = 0;
static int fobidden_flag = 1;
static int fishing_flag = 1;
static long age = 604800;//һ��
						 /*�궨��---------------------------------------------*/
#define MAXSIZE 1024*1024*10//�������ݱ��ĵ���󳤶�,500KB
#define HTTP_PORT 80 //http �������˿�



						 /*�ṹ�嶨��-----------------------------------------*/
						 //Http ��Ҫͷ������

struct HttpHeader {
	char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
	char url[1024];  // ����� url
	char host[1024]; // Ŀ������
	char cookie[1024 * 10]; //cookie
	char date[1024];
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};
struct fishing {
	char init_host[50];
	char fishing_host[50];
	char fishing_head[500];
};


/*��������----------------------------------------------*/
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
int checkinf(char *host);
void read_void(char path[]);
void read_fishing(char path[]);
int checkfishing(char* host);
char* makechar(char* name);
int checknum(char* buffer);
int checkoutofdate(FILE* fp);
void addifsincemodified(char* buffer, HttpHeader* httpHeader);
time_t timeconvert(IN char *buf, OUT struct tm *p);
int monthcmp(IN char *p);
int weekcmp(IN char *p);

/*ȫ�ֱ�������-------------------------------------------*/
//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;


//�����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};

//CRITICAL_SECTION g_cs;
int _tmain(int argc, _TCHAR* argv[])
{
	
	printf("�����������������\n");
	printf("��ʼ��...\n");
	if (!InitSocket()) {
		printf("socket ��ʼ��ʧ��\n");
		return -1;
	}
	printf("����������������У������˿� %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//������������ϼ���
	while (true) {
		acceptSocket = accept(ProxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		//Sleep(20);
	}
	//DeleteCriticalSection(&g_cs);
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket() {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
		printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ�� winsock �汾\n");
		WSACleanup();
		return FALSE;
	}
	//����TCP�׽���
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("�����׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR *)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("���׽���ʧ��\n");
		return FALSE;
	}
	if (listen(ProxyServer, 3000) == SOCKET_ERROR) {
		printf("�����˿�%d ʧ��", ProxyPort);
		return FALSE;
	}
	return TRUE;
}

//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	//EnterCriticalSection(&g_cs);
	char *Buffer;
	char *CacheBuffer;
	char *CacheBuffer2;
	Buffer = new char[MAXSIZE];
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	char ch;
	int index;
	char *cache_dir;
	cache_dir = new char[100];
	memset(cache_dir, 0, 100);
	//�ӿͻ��˽���http����
	recvSize = recv(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	
	if (recvSize <= 0) {
		printf("���ܿͻ��������������\n");
		goto error;
	}
	/*
	1.���жϻ����Ƿ�����
	1.1��û��������ֱ��������󣬻�ȡ����󣬲鿴cache-control�ֶ�
	1.1.1�����Ի��棬�򱣴浽���أ������ı��ģ��������ͻؿͻ���
	1.1.2�������Ի��棬��ֱ�ӷ��ͻؿͻ���
	1.2���������У��Ƿ���ڣ�DATE+max-age�뵱ǰʱ��ĶԱȣ�
	1.2.1��û�й��ڣ�ֱ�Ӵӻ����ж�ȡ�������ؿͻ���
	1.2.2�����ڣ������������IF-MODIFIED-SINCE��LAST MODIFIED�е�ʱ��
	1.2.2.1������200������»��棬�����ؿͻ���
	1.2.2.2������304�����ؿͻ���
	*/



	HttpHeader* httpHeader = new HttpHeader();
	HttpHeader* httpHeader_fishing = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);
	printf("httpHeader->host(Ŀ������):%s\n ", httpHeader->host);
	printf("httpHeader->url(Ŀ��url):%s\n\n ", httpHeader->url);
	//printf("�������Buffer��\n%s\n\n", Buffer);
	if (strcmp(httpHeader->host, "masterconn.qq.com") == 0 || strcmp(httpHeader->host, "wup.browser.qq.com") == 0 || strcmp(httpHeader->host, "wup.browser.qq.com:443") == 0 || strcmp(httpHeader->host, "qbwup.imtt.qq.com") == 0 || strcmp(httpHeader->host, "plugin.browser.qq.com") == 0)
		goto liulanqi;
	
	char host[255];
	if (gethostname(host, sizeof(host)) == SOCKET_ERROR)
	{
		printf("�޷���ȡ�������");
	}
	else
	{
		printf("�������Ϊ��%s\n", host);
	}

	struct hostent *p = gethostbyname(host);
	if (p == 0)
	{
		printf("�޷���ȡ�������������IP");
	}
	else
	{

		//����IP:����ѭ��,�����������IP  
		for (int i = 0; p->h_addr_list[i] != 0; i++)
		{
			struct in_addr in;
			memcpy(&in, p->h_addr_list[i], sizeof(struct in_addr));
			printf("IPΪ��%s\n", inet_ntoa(in));  
			if (fob_flag==1 && strcmp(inet_ntoa(in), "172.20.7.72") == 0)
			{
				printf("��ֹ���û�������������������\n");
				//return FALSE;
				goto error;
			}
		}

	}
	
	
	if(fobidden_flag == 1 && strcmp(httpHeader->host,"jwc.hit.edu.cn") == 0)
	{
		printf("��ֹ���ʵ���վ������������������\n");
		//return FALSE;
		goto error;
	}

	if (fishing_flag == 1 && strcmp(httpHeader->host,"219.217.226.44") == 0)
	{
		
		 memcpy(httpHeader->host,"today.hit.edu.cn",22);
	}
	
		cache_dir = makechar(httpHeader->url);
	FILE *fp = fopen(cache_dir, "rb");//cache�иû����Ƿ����
	int bf = 0;
	if (fp != NULL && checkoutofdate(fp) == 0)//����������û�й���
	{
		ZeroMemory(Buffer, MAXSIZE);
		/*while (!feof(fp))//��ȡ���ػ���ı���
		{
		ch = fgetc(fp);
		if (ch == '\n')
		{
		Buffer[bf++] = '\r';
		Buffer[bf++] = '\n';
		}//
		else
		Buffer[bf++] = ch;
		}*/
		fread(Buffer, sizeof(char), MAXSIZE, fp);
		printf("�������� %s\n", cache_dir);
		printf("%d\n", strlen(Buffer));
		ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);//������ı��Ļش�
		fclose(fp);
		goto success;//�������߳�
	}
	else if (checkoutofdate(fp) == 1)//����
	{
		addifsincemodified(Buffer, httpHeader);
	}
	//���û�����л���

	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host)) {
		printf("���ӷ�������������\n");
		goto error;
	}
	printf("������������%s�ɹ�\n", httpHeader->host);
	//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
//	printf("\n%s\n", Buffer);
	ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer), 0);



	//�ȴ�Ŀ���������������
	ZeroMemory(Buffer, MAXSIZE);
	recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);

	if (recvSize <= 0) {
		printf("���ܷ��������ش��󣡣���\n");
		goto error;
	}


	//��Ŀ����������ص�����ֱ��ת�����ͻ���
	ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer, strlen(Buffer), 0);


	//�����ݴ��뱾����Ϊ����
	if (checknum(Buffer) == 200)
	{
		FILE *fm = fopen(cache_dir, "wb");//�����ļ�����URL��Ϊ�ļ���
		if (fm != NULL)
		{
			//fprintf(fm, "%s", Buffer);
			//printf("%s", Buffer);
			/*int i = 0;
			while (Buffer[i] != 0)
			{
			fputc(Buffer[i], fm);
			i++;
			}*/
			fwrite(Buffer, sizeof(char), strlen(Buffer), fm);
			fclose(fm);
		}
	}
	/*	if (strcmp(cache_dir, "1994") == 0)
	{
	FILE *fm = fopen("hehehe", "w");
	fprintf(fm, "%s", Buffer);
	}*/
	else {
		printf("����ʧ�ܣ���������\n");
	}
	goto success;

error:
	printf("����ʧ�ܹر��׽���\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
success:
	printf("����ɹ��ر��׽���\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
liulanqi:
	printf("������׽��֣�ֱ�ӹر�\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer, HttpHeader * httpHeader) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
	printf("%s\n", p);
	if (p[0] == 'G') {//GET ��ʽ
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST ��ʽ
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	//printf("������%s\n", host);
	//if (fobidden_flag == 1 && checkinf(host) == 1)
	if (fobidden_flag == 1 && strcmp(host,"jwc.hit.edu.cn")==0)
	{
		return FALSE;
	}
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		printf("��������!!!");
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		printf("�����׽��ֳ���!!!");
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		printf("����Ŀ�����������!!!");
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}

char* makechar(char* name)
{
	char* r_name = new char[100];
	memset(r_name, 0, 100);
	int i = 0;
	int i_name = 0;
	while (name[i] != NULL)
	{
		i_name = i_name + name[i];
		i++;
	}
	_itoa(i_name, r_name, 10);
	return r_name;
}

int checknum(char* buffer)
{
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	char num[10] = { 0 }; return 200;
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
	memcpy(num, &p[9], 3);
	if (strcmp(num, "304") == 0)
		return 304;
	return 200;
}
int checkoutofdate(FILE* fp) {
	char date[40] = { 0 };
	long second = 0;
	long nowt = 0;
	struct tm *nowtime = (struct tm*)malloc(sizeof(struct tm));
	struct tm *time1 = (struct tm*)malloc(sizeof(struct tm));
	if (fp == NULL || 1)
	{
		return 0;
	}
	else
	{
		char strline[200] = { 0 };
		while (fp != NULL)
		{
			fgets(strline, 1024, fp);
			if (strline[0] == 'D'&&strline[1] == 'a'&&strline[2] == 't'&&strline[3] == 'e')
			{
				memcpy(date, &strline[6], 25);
				timeconvert(date, time1);
				second = mktime(time1);
				break;
			}
		}
		if (nowt - second < age)
			return 0;
		else
			return 1;
	}
}
void addifsincemodified(char* buffer, HttpHeader* httpHeader) {
	char add[1024] = { 0 };
	memcpy(add, "if modified since", 20);
	strcat(add, httpHeader->date);
	strcat(buffer, add);
}

//�Ƚ��������ɹ�����0-6���������󷵻�7
//p����������ȡ����ǰ3����ĸ����Mon������1���Դ�����
//�Ķ��ܼ���Ӱ�췵�ص�ʱ��ֵ������ͨ���Ķ����ڵ��������ﵽ�޸�ʱ��
int weekcmp(IN char *p)
{
	char week[7][5] = { "Sun\0", "Mon\0", "Tue\0", "Wed\0", "Thu\0", "Fri\0", "Sat\0" };
	int i;


	for (i = 0; i<7; i++)
		if (strcmp(p, week[i]) == 0)
			break;

	if (i == 7)
	{
		printf("fail to find week.\n");
		return i;
	}
	return i;
}
//�Ƚ��·ݣ��ɹ�����0-11���������󷵻�12
//P Ϊ�·ݵ�ǰ������ĸ����Feb������£��Դ�����
int monthcmp(IN char *p)
{
	char month[13][5] = { "Jan\0", "Feb\0", "Mar\0", "Apr\0", "May\0", "Jun\0", "Jul\0", "Aug\0", "Sep\0", "Oct\0", "Nov\0", "Dec\0" };

	int i;
	for (i = 0; i<12; i++)
		if (strcmp(p, month[i]) == 0)
			break;
	if (i == 12)
	{
		printf("fail to find month.\n");
		return i;
	}
	return i;
}
//���ִ���ʽ��ʱ��ת��Ϊ�ṹ��,���ؾ���1970��1��1��0��0��0�����������ַ�����ʽ�����ֵʱ����0
//BUF Ϊ����Tue May 15 14:46:02 2007��ʽ�ģ�pΪʱ��ṹ��
time_t timeconvert(IN char *buf, OUT struct tm *p)
{

	char cweek[4];
	char cmonth[4];
	time_t second;

	sscanf(buf, "%s %s %d %d:%d:%d %d", cweek, cmonth, &(p->tm_mday), &(p->tm_hour), &(p->tm_min), &(p->tm_sec), &(p->tm_year));
	p->tm_year -= 1900;
	printf("****%s,%s*****\n", cweek, cmonth);
	p->tm_mon = monthcmp(cmonth);
	//�Ķ��ܼ���Ӱ�췵�ص�ʱ��ֵ������ͨ���Ķ����ڵ��������ﵽ�޸�ʱ��
	p->tm_wday = weekcmp(cweek);
	if (p->tm_mon == 12 && p->tm_wday == 7)
	{
		printf("monthcmp() or weekcmp() fail to use.\n");
		return 0;
	}
	return second = mktime(p);
}

