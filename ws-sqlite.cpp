/**
 *
 */
#include <iostream>
#include <string.h>
#include <signal.h>

#include "argtable3/argtable3.h"

#include "platform.h"

#include "lorawan-ws.h"

const char *progname = "ws-sqlite";

#define MHD_START_FLAGS 		0

/**
 * Number of threads to run in the thread pool.  Should (roughly) match
 * the number of cores on your system.
 */

#if defined(CPU_COUNT) && (CPU_COUNT+0) < 2
#undef CPU_COUNT
#endif
#if !defined(CPU_COUNT)
#define CPU_COUNT 2
#endif

#define NUMBER_OF_THREADS CPU_COUNT

#define DEF_PORT	5002

#define DEF_DB_FN "../lorawan-network-server/db/logger-huffman.db"

wsConfig config;

std::string getPathFirstFragments(const std::string &value)
{
	const size_t last_slash_idx = value.rfind('\\');
	if (std::string::npos != last_slash_idx)
		return value.substr(0, last_slash_idx);
	else
		return value;
}

/**
 * Parse command line into struct ggdbsvcConfig
 * Return 0- success
 *        1- show help and exit, or command syntax error
 *        2- output file does not exists or can not open to write
 **/
int parseCmd
(
	struct wsConfig *retval,
	int argc,
	char* argv[]
)
{
	struct arg_int *a_listenport = arg_int0("l", "listen", "<port>", "port number. Default 5002");
	struct arg_str *a_dirroot = arg_str0("r", "root", "<path>", "web root path. Default './html'");
	struct arg_str *a_database = arg_str0("d", "database", "<file>", "SQLite database file name. Default "DEF_DB_FN);

	struct arg_lit *a_verbosity = arg_litn("v", "verbosity", 0, 4, "v- error, vv- warning, vvv, vvvv- debug");
	struct arg_file *a_logfile = arg_file0("l", "log", "<file>", "log file");
	
	struct arg_lit *a_help = arg_lit0("h", "help", "Show this help");
	struct arg_end *a_end = arg_end(20);

	void* argtable[] = { a_listenport, a_dirroot, a_logfile,
		a_verbosity, a_help, a_end
	};

	int nerrors;

	// verify the argtable[] entries were allocated successfully
	if (arg_nullcheck(argtable) != 0)
	{
		arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
		return 1;
	}
	// Parse the command line as defined by argtable[]
	nerrors = arg_parse(argc, argv, argtable);

	// special case: '--help' takes precedence over error reporting
	if ((a_help->count) || nerrors)
	{
		if (nerrors)
			arg_print_errors(stderr, a_end, progname);
		printf("Usage: %s\n",  progname);
		arg_print_syntax(stdout, argtable, "\n");
		printf("GPS satellite level db service\n");
		arg_print_glossary(stdout, argtable, "  %-25s %s\n");
		arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
		return 1;
	}

	if (a_dirroot->count)
		retval->dirRoot = *a_dirroot->sval;
	else
		retval->dirRoot = "html";

	retval->verbosity = a_verbosity->count;

	if (a_logfile->count)
	{
		std::ostream *f = new std::ofstream(*a_logfile->filename);
		if (f)
			setLogFile(f);
	}

	if (a_listenport->count)
		retval->port = *a_listenport->ival;
	else
		retval->port = DEF_PORT;

	retval->dbfilename = *a_database->sval;

	if ((!retval->dbfilename) || (strlen(retval->dbfilename) == 0))
		retval->dbfilename = DEF_DB_FN;

	// check database connection
	if (!checkDbConnection(retval))
	{
		fprintf(stderr, "Invalid database file %s\n", retval->dbfilename);
		return 2;
	}

	arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
	return 0;
}

void signalHandler(int signal)
{
	switch(signal)
	{
	case SIGINT:
		std::cerr << "Interrupted..";
		doneWS(config);
		exit(signal);
	default:
		std::cerr << "Signal " << signal;
	}
}

void setSignalHandler(int signal)
{
#ifndef _MSC_VER
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = &signalHandler;
	sigaction(signal, &action, NULL);
#endif
}

int main(int argc, char* argv[])
{
	wsConfig config;
	int r = parseCmd(&config, argc, argv);
	if (r)
		exit(r);

	// Signal handler
	setSignalHandler(SIGINT);

	if (!startWS(NUMBER_OF_THREADS, 1000, MHD_START_FLAGS, config))
		return 1;

	while (true)
	{
		getc(stdin);
	}

	doneWS(config);
	return 0;
}
