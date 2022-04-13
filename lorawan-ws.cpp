/**
 *
 */

#include <cstdlib>
#include <cstdbool>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "platform.h"

#include <sys/stat.h>

#ifndef _MSC_VER
#include <sys/time.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <microhttpd.h>
// Caution: version may be different, if microhttpd dependecy not compiled, revise versiuon humber
#if MHD_VERSION <= 0x00096600
#define MHD_Result int
#endif
#ifndef MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS
#define MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS "Access-Control-Allow-Credentials"
#endif
#ifndef MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS
#define MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS "Access-Control-Allow-Methods"
#endif
#ifndef MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS
#define MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS "Access-Control-Allow-Headers"
#endif

#define	LOG_ERR								3
#define	LOG_INFO							5

#define MODULE_WS	200

#include "lorawan-ws.h"

LOG_CALLBACK logCB = NULL;

void setLogCallback(LOG_CALLBACK value)
{
	logCB = value;
}

#define PATH_COUNT 6

typedef enum
{
	RT_RAW = 0,			//< List of raw
	RT_T = 1,			//< List of values(temperatures)
	RT_RAW_COUNT = 2,	//> Count of raw records
	RT_T_COUNT = 3,		//> Count of value records
	RT_RAW_1 = 4,		//> One raw record (by id)
	RT_T_1 = 5,			//> One value record (by id)
	RT_UNKNOWN = 100	//< file request

} RequestType;

const char *paths[PATH_COUNT] = {
	"/raw",
	"/t",
	"/raw-count",
	"/t-count",
	"/raw-id",
	"/t-id"};

typedef enum
{
	CT_ID = 0,			//< Id
	CT_KOSA = 1,		//< Kosa
	CT_YEAR = 2,		//> Kosa year
	CT_NO = 3,			//> Number
	CT_MEASURED = 4,	//> Measured time
	CT_PARSED = 5,		//> Parsed time
	CT_VCC = 6,			//> Vcc
	CT_VBAT = 7,		//> Vbat 
	CT_T = 8,			//> Measured temperature
	CT_RAW = 9,			//> raw packets in hex
	CT_DEVNAME = 10,	//> 
	CT_LORAADDR = 11,	//> 
	CT_RECEIVED = 12	//> 
} ColumnTemperature;

const char *pathSelectPrefix[PATH_COUNT] = {
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, raw, devname, loraaddr, received FROM logger_lora",
	"SELECT count(id) cnt FROM logger_raw",
	"SELECT count(id) cnt FROM logger_lora",
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw WHERE id = ?1",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, raw, devname, loraaddr, received FROM logger_lora WHERE id = ?1"
};

const char *pathSelectSuffix[PATH_COUNT] = {
	"ORDER BY id DESC LIMIT ?1, ?2;",
	"ORDER BY id DESC LIMIT ?1, ?2;",
	"",
	"",
	"",
	""
};

#define QUERY_PARAMS_SIZE 15
#define QUERY_PARAMS_OPTIONAL_MIN	2
#define QUERY_PARAMS_OPTIONAL_MAX	QUERY_PARAMS_SIZE - 1

// first QUERY_PARAMS_REQUIRED_MAX = 3 parameters can be required
static const char* queryParamNames[QUERY_PARAMS_SIZE] = {
	"o",	    "s",		"id",		"measured",	"parsed",
	"no"	,	"kosa", 	"year",		"raw",		"t",
	"vcc",		"vbat",		"devname",	"loraaddr",	"received"
};

static const char* queryDbParamName = "db";

static const bool queryParamIsString[QUERY_PARAMS_SIZE] = {
	false,	    false,		false,		false,		false,
	false,	    false,		false,		true,		false,
	false,	    false,		true,		true,		true
};

#define SQL_STRING_QUOTE	"\'"

#define QUERY_PARAMS_SUFFIX_SIZE	5

static const char* queryParamSuffix[QUERY_PARAMS_SUFFIX_SIZE] = {
	"-gt",	    "-ge",		"-lt",		"-le",		"-like"
};

