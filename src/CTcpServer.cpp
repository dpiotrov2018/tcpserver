//============================================================================
// Name        : CTcpServer.cpp
// Author      : Dmitry Piotrovsky for Percepto
// Version     : V0.1
// Copyright   :
// Description : CTcpServer class methods, C++11; Sockets using POSIX API.
//============================================================================


#include "CTcpServer.hpp"

//--------- Public Methods --------------

/*
 * Constructor.
 * Arguments:
 * int port - the port number
 * std::function<void(uint8_t*, int)> callback - the pointer to user function that will be called back by server
 * 		callback should support two arguments: uint8_t *buffer - buffer for the data; int max_length - size of the buffer
 */
CTcpServer::CTcpServer(int port, std::function<void(uint8_t*, int)> callback){

	server_thr = NULL;

	if((port > 0) && (port < 65535))
		srv_port = port;
	else
		throw std::runtime_error("Invalid port number");

	if(callback == NULL)
		throw std::runtime_error("Callback is NULL");
	else
		data_callback = callback;
}

/*
 * Destructor. Empty.
 */
CTcpServer::~CTcpServer(){}


/*
 * Start main thread of the server
 */
void CTcpServer::Start(){
	clientsck_cnt = 0;
	server_run = true;
	server_thr = new std::thread(&CTcpServer::ServerThread,this);
}

/*
 * Stop main thread of the server
 */
void CTcpServer::Stop(){
	server_run = false;
	server_thr->join();
}

/*
 * Pop data from data queue
 * arguments:
 * uint8_t *buffer - buffer for the data;
 * int max_length - size of the buffer
 * return actual length of the data
 */
int CTcpServer::PopData(uint8_t* buffer, int max_length){

	data_q_lock.lock();
	std::vector<uint8_t> m = data_q.back();
	data_q.pop_back();
	data_q_lock.unlock();

	size_t len = m.size();
	if(len > (size_t)max_length) len = (size_t)max_length;
	//truncating vector for buffer size:
	m.resize(len);
	std::copy(m.begin(), m.end(), buffer);

	m.clear();
	m.shrink_to_fit();

	return len;
};

/*
 * Pop debug messages from debug message queue
 * argument the string that filled by message if it exists
 * return how many strings was in queue before this pop
 */
int CTcpServer::pop_debug_msg(std::string& msg){
	int x;
	if((x = debug_q.size()) > 0){
		debug_q_lock.lock();
		msg = debug_q.back();
		debug_q.pop_back();
		debug_q_lock.unlock();
	}
	return x;
}

//------ Private methods -----------

/*
 * Method Implements main thread of the server
 */
void CTcpServer::ServerThread() {

	int server_socket;
	struct sockaddr_in serv_addr;
	struct sockaddr_in client_addr;
	int optval = 1;
	socklen_t len=sizeof(struct sockaddr);

	// ----- prepare server socket -----
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(srv_port);
    serv_addr.sin_addr.s_addr = INPUT_IP_FILTER;
    if((server_socket=socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	push_debug_msg("Socket creation Error: " + std::to_string(errno));
    	return;
    }
    if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR, &optval, sizeof(optval))){
    	push_debug_msg("Socket option set Error: " + std::to_string(errno));
    	close(server_socket);
    	return;
    }

   // int flags = fcntl(server_socket,F_GETFL,0);
   //if (flags < 0){
   // 	push_debug_msg("Socket fcntl Error: " + std::to_string(errno));
   // 	close(server_socket);
   // 	return;
   // }
   // fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    if(bind(server_socket, (struct sockaddr*) &serv_addr, len) < 0){
    	push_debug_msg("Socket binding Error: " + std::to_string(errno));
    	close(server_socket);
    	return;
    }

    if((listen(server_socket,MAX_SOCKET_BACKLOG)) < 0){
    	push_debug_msg("Socket listening Error: " + std::to_string(errno));
    	close(server_socket);
    	return;
    }
    push_debug_msg("Listening... (port:" + std::to_string(srv_port) + ")");

    //----- create async client thread pool ------
    std::thread * pthr = NULL;
    for(int x = 0; x < MAX_CLIENT_THREADS; x++){
    	pthr = new std::thread(&CTcpServer::async_client, this, x);
		if(pthr == NULL){
			push_debug_msg("async client thread creation error");
			break;
		}
		async_clients.push_back(pthr);
    }

    //----- accept loop variables ------
    int client_socket;
    int sock_cnt = 0;
    async_pool_run = true;

    //----- prepare select parameters -----
    fd_set set;
    struct timeval timeout;
    int res;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    //----- accept loop -----
    while(server_run.load()) {
    	if(sock_cnt < MAX_CLIENT_THREADS){

    		//waiting incoming connection on select:
    		//block for 3 sec. then check for stop flag and go next blocking or accept if data ready
    		FD_ZERO(&set);
    		FD_SET(server_socket, &set);
    		timeout.tv_sec = 3;
    		res = select(server_socket + 1, &set, NULL, NULL, &timeout);
    		if(res < 0){
    			// error - report and stop the server
    			push_debug_msg("Socket select Error: " + std::to_string(errno));
    			break; // stop server
    		}else if(res == 0){
    			// timeout happened - check for stop flag and continue
    			if(!server_run.load()) break;
    			continue;
    		}
    		//push_debug_msg("Incoming connection...");

    		client_socket=accept(server_socket,(struct sockaddr*) &client_addr,&len);

			if( client_socket >= 0 ){
				// add new client socket to queue
				socket_q_lock.lock();
				clientsck_queue.push_back(client_socket);
				sock_cnt = clientsck_queue.size();
				socket_q_lock.unlock();

				// notify one of the async workers about new socket in queue
				{
					std::unique_lock<std::mutex> ulk(async_client_mutex);
					async_client_cv.notify_one();
					ulk.~unique_lock();
				}

			}else{
				push_debug_msg("Socket accept Error: " + std::to_string(errno));
				std::this_thread::sleep_for( std::chrono::milliseconds(MAIN_SLEEP_MS) );
			}




		}else{
			// queue overflow, so, don't accept new before done previous accepted
			socket_q_lock.lock();
			sock_cnt = clientsck_queue.size();
			socket_q_lock.unlock();
			{
				std::unique_lock<std::mutex> ulk(async_client_mutex);
				async_client_cv.notify_one();
				ulk.~unique_lock();
			}
		}

    }
    close(server_socket);
    async_pool_run = false;
    async_client_cv.notify_all();
    // check that all async clients where ends and clean it
    if(async_clients.size() > 0){
		for (auto & element : async_clients) {
			if(element->joinable()) element->join();
			delete element;
		}
    }
}


