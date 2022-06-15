#include "Arduino.h"

#ifdef LINUX_FRAMEWORK
#include "TestFrameworkLinux.h"
#include <sys/socket.h>
#else
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include "TestFramework.h"
#endif

#include <chrono>
#include <random>
#include <time.h>

SerialClass Serial;

#ifdef LINUX_FRAMEWORK
static uint64_t start;
#define SOCKET int
#else
static LARGE_INTEGER start;
static LARGE_INTEGER tps; //tps = ticks per second
#pragma comment (lib, "ws2_32.lib")
#define DEFAULT_PORT "33456"
SOCKET sock = -1;
SOCKET client_sock = -1;
bool is_server = false;
static bool receive_message(uint32_t &out_message);
static bool send_network_message(uint32_t message);
static bool accept_connection();
static DWORD __stdcall listen_connection(void *arg);
static bool init_server();
static bool init_network_client();
#endif

void init_arduino()
{
#ifdef LINUX_FRAMEWORK
  start = micros();
#else
  QueryPerformanceFrequency(&tps);
  QueryPerformanceCounter(&start);

  WSAData wsaData;
  // Initialize Winsock
  int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (res != 0) {
    printf("WSAStartup failed with error: %d\n", res);
    return;
  }
  // try to connect to server first, if no server
  if (!init_network_client()) {
    // then just initialize an listen
    init_server();
  }
#endif
}

void delay(size_t amt)
{
#ifdef LINUX_FRAMEWORK
  //sleep(amt);
#else
  Sleep((DWORD)amt);
#endif
}

void delayMicroseconds(size_t us)
{
}

// used for seeding randomSeed()
unsigned long analogRead(uint32_t pin)
{
  return rand();
}

// used to read button input
unsigned long digitalRead(uint32_t pin)
{
  if (pin == 1) {
    // get button state
    if (g_pTestFramework->isButtonPressed()) {
      return LOW;
    }
    return HIGH;
  }
  return HIGH;
}

void digitalWrite(uint32_t pin,  uint32_t val)
{
}

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns)   ((ns)/1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns)    ((ns)/1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns)    ((ns)/1000)

unsigned long millis()
{
#ifdef LINUX_FRAMEWORK
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
  return (unsigned long)ms;
#else
  return GetTickCount();
#endif
}

unsigned long micros()
{
#ifdef LINUX_FRAMEWORK
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
  return (unsigned long)us;
#else
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  // yes, this will overflow, that's how arduino micros() works *shrug*
  return (unsigned long)((now.QuadPart - start.QuadPart) * 1000000 / tps.QuadPart);
#endif
}

unsigned long random(uint32_t low, uint32_t high)
{
  return low + (rand() % high);
}

void randomSeed(uint32_t seed)
{
  srand(seed);
}

void pinMode(uint32_t pin, uint32_t mode)
{
  // ???
}

void attachInterrupt(int interrupt, void (*func)(), int type)
{
}

void detachInterrupt(int interrupt)
{
}

void test_ir_mark(uint32_t duration)
{
#ifndef LINUX_FRAMEWORK
  send_network_message(duration | (1<<31));
#endif
}

void test_ir_space(uint32_t duration)
{
#ifndef LINUX_FRAMEWORK
  send_network_message(duration);
#endif
}

// this is only used on the windows framework to test IR
void (*IR_change_callback)(uint32_t data);
void installIRCallback(void (*func)(uint32_t))
{
  IR_change_callback = func;
}

int digitalPinToInterrupt(int pin)
{
  return 0;
}


#ifndef LINUX_FRAMEWORK
// windows only IR simulator via network socket

// receive a message from client
static bool receive_message(uint32_t &out_message)
{
  if (recv(client_sock, (char *)&out_message, sizeof(out_message), 0) <= 0) {
    printf("Recv failed with error: %d\n", WSAGetLastError());
    return false;
  }
  return true;
}

// send a message
static bool send_network_message(uint32_t message)
{
  if (send(sock, (char *)&message, sizeof(message), 0) == SOCKET_ERROR) {
    // most likely server closed
    printf("send failed with error: %d\n", WSAGetLastError());
    return false;
  }
  return true;
}

// wait for a connection from client
static bool accept_connection()
{
  // Wait for a client connection and accept it
  client_sock = accept(sock, NULL, NULL);
  if (client_sock == INVALID_SOCKET) {
    printf("accept failed with error: %d\n", WSAGetLastError());
    return 1;
  }
  printf("Received connection!\n");
  return true;
}

static DWORD __stdcall listen_connection(void *arg)
{
  // block till a client connects and clear the output files
  if (!accept_connection()) {
    return 0;
  }
  printf("Accepted connection\n");

  // idk when another one launches the first ones strip unpaints
  g_pTestFramework->redrawStrip();
  PostMessage(NULL, WM_PAINT, NULL, NULL);

  // each message received will get passed into the logging system
  while (1) {
    uint32_t message = 0;
    if (!receive_message(message)) {
      break;
    }
    bool is_mark = (message & (1<<31)) != 0;
    message &= ~(1<<31);
    if (IR_change_callback) {
      IR_change_callback(message);
    }
    //printf("Received %s: %x\n", is_mark ? "mark" : "space", message);
  }

  printf("Connection closed\n");
  return 0;
}

// initialize the server
static bool init_server()
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
  sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sock == INVALID_SOCKET) {
    printf("socket failed with error: %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    return false;
  }
  // Setup the TCP listening socket
  res = bind(sock, result->ai_addr, (int)result->ai_addrlen);
  freeaddrinfo(result);
  if (res == SOCKET_ERROR) {
    printf("bind failed with error: %d\n", WSAGetLastError());
    closesocket(sock);
    return false;
  }
  // start listening
  if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    return false;
  }
  printf("Success listening on *:8080\n");
  is_server = true;
  CreateThread(NULL, 0, listen_connection, NULL, 0, NULL);
  return true;
}

// initialize the network client for server mode, thanks msdn for the code
static bool init_network_client()
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
    sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET) {
      printf("Error creating socket: %d\n", GetLastError());
      freeaddrinfo(addrs);
      return false;
    }
    if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
      // try again
      closesocket(sock);
      sock = INVALID_SOCKET;
      continue;
    }
    // Success!
    break;
  }
  freeaddrinfo(addrs);
  if (sock == INVALID_SOCKET) {
    return false;
  }
  // turn on non-blocking for the socket so the module cannot
  // get stuck in the send() call if the server is closed
  u_long iMode = 1; // 1 = non-blocking mode
  res = ioctlsocket(sock, FIONBIO, &iMode);
  if (res != NO_ERROR) {
    printf("Failed to ioctrl on socket\n");
    closesocket(sock);
    return false;
  }
  printf("Success initializing network client\n");
  //info("Connected to server %s", config.server_ip.c_str());
  send_network_message(1);
  return true;
}

#endif // ifndef LINUX_FRAMEWORK