static const char* queryParamSQLComparisonOperator[QUERY_PARAMS_SUFFIX_SIZE] = {
	">",	    ">=",		"<",		"<=",		"LIKE"
};

#define EOP -1
#define QUERY_PARAMS_REQUIRED_MAX 3

const int pathRequiredParams[PATH_COUNT][QUERY_PARAMS_REQUIRED_MAX] = {
	{ 0, 1, EOP },
	{ 0, 1, EOP },
	{ EOP, 0, 0 },
	{ EOP, 0, 0 },
	{ 2, EOP, 0 },
	{ 2, EOP, 0 }
};

const static char *MSG404 = "404 not found";
const static char *CT_HTML = "text/html;charset=UTF-8";
const static char *CT_JSON = "text/javascript;charset=UTF-8";
const static char *CT_KML = "application/vnd.google-earth.kml+xml";
const static char *CT_PNG = "image/png";
const static char *CT_JPEG = "image/jpeg";
const static char *CT_CSS = "text/css";
const static char *CT_TEXT = "text/plain;charset=UTF-8";
const static char *CT_TTF = "font/ttf";
const static char *CT_BIN = "application/octet";
const static char *HDR_CORS_ORIGIN = "*";
const static char *HDR_CORS_CREDENTIALS = "true";
const static char *HDR_CORS_METHODS = "GET,HEAD,OPTIONS,POST,PUT";
const static char *HDR_CORS_HEADERS = "Authentication, Access-Control-Allow-Headers, Origin, Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers";

typedef enum {
	START_FETCH_DB_OK = 0,
	START_FETCH_DB_CONNECT_FAILED = 1,
	START_FETCH_DB_PREPARE_FAILED = 2,
	START_FETCH_DB_NO_PARAM = 3,
	START_FETCH_DB_BIND_PARAM = 4
} START_FETCH_DB_RESULT;

const static char *MSG500[5] = {
	"200 OK",
	"Database connection not established",
	"SQL statement preparation failed",
	"Required parameter missed",
	"Binding parameter failed"
};

typedef struct 
{
	RequestType requestType;
} RequestParams;

typedef struct{
	int state;
} OutputState;

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

typedef struct 
{
	RequestParams request;
	DatabaseIntf *db;		// database interface each request in separate connection
	void *stmt;				// SQL statement
	WSConfig *config;
	OutputState state;
} RequestEnv;

static RequestType parseRequestType(const char *url)
{
	int i;
	for (i = 0; i < PATH_COUNT; i++)
	{
		if (strcmp(paths[i], url) == 0)
			return (RequestType) i;
	}
	return RT_UNKNOWN;
}

const char *requestTypeString(RequestType value)
{
	if (value == RT_UNKNOWN)
		return "Unknown";
	if ((value >= 0) && (value < PATH_COUNT))
		return paths[(int) value];
	else
		return "???";
}

void *uri_logger_callback (void *cls, const char *uri)
{
	if (logCB)
		logCB(cls, LOG_INFO, 0, 0, uri);
	return NULL;
}

static int doneFetch(
	RequestEnv *env
)
{
	if (!env->stmt)
		return 0;
	int r = env->db->cursorClose(env->stmt);
	if (r) {
		if (logCB)
			logCB(env, LOG_ERR, MODULE_WS, r, "Cursor close error " + env->db->errmsg);
		return START_FETCH_DB_PREPARE_FAILED;
	}
	env->stmt = NULL;
	return 0;
}

const char *NULLSTR = "";

static ssize_t file_reader_callback(void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = (FILE *) cls;
	(void) fseek (file, (long) pos, SEEK_SET);
	return fread (buf, 1, max, file);
}

static void free_file_reader_callback(void *cls)
{
	fclose ((FILE *) cls);
}

