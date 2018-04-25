//============================================================================
// Name        : test_server.cpp
// Author      : Dmitry Piotrovsky for Percepto
// Version     : V0.1
// Copyright   :
// Description : Test server for CTcpServer class, C++11
//============================================================================

#include "CTcpServer.hpp"
#include <string>
#include <list>
#include <atomic>
#include <signal.h>

#define TESTPORT 	4444

std::list<std::string> msg;
std::mutex msg_lock;
std::atomic_bool interrupt = {false};

/*
 * Function to test user callback from class
 */
void data_callback(uint8_t * buff, int datalen){
	msg_lock.lock();
	msg.push_front(std::string(buff + DATA_HEADER_LENGTH, buff + DATA_HEADER_LENGTH + datalen));
	msg_lock.unlock();
}

/*
 * Signal handler for properly stop the application
 */
static void s_sighandler (int signal_value)
{
	interrupt = true;
}
/*
 * Signal handling parameter setup for properly stop the application
 */
static void s_siginit (void)
{
    struct sigaction action;
    action.sa_handler = s_sighandler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);
}

/*
 * Main of the test server
 */
int main() {
	msg.clear();
	s_siginit();
	std::cout << "TcpServer Starting... (To stop press Ctrl+C)" << std::endl;
	std::cout << "TcpServer Creating Object." << std::endl;

	// Creating the object of the class
	CTcpServer srv(TESTPORT, data_callback);

	std::cout << "TcpServer Starting Main thread." << std::endl;
	// Start main server thread after completion of the constructor
	srv.Start();

	// Test main loop that receive and print out messages from shared lists
	std::cout << "TcpServer Main loop." << std::endl;
	// loop while signal like Ctrl-C arrives
	std::string md;
	int x, y;
	while(! interrupt.load()){

		// Debug messages from Object
		x = srv.pop_debug_msg(md);
		if(x > 0){
			std::cout << "debug: " + md << std::endl;
		}

		// Messages from Callback function
		y = msg.size();
		if(y > 0){
			msg_lock.lock();
			md = msg.back();
			msg.pop_back();
			msg_lock.unlock();
			std::cout << "Callback function: " + md << std::endl;
		}

		if((x == 0) && (y == 0)){
			// big sleep if no printouts
			std::this_thread::sleep_for( std::chrono::milliseconds(50));
		}

		// this sleep should be balanced between CPU loading by main loop of the test at printout time.
		// with it printout delay may be big - so to be patient.
		//std::this_thread::sleep_for( std::chrono::milliseconds(10));


	}

	// Properly stop the server
	std::cout << std::endl << "TcpServer Stopping. Please wait about 3-4 sec." << std::endl;
	srv.Stop();
	std::cout << "Good bye." << std::endl;

	return 0;
}
