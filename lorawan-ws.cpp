#include <cstdlib>
#include <cstdbool>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <map>

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

#ifdef ENABLE_JWT
#include "auth-jwt.h"
#endif

#include "log-intf.h"

static LogIntf *logCB = nullptr;

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

static const char *paths[PATH_COUNT] = {
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
	CT_TP = 9,			//> Calculated temperature
	CT_RAW = 10,			//> raw packets in hex
	CT_DEVNAME = 11,	//> 
	CT_LORAADDR = 12,	//> 
	CT_RECEIVED = 13	//> 
} ColumnTemperature;

const char *pathSelectPrefix[PATH_COUNT] = {
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, tp, raw, devname, loraaddr, received FROM logger_lora",
	"SELECT count(id) cnt FROM logger_raw",
	"SELECT count(id) cnt FROM logger_lora",
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw WHERE id = ?1",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, tp, raw, devname, loraaddr, received FROM logger_lora WHERE id = ?1"
};

const char *pathDeletePrefix[2] = {
    "DELETE FROM logger_raw",
    "DELETE FROM logger_lora"
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

const static char *CE_GZIP = "gzip";
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
const static char *HDR_CORS_METHODS = "GET,HEAD,OPTIONS,POST,PUT,DELETE";
const static char *HDR_CORS_HEADERS = "Authorization, Access-Control-Allow-Headers, "
    "Origin, Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers";

typedef enum {
	START_FETCH_DB_OK = 0,
	START_FETCH_DB_CONNECT_FAILED = 1,
	START_FETCH_DB_PREPARE_FAILED = 2,
	START_FETCH_DB_NO_PARAM = 3,
	START_FETCH_DB_BIND_PARAM = 4
} START_FETCH_DB_RESULT;

const static char *MSG_HTTP_ERROR = "Error";
const static char *MSG404 = "404 not found";
const static char *MSG401 = "Unauthorized";
const static char *MSG_DELETE_OK = "{\"result\": 0}";

const static char *MSG500[5] = {
	"",                                     // 0
	"Database connection not established",  // 1
	"SQL statement preparation failed",     // 2
	"Required parameter missed",            // 3
	"Binding parameter failed"              // 4
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

void *uri_logger_callback(void *cls, const char *uri)
{
	if (logCB) {
		std::stringstream ss;
		ss << "URI: " << uri;
		logCB->logMessage(cls, LOG_INFO, MODULE_WS, 0, ss.str());
	}
	return nullptr;
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
			logCB->logMessage(env, LOG_ERR, MODULE_WS, r, "Cursor close error " + env->db->errmsg);
		return START_FETCH_DB_PREPARE_FAILED;
	}
	env->stmt = nullptr;
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

static const char *mimeTypeByFileExtension(const std::string &filename)
{
	std::string ext = filename.substr(filename.find_last_of('.') + 1);
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

static MHD_Result processFile(
    struct MHD_Connection *connection,
    const std::string &filename
)
{
	struct MHD_Response *response;
	MHD_Result ret;
	FILE *file;
	struct stat buf;

	const char *localFileName = filename.c_str();
    bool gzipped = false;
	if (stat(localFileName, &buf) == 0)
		file = fopen(localFileName, "rb");
	else {
        std::string fnGzip(filename);
        fnGzip += ".gz";
        localFileName = fnGzip.c_str();
        if (stat(localFileName, &buf) == 0) {
            file = fopen(localFileName, "rb");
            gzipped = true;
        } else
            file = nullptr;
    }
	if (file == nullptr) {
		if (logCB)
			logCB->logMessage(connection, LOG_ERR, MODULE_WS, 404, "File not found " + filename);

		response = MHD_create_response_from_buffer(strlen(MSG404), (void *) MSG404, MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
		MHD_destroy_response (response);
	} else {
		response = MHD_create_response_from_callback(buf.st_size, 32 * 1024,
			&file_reader_callback, file, &free_file_reader_callback);
		if (nullptr == response) {
			fclose (file);
			return MHD_NO;
		}

		MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mimeTypeByFileExtension(filename));
        if (gzipped)
            MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, CE_GZIP);
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
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
		size_t l = strlen(url);
		if (l && (url[l - 1] == '/'))
			r << "index.html";
	}
	return r.str();
}

/**
 * Add WHERE clause if any query parameter found
 * @param retval return WHERE clause
 * @param connection MHD connection to read query paramaters
 * @return true- at least one parameter substituted
 */
static bool buildWhereParameters(
        std::stringstream &retval,
        MHD_Connection *connection
) {
    bool isFirst = true;
    for (int i = QUERY_PARAMS_OPTIONAL_MIN; i <= QUERY_PARAMS_OPTIONAL_MAX; i++) {
        const char *pn = queryParamNames[i];
        const char* c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, pn);
        if (c) {
            // equals
            if (isFirst) {
                retval << " WHERE ";
                isFirst = false;
            } else {
                retval << " AND ";
            }
            retval << pn << " = ";
            if (queryParamIsString[i])
                retval << SQL_STRING_QUOTE << c << SQL_STRING_QUOTE;
            else
                retval << c;
            // do not check other conditions
            continue;
        }
        for (int pns = 0; pns < QUERY_PARAMS_SUFFIX_SIZE; pns++) {
            std::string pnc = std::string(pn) + queryParamSuffix[pns];
            const char* cc = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, pnc.c_str());
            if (cc) {
                if (isFirst) {
                    retval << " WHERE ";
                    isFirst = false;
                } else
                    retval << " AND ";
                retval << pn << " " << queryParamSQLComparisonOperator[pns] << " ";
                if (queryParamIsString[i])
                    retval << SQL_STRING_QUOTE << cc << SQL_STRING_QUOTE;
                else
                    retval << cc;
                // Do not break, check other conditions
                // break;
            }
        }
    }
    return !isFirst;
}

