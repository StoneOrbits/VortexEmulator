#pragma once

#include <Windows.h>
#include <inttypes.h>

// This is a network connection that simulates the IR connection between
// two vortex engines so that mode sharing can be tested

class IRSimulator
{
public:
  static bool init();
  static void cleanup();

  static bool send_message(uint32_t message);

  static bool startServer();
  static bool startClient();

  static bool isConnected() { return m_isConnected; }

private:
  static DWORD __stdcall wait_connection(void *arg);
  static void process_incoming_messages();
  static bool accept_connection();
  static bool receive_message(uint32_t &out_message);

  static SOCKET m_sock;
  static SOCKET m_clientSock;
  static bool m_isServer;
  static bool m_isConnected;
  static HANDLE m_hServerMutex;
};