static const char *mimeTypeByFileExtention(const std::string &filename)
{
	std::string ext = filename.substr(filename.find_last_of(".") + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	if (ext == "html")
		return CT_HTML;
	else
		if (ext == "htm")
			return CT_HTML;
	else
		if (ext == "js")
			return CT_JSON;
	else
		if (ext == "css")
			return CT_CSS;
	if (ext == "png")
		return CT_PNG;
	else
		if (ext == "jpg")
			return CT_JPEG;
	else
		if (ext == "jpeg")
			return CT_JPEG;
	else
		if (ext == "kml")
			return CT_KML;
	else
		if (ext == "txt")
			return CT_TEXT;
	else
		if (ext == "ttf")
			return CT_TTF;
	else
		return CT_BIN;

}

static MHD_Result processFile(struct MHD_Connection *connection, const std::string &filename)
{
	struct MHD_Response *response;
	MHD_Result ret;
	FILE *file;
	struct stat buf;

	const char *fn = filename.c_str();

	if (stat(fn, &buf) == 0)
		file = fopen(fn, "rb");
	else
		file = NULL;
	if (file == NULL) {
		if (logCB)
			logCB(connection, LOG_ERR, MODULE_WS, 404, "File not found " + std::string(fn));

		response = MHD_create_response_from_buffer(strlen(MSG404), (void *) MSG404, MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
		MHD_destroy_response (response);
	} else {
		response = MHD_create_response_from_callback(buf.st_size, 32 * 1024,
			&file_reader_callback, file, &free_file_reader_callback);
		if (NULL == response)
		{
			fclose (file);
			return MHD_NO;
		}

		MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mimeTypeByFileExtention(filename));
		ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	return ret;
}

/**
 * Translate Url to the file name
 */
static std::string buildFileName(const char *dirRoot, const char *url)
{
	std::stringstream r;
	r << dirRoot;
	if (url)
	{
		r << url;
		int l = strlen(url);
		if (l && (url[l - 1] == '/'))
			r << "index.html";
	}
	return r.str();
}

static START_FETCH_DB_RESULT startFetchDb(
	struct MHD_Connection *connection,
	RequestEnv *env
)
{
	std::stringstream pathSelectSS;
	pathSelectSS << pathSelectPrefix[env->request.requestType];
	// get by identifier- no optional conditions
	if (!((env->request.requestType == RT_RAW_1) || (env->request.requestType == RT_T_1))) {
		// build WHERE clause
		bool isFirst = true;
		for (int i = QUERY_PARAMS_OPTIONAL_MIN; i <= QUERY_PARAMS_OPTIONAL_MAX; i++) {
			const char *pn = queryParamNames[i];
			const char* c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, pn);
			if (c) {
				// equals
				if (isFirst) {
					pathSelectSS << " WHERE ";
					isFirst = false;
				} else {
					pathSelectSS << " AND ";
				}
				pathSelectSS << pn << " = ";
				if (queryParamIsString[i])
					pathSelectSS << SQL_STRING_QUOTE << c << SQL_STRING_QUOTE;
				else
					pathSelectSS << c;
				// do not check other contitions
				continue;
			}
			for (int pns = 0; pns < QUERY_PARAMS_SUFFIX_SIZE; pns++) {
				std::string pnc = std::string(pn) + queryParamSuffix[pns];
				const char* cc = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, pnc.c_str());
				if (cc) {
					if (isFirst) {
						pathSelectSS << " WHERE ";
						isFirst = false;
					} else
						pathSelectSS << " AND ";
					pathSelectSS << pn << " " << queryParamSQLComparisonOperator[pns] << " ";
					if (queryParamIsString[i])
						pathSelectSS << SQL_STRING_QUOTE << cc << SQL_STRING_QUOTE;
					else
						pathSelectSS << cc;
					// Do not break, check other contitions
					// break;
				}
			}
		}
	}

	// finish WHERE clause
	pathSelectSS << " " << pathSelectSuffix[env->request.requestType];
	std::string pathSelect = pathSelectSS.str();

	// preparation
	int r = env->db->cursorOpen(&env->stmt, pathSelect);
	if (r) {
		if (logCB) {
			std::stringstream ss;
			ss << "Fetch error " << r 
				<< ": " << env->db->errmsg
				<< " db " << env->db->type
				<< " SQL " << pathSelect.c_str();
			logCB(env, LOG_ERR, MODULE_WS, r, ss.str());
		}
		return START_FETCH_DB_PREPARE_FAILED;
	}

	// bind required params
	for (int i = 0; i < QUERY_PARAMS_REQUIRED_MAX; i++)	{
		int v = pathRequiredParams[env->request.requestType][i];
		if (v == EOP)
			break; // no more parameters
		const char *c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryParamNames[v]);
		if (!c) {
			if (logCB) {
				std::stringstream ss;
				ss << "Binding parameter #" << i + 1
					<< " " << queryParamNames[v]
					<< " empty";
				logCB(env, LOG_ERR, MODULE_WS, r, ss.str());
			}
			env->db->cursorClose(env->stmt);
			return START_FETCH_DB_NO_PARAM;
		} else {	
			r = env->db->cursorBindText(env->stmt, i + 1, c);
			if (r) {
				if (logCB) {
					std::stringstream ss;
					ss << "Bind parameter error "
						<< r << ": " << env->db->errmsg
						<< ", parameter #" << i + 1
						<< " " << queryParamNames[v]
						<< " = " << c;
					logCB(env, LOG_ERR, MODULE_WS, r, ss.str());
				}
				env->db->cursorClose(env->stmt);
				return START_FETCH_DB_BIND_PARAM;
			}
		}
	}
	return START_FETCH_DB_OK;
}