static START_FETCH_DB_RESULT startFetchDb(
	struct MHD_Connection *connection,
	RequestEnv *env
)
{
	if (logCB) {
		logCB->logMessage(env, LOG_INFO, MODULE_WS, 0, "Fetch");
	}

	std::stringstream pathSelectSS;
	pathSelectSS << pathSelectPrefix[env->request.requestType];
	// get by identifier- no optional conditions
	if ((env->request.requestType == RT_RAW_1) || (env->request.requestType == RT_T_1)) {
        const char* c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryParamNames[2]);
        if (!c)
            return START_FETCH_DB_NO_PARAM;
        pathSelectSS << " WHERE " << queryParamNames[2] << " = " << SQL_STRING_QUOTE << c << SQL_STRING_QUOTE;
    } else {
        buildWhereParameters(pathSelectSS, connection);
        // finish WHERE clause
        pathSelectSS << " " << pathSelectSuffix[env->request.requestType];
    }

    std::string pathSelect = pathSelectSS.str();

	if (logCB) {
		std::stringstream ss;
		ss << "SQL " << pathSelect;
		logCB->logMessage(env, LOG_INFO, MODULE_WS, 0, ss.str());
	}

	// preparation
	int r = env->db->cursorOpen(&env->stmt, pathSelect);
	if (r) {
		if (logCB) {
			std::stringstream ss;
			ss << "Fetch error " << r 
				<< ": " << env->db->errmsg
				<< " db " << env->db->type
				<< " SQL " << pathSelect.c_str();
			logCB->logMessage(env, LOG_ERR, MODULE_WS, r, ss.str());
		}
		return START_FETCH_DB_PREPARE_FAILED;
    } else {
		if (logCB) {
			logCB->logMessage(env, LOG_INFO, MODULE_WS, 0, "cursor opened");
		}
	}

    if (!((env->request.requestType == RT_RAW_1) || (env->request.requestType == RT_T_1))) {
        // bind required params if no id
        for (int i = 0; i < QUERY_PARAMS_REQUIRED_MAX; i++) {
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
                    logCB->logMessage(env, LOG_ERR, MODULE_WS, r, ss.str());
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
                        logCB->logMessage(env, LOG_ERR, MODULE_WS, r, ss.str());
                    }
                    env->db->cursorClose(env->stmt);
                    return START_FETCH_DB_BIND_PARAM;
                }
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
		switch (c) {
			case CT_T:
			case CT_TP:
				ss << "[" << v << "]";
				break;
			default:
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
		env->state.state = env->state.state == 0 ? 3 : 2;
	}
	return result2json(buf, max, env);
}

static void chunk_done_callback(void *cls)
{
	RequestEnv *e = (RequestEnv*) cls;
	if (e != nullptr)
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
		return nullptr;
	else
		return it->second;
}

static void addCORS(MHD_Response *response) {
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, HDR_CORS_ORIGIN);
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS, HDR_CORS_CREDENTIALS);
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS, HDR_CORS_METHODS);
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS, HDR_CORS_HEADERS);
}

