//============================================================================
// Name        : CTcpServer.hpp
// Author      : Dmitry Piotrovsky for Percepto
// Version     : V0.1
// Copyright   :
// Description : CTcpServer class header, C++11; Sockets using POSIX API.
//============================================================================

#ifndef CTCPSERVER_HPP_
#define CTCPSERVER_HPP_

#include <iostream>
#include <thread>
#include <chrono>
#include <deque>
#include <list>
#include <vector>
#include <mutex>
#include <future>
#include <algorithm>
#include <condition_variable>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


class CTcpServer {

#define INPUT_IP_FILTER 	(INADDR_ANY)
#define QEUE_LIMIT 			100
#define DATA_LEN_LIMIT 		512
#define MAIN_SLEEP_MS 		20
#define MAX_CLIENT_THREADS 	10
#define MAX_SOCKET_BACKLOG 	10
#define SLEEP_ON_ACCEPT_BARIER 10000

#define DATA_HEADER_LENGTH	4
#define DATA_VERSION_OFFSET	0
#define DATA_OPCODE_OFFSET	1
#define DATA_LENGTH_OFFSET	2 /*length format is uint16_t*/
#define PROTOVERSION = 0x01
#define OPCODE = 0x5a  # test operation

public:


	/*
	 * Constructor.
	 * Arguments:
	 * int port - the port number
	 * std::function<void(uint8_t*, int)> callback - the pointer to user function that will be called back by server
	 * 		callback should support two arguments: uint8_t *buffer - buffer for the data; int max_length - size of the buffer
	 */
	CTcpServer(int port, std::function<void(uint8_t*, int)> callback);

	/*
	 * Destructor. Empty.
	 */
	~CTcpServer();

	/*
	 * Start main thread of the server
	 */
	void Start();

	/*
	 * Stop main thread of the server
	 */
	void Stop();

	/*
	 * Pop data from data queue
	 * arguments:
	 * uint8_t *buffer - buffer for the data;
	 * int max_length - size of the buffer
	 * return actual length of the data
	 */
	int PopData(uint8_t* buffer, int max_length);

	/*
	 * Pop debug messages from debug message queue
	 * argument the string that filled by message if it exists
	 * return how many strings stay in queue
	 */
	int pop_debug_msg(std::string& msg);


private:

	//Main server thread handle and run flag.
    std::thread* server_thr;
    std::atomic_bool server_run;

    //Asynchronous client handler variables
    std::atomic_int clientsck_cnt;
    std::list<std::thread *> async_clients;
    std::mutex socket_q_lock;

    std::mutex async_client_mutex;
    std::condition_variable async_client_cv;

    std::atomic_bool async_pool_run;
    std::deque<int> clientsck_queue;

    // Srver port
    int srv_port;

    // Callback function pointer
    std::function<void(uint8_t*, int)> data_callback;

    // Data queue storage and lock
    std::deque<std::vector<uint8_t>> data_q;
    std::mutex data_q_lock;

    // Debug messages queue storage and lock
    std::deque<std::string> debug_q;
    std::mutex debug_q_lock;

    /*
     * Method Implements main thread of the server
     */
    void ServerThread();

    /*
     * Method Implements asynchronous client connection handler
     */
    void async_client(int id);

    /*
     * Simple sending bytes to socket
     */
    int sendbytes(int sck, uint8_t * buff, int cnt);

    /*
     * Simple receive bytes from socket
     */
    int recvbytes(int sck, uint8_t * buff, int cnt);

    /*
     * Push debug messages to debug message queue
     */
    void push_debug_msg(std::string msg);

};

#endif /* CTCPSERVER_HPP_ */
