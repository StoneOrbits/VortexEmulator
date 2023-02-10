#include <WinSock2.h>
#include <Ws2tcpip.h>

#include "TestFramework.h"
#include "IRSimulator.h"
#include "VortexLib.h"

#include <stdio.h>

#define DEFAULT_PORT "33456"

#pragma comment(lib, "ws2_32.lib")

SOCKET IRSimulator::m_sock = -1;
SOCKET IRSimulator::m_clientSock = -1;
bool IRSimulator::m_isServer = false;
bool IRSimulator::m_isConnected = false;
HANDLE IRSimulator::m_hServerMutex = nullptr;

bool IRSimulator::init()
{
  WSAData wsaData;
  // Initialize Winsock
  int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (res != 0) {
    printf("WSAStartup failed with error: %d\n", res);
    return false;
  }
  // use a mutex to detect if server is running yet
  m_hServerMutex = CreateMutex(NULL, TRUE, "VortexServerMutex");
  if (!m_hServerMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
    // otherwise try to connect client to server
    startClient();
  }
  return true;
}

bool IRSimulator::receive_message(uint32_t &out_message)
{
  SOCKET target_sock = m_sock;
  if (m_isServer) {
    target_sock = m_clientSock;
  }
  if (recv(target_sock, (char *)&out_message, sizeof(out_message), 0) <= 0) {
    printf("Recv failed with error: %d\n", WSAGetLastError());
    return false;
  }
  return true;
}

bool IRSimulator::send_message(uint32_t message)
{
  SOCKET target_sock = m_sock;
  if (m_isServer) {
    target_sock = m_clientSock;
  }
  //static uint32_t counter = 0;
  //printf("Sending[%u]: %u\n", counter++, message);
  if (send(target_sock, (char *)&message, sizeof(message), 0) == SOCKET_ERROR) {
    // most likely server closed
    printf("send failed with error: %d\n", WSAGetLastError());
    return false;
  }
  return true;
}

// wait for a connection from client
bool IRSimulator::accept_connection()
{
  // Wait for a client connection and accept it
  m_clientSock = accept(m_sock, NULL, NULL);
  if (m_clientSock == INVALID_SOCKET) {
    printf("accept failed with error: %d\n", WSAGetLastError());
    return 1;
  }
  printf("Received connection!\n");
  return true;
}

DWORD __stdcall IRSimulator::wait_connection(void *arg)
{
  // block till a client connects and clear the output files
  if (!accept_connection()) {
    return 0;
  }
  printf("Accepted connection\n");

  m_isConnected = true;

  // idk when another one launches the first ones strip unpaints
  PostMessage(NULL, WM_PAINT, NULL, NULL);

  // each message received will get passed into the logging system
  while (1) {
    uint32_t message = 0;
    if (!receive_message(message)) {
      break;
    }
    bool is_mark = (message & (1 << 31)) != 0;
    message &= ~(1 << 31);
    Vortex::IRDeliver(message);
    //printf("Received %s: %u\n", is_mark ? "mark" : "space", message);
  }

  printf("Connection closed\n");
  return 0;
}

// initialize the server
bool IRSimulator::startServer()
{
  struct addrinfo hints;
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // Resolve the server address and port
  struct addrinfo *result = NULL;
  int res = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
  if (res != 0) {
    printf("getaddrinfo failed with error: %d\n", res);
    WSACleanup();
    return false;
  }
  // Create a SOCKET for connecting to server
  m_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (m_sock == INVALID_SOCKET) {
    printf("socket failed with error: %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    return false;
  }
  // Setup the TCP listening socket
  res = bind(m_sock, result->ai_addr, (int)result->ai_addrlen);
  freeaddrinfo(result);
  if (res == SOCKET_ERROR) {
    printf("bind failed with error: %d\n", WSAGetLastError());
    closesocket(m_sock);
    return false;
  }
  // start listening
  if (listen(m_sock, SOMAXCONN) == SOCKET_ERROR) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    return false;
  }
  CreateThread(NULL, 0, wait_connection, NULL, 0, NULL);
  printf("Success listening on *:8080\n");
  m_isServer = true;
  g_pTestFramework->setWindowTitle(g_pTestFramework->getWindowTitle() + " Receiver");
  g_pTestFramework->setWindowPos(900, 350);

  // launch another instance of the test framwork to act as the sender
  if (m_isServer) {
    char filename[2048] = { 0 };
    GetModuleFileName(GetModuleHandle(NULL), filename, sizeof(filename));
    PROCESS_INFORMATION procInfo;
    memset(&procInfo, 0, sizeof(procInfo));
    STARTUPINFO startInfo;
    memset(&startInfo, 0, sizeof(startInfo));
    CreateProcess(filename, NULL, NULL, NULL, false, 0, NULL, NULL, &startInfo, &procInfo);
  }
  return true;
}

// initialize the network client for server mode, thanks msdn for the code
bool IRSimulator::startClient()
{
  struct addrinfo *addrs = NULL;
  struct addrinfo *ptr = NULL;
  struct addrinfo hints;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // allows ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  // Resolve the server address and port
  int res = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &addrs);
  if (res != 0) {
    printf("Could not resolve addr info\n");
    return false;
  }
  //info("Attempting to connect to %s", config.server_ip.c_str());
  // try connecting to all the addrs
  for (ptr = addrs; ptr != NULL; ptr = ptr->ai_next) {
    m_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
      printf("Error creating socket: %d\n", GetLastError());
      freeaddrinfo(addrs);
      return false;
    }
    if (connect(m_sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
      // try again
      closesocket(m_sock);
      m_sock = INVALID_SOCKET;
      continue;
    }
    // Success!
    break;
  }
  freeaddrinfo(addrs);
  if (m_sock == INVALID_SOCKET) {
    return false;
  }
  // turn on non-blocking for the socket so the module cannot
  // get stuck in the send() call if the server is closed
  u_long iMode = 1; // 1 = non-blocking mode
  res = ioctlsocket(m_sock, FIONBIO, &iMode);
  if (res != NO_ERROR) {
    printf("Failed to ioctrl on socket\n");
    closesocket(m_sock);
    return false;
  }
  printf("Success initializing network client\n");
  g_pTestFramework->setWindowTitle(g_pTestFramework->getWindowTitle() + " Sender");
  g_pTestFramework->setWindowPos(450, 350);
  m_isConnected = true;
  //info("Connected to server %s", config.server_ip.c_str());
  return true;
}