static MHD_Result putStringVector(
    void *retVal,
    enum MHD_ValueKind kind,
    const char *key,
    const char *value
)
{
    std::map<std::string, std::string> *r = (std::map<std::string, std::string> *) retVal;
    r->insert(std::pair<std::string, std::string>(key, value));
    return MHD_YES;
}

static MHD_Result httpError(
    struct MHD_Connection *connection,
    int code
)
{
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(MSG_HTTP_ERROR), (void *) MSG_HTTP_ERROR, MHD_RESPMEM_PERSISTENT);
    addCORS(response);
    MHD_Result r = MHD_queue_response(connection, code, response);
    MHD_destroy_response(response);
    return r;
}

static MHD_Result httpError401(
    struct MHD_Connection *connection
)
{
    int hc = MHD_HTTP_UNAUTHORIZED;
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(MSG401), (void *) MSG401, MHD_RESPMEM_PERSISTENT);
    std::string hwa = "Bearer error=\"invalid_token\"";
    MHD_add_response_header(response, MHD_HTTP_HEADER_WWW_AUTHENTICATE, hwa.c_str());
    addCORS(response);
    MHD_Result r = MHD_queue_response(connection, hc, response);
    MHD_destroy_response(response);
    return r;
}

/**
 * Delete records
 * @param connection MHD connection
 * @param env MHD request
 * @return 0- OK, 1- Database connection not established, 2- SQL statement preparation failed, 3- Required parameter missed, 4- Binding parameter failed
 */