static size_t result2json(
	char *buf,
	size_t bufSize,
	RequestEnv *env
)
{
	std::stringstream ss;
	int st = env->state.state;
	size_t sz;

	switch (st)
	{
		case 0:
			env->state.state = 1;
			ss << "[{";
			break;
		case 1:
			ss << "}, {";
			break;
		case 2:
			sz = snprintf(buf, bufSize, "}]");
			return sz;
		case 3:
			sz = snprintf(buf, bufSize, "[]");
			return sz;
		default:
			sz = snprintf(buf, bufSize, "]");
			return sz;
	}
	int columns = env->db->cursorColumnCount(env->stmt);
	
	bool isFirst = true;
	for (int c = 0; c < columns; c++) {
		std::string n = env->db->cursorColumnName(env->stmt, c);
		std::string v = env->db->cursorColumnText(env->stmt, c);
		DB_FIELD_TYPE t = env->db->cursorColumnType(env->stmt, c);
		if (t == DBT_NULL) 
			 continue;
		if (isFirst)
			isFirst = false;
		else
			ss << ", ";
		ss << "\"" << n << "\": ";
		if (c == (int) CT_T) {
			ss << "[" << v << "]";
		} else {
			if (t == DBT_TEXT) 
				ss << "\"" << v << "\"";
			else
				ss << v;
		}
	}
	std::string s = ss.str();
	sz = s.size();
	memmove(buf, s.c_str(), sz > bufSize ? bufSize : sz);
	return sz;
}

static ssize_t chunk_callbackFetchDb(void *cls, uint64_t pos, char *buf, size_t max)
{
	RequestEnv *env = (RequestEnv*) cls;
	
	if (env->state.state >= 2)
		return MHD_CONTENT_READER_END_OF_STREAM;

	bool r = env->db->cursorNext(env->stmt);
	if (!r) {
		if (logCB) {
			std::stringstream ss;
			ss << "cursor next error " << r
				<< ": " << env->db->errmsg;
			logCB(env, LOG_ERR, MODULE_WS, r, ss.str());
		}
		env->state.state = env->state.state == 0 ? 3 : 2;
	}
	return result2json(buf, max, env);
}

static void chunk_done_callback(void *cls)
{
	RequestEnv *e = (RequestEnv*) cls;
	if (e != NULL)
	{
		doneFetch(e);
		free(e);
	}
}