/*
 * Method Implements asynchronous client thread
 */
void CTcpServer::async_client(int id){

	uint8_t* buffer =  (uint8_t*)malloc(DATA_LEN_LIMIT + 2);
	if(buffer == NULL){
		push_debug_msg("Buffer allocation error");
		return;
	}

	// async client loop start
	int sck = 0;
	int cnt = 0;
	bool socket_ready = false;

	while(true){ // see checking for stop flag above

		// wait for socket to work
		do{
			// wait on condition variable for notyfy_one from accept loop thread
			{
				std::unique_lock<std::mutex> ulk(async_client_mutex);
				async_client_cv.wait(ulk);
				ulk.~unique_lock();
			}
			// get target socket from queue
			socket_q_lock.lock();
			if(!clientsck_queue.empty()){
				sck = clientsck_queue.back();
				clientsck_queue.pop_back(); // remove socket that got to work
				socket_ready = true;
			}else{
				socket_ready = false;
			}
			socket_q_lock.unlock();

			// check for stop flag
			if(!async_pool_run.load())break;

		}while(!socket_ready);

		// check for stop flag
		if(!async_pool_run.load())break;

		// got socket - do the work

		// Implementation of the simple communication protocol:
		// header 4 bytes:
		// 1 byte protocol version
		// 2 byte code of the operation
		// 3 and 4 bytes (uint16_t little-endian) length of the data block that continue after header.
		// 5 ... bytes data block
		// After data successfully received, it send "OK\0" sequence as answer.
		// If data not properly received, just close the socket. Client side will get exception about it.
		memset(buffer, 0, DATA_LEN_LIMIT + 2);
		//read first 4 bytes containing length of the rest of the data
		cnt = recvbytes(sck, buffer, DATA_HEADER_LENGTH);
		if(cnt != DATA_HEADER_LENGTH){
			push_debug_msg("Invalid header length (" + std::to_string(cnt)+ ")");
			close(sck);
			continue; // do not end the thread loop on error - go to next client socket
		}
		int datalen = *((uint16_t*) (buffer+DATA_LENGTH_OFFSET));
		if(DATA_LEN_LIMIT < (datalen + DATA_HEADER_LENGTH)){
			push_debug_msg("Invalid data length value in header (" + std::to_string(datalen)+ ")");
			close(sck);
			continue;// do not end the thread loop on error - go to next client socket
		}
		//read rest of the data
		cnt = recvbytes(sck, buffer + DATA_HEADER_LENGTH, datalen);
		if(cnt != datalen){
			push_debug_msg("Data actual length is not equal to header value (" + std::to_string(cnt) + ")");
			close(sck);
			continue;// do not end the thread loop on error - go to next client socket
		}
		// Data passed OK,
		// response to client
		cnt = sendbytes(sck, (uint8_t *)"OK\0", 3);

		// socket not needed more
		close(sck);

		//save data to queue
		if(QEUE_LIMIT > data_q.size()){
			std::vector<uint8_t> dataVector(buffer, buffer + (datalen + DATA_HEADER_LENGTH));
			data_q_lock.lock();
			data_q.push_back(dataVector);
			data_q_lock.unlock();
			dataVector.clear();
			dataVector.shrink_to_fit();
		}

		// call user callback
		data_callback(buffer, (datalen + DATA_HEADER_LENGTH));

	} // end of async client loop

	// clean buffer
	free(buffer);
}

/*
 * Simple sending bytes to socket
 */
int CTcpServer::sendbytes(int sck, uint8_t * buff, int exp){
	uint8_t *p = buff;
	ssize_t len = (ssize_t)exp;
	ssize_t n;
	while ( len > 0 && (n=send(sck,p,len,0)) > 0 ) {
	  p += n;
	  len -= (size_t)n;
	}
	return exp - len;
}

/*
 * Simple receive bytes from socket
 */
int CTcpServer::recvbytes(int sck, uint8_t * buff, int exp){
	uint8_t *p = buff;
	ssize_t len = (ssize_t)exp;
	ssize_t n;
	while ( len > 0 && (n=recv(sck,p,len,0)) > 0 ) {
		  p += n;
		  len -= (size_t)n;
	}
	return exp - len;
}

/*
 * Push debug messages to debug message queue
 */
void CTcpServer::push_debug_msg(std::string msg){
	debug_q_lock.lock();
	debug_q.push_front(msg);
	debug_q_lock.unlock();
}