static START_FETCH_DB_RESULT deleteFromDb(
    MHD_Connection *connection,
    RequestEnv *env
) {
    if ((env->request.requestType > RT_T_1) || (env->request.requestType ==	RT_RAW_COUNT)
        || (env->request.requestType == RT_T_COUNT))
        return START_FETCH_DB_NO_PARAM;
    std::stringstream clauseDeleteSS;
    clauseDeleteSS << pathDeletePrefix[env->request.requestType & 1];
    if ((env->request.requestType == RT_RAW_1) || (env->request.requestType == RT_T_1)) {
        // delete by identifier
        const char* c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryParamNames[2]);
        clauseDeleteSS << " WHERE " << queryParamNames[2] << " = " << SQL_STRING_QUOTE << c << SQL_STRING_QUOTE;
    } else {
        // delete by query parameters
        if (!buildWhereParameters(clauseDeleteSS, connection))
            return START_FETCH_DB_NO_PARAM; // prevent delete any records without parameters
    }

    std::string pathDelete = clauseDeleteSS.str();


    int r = env->db->exec(pathDelete);

    if (r) {
        if (logCB) {
            std::stringstream ss;
            ss << "Delete error " << r
               << ": " << env->db->errmsg
               << " db " << env->db->type
               << " SQL " << pathDelete.c_str();
            logCB->logMessage(env, LOG_ERR, MODULE_WS, r, ss.str());
        }
        return START_FETCH_DB_PREPARE_FAILED;
    }
    return START_FETCH_DB_OK;
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

    if (strcmp(method, "OPTIONS") == 0) {
        response = MHD_create_response_from_buffer(strlen(MSG500[0]), (void *) MSG500[0], MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CT_JSON);
        addCORS(response);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return MHD_YES;
    }
    *ptr = nullptr;					// reset when done

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

    bool authorized;

    // Verify JWT token by "Authorization: Bearer ..." header
    std::string jwt;
    const char *bearer = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_AUTHORIZATION);
    if (bearer) {
        jwt = bearer;
        size_t p = jwt.find("Bearer");
        if (p != std::string::npos) {
            jwt = jwt.substr(p + 6);
            // left trim
            jwt.erase(jwt.begin(), std::find_if(jwt.begin(), jwt.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
        }
    }
	if (logCB) {
		std::stringstream ss;
		ss << "Authorization : Bearer " << jwt;
		logCB->logMessage(requestenv->config, LOG_INFO, MODULE_WS, 0, ss.str());
	}

#ifdef ENABLE_JWT
	authorized = false;
    AuthJWT *aj = (AuthJWT *) ((WSConfig*) cls)->jwt;
    if (aj) {
        if (!aj->issuer.empty()) {
            authorized = aj->verify(jwt);
        }
    }
	if (logCB) {
		std::stringstream ss;
		ss << (authorized ? "" : "not ") << "authorized";
		logCB->logMessage(requestenv->config, LOG_INFO, MODULE_WS, 0, ss.str());
	}
#else
	authorized = true;
#endif
    if (requestenv->request.requestType == RT_UNKNOWN) {
        // if JSON service not found, try load from the host handler callback, then from the file
        MHD_Result fr;
        if (requestenv->config->onSpecialPathHandler) {
            std::string content;
            std::string ct;
            struct MHD_Response *specResponse;

            std::map<std::string, std::string> q;
            MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, putStringVector, &q);
            int responseCode = requestenv->config->onSpecialPathHandler->handle(content, ct, requestenv->config,
                MODULE_WS, url, method, version, q, upload_data, upload_data_size, authorized);

			if (logCB) {
				std::stringstream ss;
				ss << "Unknown response " << responseCode << " on special path: " << url << ", content: "<< ct.substr(0, 30) << ".., size: " << ct.size();
				logCB->logMessage(requestenv->config, LOG_INFO, MODULE_WS, 0, ss.str());
			}

            switch (responseCode) {
                case 200: {
                    if (ct.empty())
                        ct = CT_JSON;

                    specResponse = MHD_create_response_from_buffer(content.size(), (void *) content.c_str(),
                        MHD_RESPMEM_MUST_COPY);
                    addCORS(specResponse);
                    MHD_add_response_header(specResponse, MHD_HTTP_HEADER_CONTENT_TYPE, ct.c_str());
                    fr = MHD_queue_response(connection, MHD_HTTP_OK, specResponse);
                    MHD_destroy_response(specResponse);
                    return fr;
                case 401:
                    return httpError401(connection);
                case 404:
                    // try load from the file system
                    return processFile(connection, buildFileName(requestenv->config->dirRoot, url));
                default:
                    return httpError(connection, responseCode);
                }
            }
        }
	}
    if (!authorized)
        return httpError401(connection);

    int hc;
    if (strcmp(method, "DELETE") == 0) {
        // DELETE
        int r = (int) deleteFromDb(connection, requestenv);
        if (r) {
            if (logCB) {
                std::stringstream ss;
                ss << "DELETE error " << r;
                logCB->logMessage(requestenv->config, LOG_ERR, MODULE_WS, 0, ss.str());
            }
            hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
            response = MHD_create_response_from_buffer(strlen(MSG500[r]), (void *) MSG500[r], MHD_RESPMEM_PERSISTENT);
        } else {
            hc = MHD_HTTP_OK;
            response = MHD_create_response_from_buffer(strlen(MSG_DELETE_OK), (void *) MSG_DELETE_OK, MHD_RESPMEM_PERSISTENT);
        }
    } else {
		if (logCB) {
			std::stringstream ss;
			ss << "Select ";
			logCB->logMessage(requestenv->config, LOG_INFO, MODULE_WS, 0, ss.str());
		}

        // SELECT
        int r = (int) startFetchDb(connection, requestenv);
        if (r) {
            hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
            response = MHD_create_response_from_buffer(strlen(MSG500[r]), (void *) MSG500[r], MHD_RESPMEM_PERSISTENT);
        } else {
            hc = MHD_HTTP_OK;
            response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, &chunk_callbackFetchDb, requestenv,
                &chunk_done_callback);
        }
    }
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CT_JSON);
    addCORS(response);
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
		config.flags, config.port, nullptr, nullptr,
		&request_callback, &config,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,  // 2 minutes timout
		MHD_OPTION_THREAD_POOL_SIZE, config.threadCount,
		MHD_OPTION_URI_LOG_CALLBACK, &uri_logger_callback, nullptr,
		MHD_OPTION_CONNECTION_LIMIT, config.connectionLimit,
		MHD_OPTION_END
	);
	config.descriptor = (void *) d;
	logCB = config.onLog;
	if (logCB) {
		if (!config.descriptor) {
			std::stringstream ss;
			ss << "Start web service error " << errno
				<< ": " << strerror(errno);
			logCB->logMessage(&config, LOG_ERR, MODULE_WS, errno, ss.str());
		}
	}
#ifdef ENABLE_JWT
    config.jwt = new AuthJWT(config.issuer, config.secret);
#else
    config.jwt = nullptr;
#endif
	return config.descriptor != nullptr;
}

void doneWS(
	WSConfig &config
) {
	logCB = nullptr;
	if (config.descriptor)
		MHD_stop_daemon((struct MHD_Daemon *) config.descriptor);
	if (logCB) {
		logCB->logMessage(&config, LOG_INFO, MODULE_WS, 0, "web service stopped");
	}
	config.descriptor = nullptr;
#ifdef ENABLE_JWT
    if (config.jwt)
        delete (AuthJWT*) config.jwt;
#endif
}
