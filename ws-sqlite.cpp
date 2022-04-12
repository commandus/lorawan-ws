/**
 *
 */
#include <iostream>
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <microhttpd.h>
#include <sqlite3.h>

#include "argtable3/argtable3.h"

#include "platform.h"

#include "lorawan-ws.h"
#include "lorawan-network-server/db-sqlite.h"
#include "db-helper.h"

#include "daemonize/daemonize.h"

const char *progname = "ws-sqlite";

#define MHD_START_FLAGS 	MHD_USE_POLL | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_SUPPRESS_DATE_NO_CLOCK | MHD_USE_TCP_FASTOPEN | MHD_USE_TURBO

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

static WSConfig config;
static DatabaseSQLite dbSqlite;

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
	WSConfig *retval,
	std::string &retDbConnection,
	bool &createTable,
	bool &daemonize,
	int argc,
	char* argv[]
)
{
	struct arg_int *a_listenport = arg_int0("p", "port", "<port>", "port number. Default 5002");
	struct arg_str *a_dirroot = arg_str0("r", "root", "<path>", "web root path. Default './html'");
	struct arg_str *a_database = arg_str0("d", "database", "<file>", "SQLite database file name. Default " DEF_DB_FN);

	struct arg_lit *a_create_table = arg_lit0("c", "create-table", "force create table in database");
	struct arg_lit *a_daemonize = arg_lit0(NULL, "daemonize", "run daemon");
	struct arg_lit *a_verbosity = arg_litn("v", "verbosity", 0, 4, "v- error, vv- warning, vvv, vvvv- debug");
	struct arg_file *a_logfile = arg_file0("l", "log", "<file>", "log file");
	
	struct arg_lit *a_help = arg_lit0("h", "help", "Show this help");
	struct arg_end *a_end = arg_end(20);

	void* argtable[] = { a_listenport, a_dirroot, a_database, a_logfile,
		a_create_table, a_daemonize,
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

	createTable = a_create_table->count > 0;
	daemonize = a_daemonize->count > 0;

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

	retDbConnection = *a_database->sval;

	if (retDbConnection.empty())
		retDbConnection = DEF_DB_FN;

	if (!createTable) {
	// check database connection
	if (!checkDbConnection(retDbConnection))
		{
			std::cerr << "Invalid database file " << retDbConnection << std::endl;
			return 2;
		}
	}

	arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
	return 0;
}

static void done()
{
	dbSqlite.close();
}

static void start()
{
	if (!startWS(NUMBER_OF_THREADS, 1000, MHD_START_FLAGS, config)) {
		std::cerr << "Can not start web service errno " 
			<< errno << ": " << strerror(errno) << std::endl;
			std::cerr << "libmicrohttpd version " << std::hex << MHD_VERSION << std::endl;
	}
}

static void stop()
{
	doneWS(config);
}

void signalHandler(int signal)
{
	switch(signal)
	{
	case SIGINT:
		std::cerr << "Interrupted..";
		done();
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
	bool createTable;
	bool daemonize;
	std::string dbFileName;

	int r = parseCmd(&config, dbFileName, createTable, daemonize, argc, argv);
	if (createTable) {
		createTables(dbFileName);
		if (config.lasterr)
			std::cerr << "Error create tables " << config.lasterr << std::endl;
		exit(0);
	}
	if (r)
		exit(r);


	dbSqlite.open(dbFileName, "", "", "", 0);
	config.databases[""] = &dbSqlite;
	config.databases["sqlite"] = &dbSqlite;

	if (daemonize) {
		char wd[PATH_MAX];
		std::string progpath = getcwd(wd, PATH_MAX);
		Daemonize daemonize(progname, progpath, start, stop, done);
	} else {
		// Signal handler
		setSignalHandler(SIGINT);
		if (config.verbosity)
			std::cerr << "SQLite version " << SQLITE_VERSION << std::endl;

		start();

		while (true)
		{
			getc(stdin);
		}
		stop();
		done();
	}
	return 0;
}
