/*
 * @file lorawan-ws.h
 */

#ifndef LORAWANWS_H_
#define LORAWANWS_H_	1

#include <map>
#include <string>
#include <iostream>
#include <fstream>

#include "lorawan-network-server/db-intf.h"

typedef std::map<std::string, DatabaseIntf*> MAP_NAME_DATABASE;

typedef struct 
{
	// listener port
	int port;
	// last error code
	int lasterr;
	// html root
	const char* dirRoot;
	// log verbosity
	int verbosity;
	// web server descriptor
	void *descriptor;
	// databases
	MAP_NAME_DATABASE databases;
} WSConfig;

void setLogFile(std::ostream* value);

/**
 * @param threadCount threads count, e.g. 2
 * @param connectionLimit mex connection limit, e.g. 1000
 * @param flags e.g. MHD_SUPPRESS_DATE_NO_CLOCK | MHD_USE_DEBUG | MHD_USE_SELECT_INTERNALLY
 * @param config listener descriptors, port number
 */ 
bool startWS(
	unsigned int threadCount,
	unsigned int connectionLimit,
	unsigned int flags,
	WSConfig &config
);

void doneWS(
	WSConfig &config
);

#endif