static DatabaseIntf *findDatabaseByName(
	struct MHD_Connection *connection,
	const MAP_NAME_DATABASE &databases
)
{
	const char* c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryDbParamName);
	std::string dbname = c ? c : "";
	MAP_NAME_DATABASE::const_iterator it = databases.find(dbname);
	if (it == databases.end())
		return NULL;
	else
		return it->second;
}

static MHD_Result request_callback(
	void *cls,			// struct WSConfig*
	struct MHD_Connection *connection,
	const char *url,
	const char *method,
	const char *version,
	const char *upload_data,
	size_t *upload_data_size,
	void **ptr
)
{
	static int aptr;
	struct MHD_Response *response;
	MHD_Result ret;

	if (&aptr != *ptr) {
		// do never respond on first call
		*ptr = &aptr;
		return MHD_YES;
	}

	if (strcmp(method, "PUT") == 0)
		return MHD_NO;              // not implemenmted
	if (strcmp(method, "DELETE") == 0)
		return MHD_NO;              // not implemenmted
	// GET POST HEAD CONNECT OPTIONS TRACE PATCH
		
	*ptr = NULL;					// reset when done

	RequestEnv *requestenv = (RequestEnv *) malloc(sizeof(RequestEnv));
	requestenv->state.state = 0;
	requestenv->config = (WSConfig*) cls;
	requestenv->db = findDatabaseByName(connection, requestenv->config->databases);
	if (!requestenv->db) {
		// no database interface found
		int hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
		response = MHD_create_response_from_buffer(strlen(MSG500[1]), (void *) MSG500[1], MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response(connection, hc, response);
		MHD_destroy_response(response);
		return ret;
	}

	requestenv->request.requestType = parseRequestType(url);

	if (requestenv->request.requestType == RT_UNKNOWN) {
			std::string fn = buildFileName(requestenv->config->dirRoot, url);
			return processFile(connection, fn);
	}

	int r = (int) startFetchDb(connection, requestenv);
	int hc;
	if (r) {
		hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
		response = MHD_create_response_from_buffer(strlen(MSG500[r]), (void *) MSG500[r], MHD_RESPMEM_PERSISTENT);
	} else {
		hc = MHD_HTTP_OK;
		response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, &chunk_callbackFetchDb, requestenv, &chunk_done_callback);
	}
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CT_JSON);
	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, HDR_CORS_ORIGIN);
	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS, HDR_CORS_CREDENTIALS);
	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS, HDR_CORS_METHODS);
	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS, HDR_CORS_HEADERS);
	
	ret = MHD_queue_response(connection, hc, response);
	MHD_destroy_response(response);
	return ret;
}

bool startWS(
	WSConfig &config
) {
	if (config.flags == 0)
		config.flags = MHD_START_FLAGS;

	struct MHD_Daemon *d = MHD_start_daemon(
		config.flags, config.port, NULL, NULL, 
		&request_callback, &config,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
		MHD_OPTION_THREAD_POOL_SIZE, config.threadCount,
		MHD_OPTION_URI_LOG_CALLBACK, &uri_logger_callback, NULL,
		MHD_OPTION_CONNECTION_LIMIT, config.connectionLimit,
		MHD_OPTION_END
	);
	config.descriptor = (void *) d;
	setLogCallback(config.onLog);
	if (logCB) {
		if (config.descriptor) {
			logCB(&config, LOG_INFO, MODULE_WS, 0, "web service started");
		} else {
			std::stringstream ss;
			ss << "Start web service error " << errno
				<< ": " << strerror(errno);
			logCB(&config, LOG_ERR, MODULE_WS, errno, ss.str());
		}
	}
	return config.descriptor != NULL;
}

void doneWS(
	WSConfig &config
) {
	setLogCallback(NULL);
	if (config.descriptor)
		MHD_stop_daemon((struct MHD_Daemon *) config.descriptor);
	if (logCB) {
		logCB(&config, LOG_INFO, MODULE_WS, 0, "web service stopped");
	}
	config.descriptor = NULL;
}
