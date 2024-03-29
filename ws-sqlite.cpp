#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <iostream>
#include <string.h>
#include <signal.h>
#include <limits.h>

#ifdef ENABLE_JWT
#include "auth-jwt.h"
#endif

#include <microhttpd.h>
#include <sqlite3.h>

#include "argtable3/argtable3.h"

#include "platform.h"

#include "lorawan-ws.h"
#include "lorawan-network-server/db-sqlite.h"
#include "db-helper.h"

#include "daemonize/daemonize.h"

const char *progname = "ws-sqlite";

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
static std::ofstream *logFileStrm = nullptr;

std::string getPathFirstFragments(const std::string &value)
{
	const size_t last_slash_idx = value.rfind('\\');
	if (std::string::npos != last_slash_idx)
		return value.substr(0, last_slash_idx);
	else
		return value;
}

class StdErrOrFileLog: public LogIntf {
public:
    void logMessage(
        void *env,
        int level,
        int moduleCode,
        int errorCode,
        const std::string &message
    ) override {
		if (logFileStrm) 
			*logFileStrm << message << std::endl;
		else
			std::cerr << message << std::endl;
    }
};

StdErrOrFileLog stdErrOrFileLog;

class WsDumbRequestHandler : public WebServiceRequestHandler {
    int handle(
        std::string &content,
        std::string &contentType,
        void *env,
        int modulecode,
        // copy following parameters from the web request
        const char *path,
        const char *method,
        const char *version,
        std::map<std::string, std::string> &params,
        const char *upload_data,
        size_t *upload_data_size,
        bool authorized
    ) override
    {
        std::string p(path);
        if (p == "/about") {
            content = "<html><body><h1>lorawan-ws</h1><a href=\"https://github.com/commandus/lorawan-ws\">GitHub</a></body></html>";
            contentType = "text/html;charset=UTF-8";
            return 200;
        }
        return 404;
    }
};

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
    struct arg_int *a_listenflags = arg_int0("f", "flags", "<number>", "0- defaults");
	struct arg_str *a_dirroot = arg_str0("r", "root", "<path>", "web root path. Default './html'");
	struct arg_str *a_database = arg_str0("d", "database", "<file>", "SQLite database file name. Default " DEF_DB_FN);

#ifdef ENABLE_JWT
    struct arg_str *a_issuer = arg_str0("i", "issuer", "<JWT-realm>", "JWT issuer name");
    struct arg_str *a_secret = arg_str0("s", "secret", "<JWT-secret>", "JWT secret");
#endif
    struct arg_lit *a_create_table = arg_lit0("c", "create-table", "force create table in database");
	struct arg_lit *a_daemonize = arg_lit0("z", "daemonize", "run daemon");
	struct arg_lit *a_verbosity = arg_litn("v", "verbosity", 0, 4, "v- error, vv- warning, vvv, vvvv- debug");
	struct arg_file *a_logfile = arg_file0("l", "log", "<file>", "log file");
	
	struct arg_lit *a_help = arg_lit0("h", "help", "Show this help");
	struct arg_end *a_end = arg_end(20);

	void* argtable[] = { a_listenport, a_listenflags, a_dirroot, a_database, a_logfile,
#ifdef ENABLE_JWT
        a_issuer, a_secret,
#endif
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

	retval->threadCount = NUMBER_OF_THREADS;
	retval->connectionLimit = 1024;
    if (a_listenflags->count) {
        retval->flags = *a_listenflags->ival;
    } else {
        retval->flags = MHD_START_FLAGS;
    }
	retval->onLog = &stdErrOrFileLog;

#ifdef ENABLE_JWT
    retval->issuer = *a_issuer->sval;
    retval->secret = *a_secret->sval;
#endif

	if (a_logfile->count)
		logFileStrm = new std::ofstream(*a_logfile->filename);

	if (a_listenport->count)
		retval->port = *a_listenport->ival;
	else
		retval->port = DEF_PORT;

	retDbConnection = *a_database->sval;

	if (retDbConnection.empty())
		retDbConnection = DEF_DB_FN;

	if (!createTable) {
        // check database connection
        if (!checkDbConnection(retDbConnection)) {
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
	if (logFileStrm) {
		delete logFileStrm;
		logFileStrm = nullptr;
	}
}

static void start()
{
    if (!startWS(config)) {
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
	sigaction(signal, &action, nullptr);
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

    // web service special path
    WsDumbRequestHandler dumbPathHandler;
    config.onSpecialPathHandler = &dumbPathHandler;

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
