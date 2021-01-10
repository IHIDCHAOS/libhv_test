#include <iostream>
#include <fstream>
#include <utility>
#include "requests.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace std;
#define OTA_FILE "firmware.rar"
#define HE_PID "382381"
#define HE_DEV_ID "645330303"
#define HE_MANUF "000"
#define HE_MODEL "00001"
#define HE_TYPE "1"
#define HE_VERSION "V1"

string new_ver = "V2";

string otaUrl = "http://ota.heclouds.com";
string Auth = "version=2018-10-31&res=products/382381/devices/355750075160144"
              "&et=231817255824&method=sha1&sign=3VWVDHXSYA3ilS%2FdNvtoAUVdGOk%3D";

namespace checkTask {
    using nlohmann::json;

    inline json getUntyped(const json &j, const char *property) {
        if (j.find(property) != j.end()) {
            return j.at(property).get<json>();
        }
        return json();
    }

    inline json getUntyped(const json &j, std::string property) {
        return getUntyped(j, property.data());
    }

    struct Data {
        std::string target;
        std::string token;
        int64_t size{};
        std::string md5;
        int64_t signal{};
        int64_t power{};
        int64_t retry{};
        int64_t interval{};
        int64_t type{};
    };

    struct Body {
        int64_t errnum{};
        std::string error;
        Data data;
    };

    void from_json(const json &j, checkTask::Data &x);

    void to_json(json &j, const checkTask::Data &x);

    void from_json(const json &j, checkTask::Body &x);

    void to_json(json &j, const checkTask::Body &x);

    inline void from_json(const json &j, checkTask::Data &x) {
        x.target = j.at("target").get<std::string>();
        x.token = j.at("token").get<std::string>();
        x.size = j.at("size").get<int64_t>();
        x.md5 = j.at("md5").get<std::string>();
        x.signal = j.at("signal").get<int64_t>();
        x.power = j.at("power").get<int64_t>();
        x.retry = j.at("retry").get<int64_t>();
        x.interval = j.at("interval").get<int64_t>();
        x.type = j.at("type").get<int64_t>();
    }

    inline void to_json(json &j, const checkTask::Data &x) {
        j = json::object();
        j["target"] = x.target;
        j["token"] = x.token;
        j["size"] = x.size;
        j["md5"] = x.md5;
        j["signal"] = x.signal;
        j["power"] = x.power;
        j["retry"] = x.retry;
        j["interval"] = x.interval;
        j["type"] = x.type;
    }

    inline void from_json(const json &j, checkTask::Body &x) {
        x.errnum = j.at("errno").get<int64_t>();
        x.error = j.at("error").get<std::string>();
        x.data = j.at("data").get<checkTask::Data>();
    }

    inline void to_json(json &j, const checkTask::Body &x) {
        j = json::object();
        j["errno"] = x.errnum;
        j["error"] = x.error;
        j["data"] = x.data;
    }
}

using namespace checkTask;

struct OTA_Context {
    HttpRequest req;
    HttpResponse res;
    http_headers header;
    Data data;
    string dev_id;

    bool checkTask();

    bool checkToken();

    bool pullFile();

    bool reportResult(int result);

    bool postVersion();

    bool queryInfo();
};

bool OTA_Context::checkTask() {
    string checkTaskParams = "/ota/south/check?dev_id=%s&manuf=%s&model=%s&type=%s&version=%s";
    char checkTaskUrl[256] = {0};
    sprintf(checkTaskUrl, (otaUrl + checkTaskParams).c_str(), HE_DEV_ID, HE_MANUF, HE_MODEL, HE_TYPE, HE_VERSION);

    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = checkTaskUrl;
    req.method = HTTP_GET;

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0) {
        cout << "checkTask error: " << j["error"].get<string>() << endl;
        return false;
    } else {
        from_json(j["data"], data);
    }
    cout << "checkTask Success" << endl;
    return true;
}

bool OTA_Context::checkToken() {
    string checkTokenParams = "/ota/south/download/%s/check?dev_id=%s";
    char checkTokenUrl[256] = {0};
    sprintf(checkTokenUrl, (otaUrl + checkTokenParams).c_str(), data.token.c_str(), HE_DEV_ID);

    req.Reset();
    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = checkTokenUrl;
    req.method = HTTP_GET;

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0) {
        cout << "checkToken error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "checkToken Success" << endl;
    return true;
}

bool OTA_Context::pullFile() {
    string pullFileParams = "/ota/south/download/%s";
    char pullFileUrl[256] = {0};
    sprintf(pullFileUrl, (otaUrl + pullFileParams).c_str(), data.token.c_str());

    req.Reset();
    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = pullFileUrl;
    req.method = HTTP_GET;
    http_client_send(&req, &res);
    ofstream myfile;
    myfile.open(OTA_FILE);
    myfile << res.body;
    myfile.close();

    if (res.status_code != HTTP_STATUS_OK) {
        cout << "pullFile error: " << endl;
        return false;
    }
    cout << "pullFile Success" << endl;
    return true;
}

bool OTA_Context::reportResult(int result) {
    string reportStateParams = "/ota/south/report?dev_id=%s&token=%s";
    char reportStateUrl[256] = {0};
    sprintf(reportStateUrl, (otaUrl + reportStateParams).c_str(), HE_DEV_ID, data.token.c_str());
    json reportStateBody;
    reportStateBody["result"] = result;

    req.Reset();
    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = reportStateUrl;
    req.method = HTTP_POST;
    req.body = reportStateBody.dump();

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0){
        cout << "reportState error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "reportState Success" << endl;
    return true;
}

bool OTA_Context::postVersion() {
    string postVersionParams = "/ota/device/version?dev_id=%s";
    char postVersionUrl[256] = {0};
    sprintf(postVersionUrl, (otaUrl + postVersionParams).c_str(), HE_DEV_ID);
    json postVersionBody;
    postVersionBody["f_version"] = new_ver;
    postVersionBody["s_version"] = "s_v1";

    req.Reset();
    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = postVersionUrl;
    req.method = HTTP_POST;
    req.body = postVersionBody.dump();

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0){
        cout << "postVersion error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "postVersion Success" << endl;
    return true;
}

bool OTA_Context::queryInfo() {
    string queryInfoParams = "/ota/devInfo";
    char queryInfoUrl[256] = {0};
    sprintf(queryInfoUrl, (otaUrl + queryInfoParams).c_str(), HE_DEV_ID);
    json queryInfoBody;
    queryInfoBody["pid"] = HE_PID;
    queryInfoBody["authInfo"] = data.token;// 这里不知道参数是什么

    req.Reset();
    req.content_type = APPLICATION_JSON;
    req.headers = header;
    req.url = queryInfoUrl;
    req.method = HTTP_GET;

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0){
        cout << "queryInfo error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "queryInfo Success" << endl;
    return true;
}

static void test_http_sync_client() {
    HttpRequest req;
    HttpResponse res;
    http_headers header;
    header["Authorization"] = Auth;

    req.method = HTTP_GET;
    req.content_type = APPLICATION_JSON;
    req.headers = header;

    //checkTask
    string checkTaskParams = "/ota/south/check?dev_id=%s&manuf=%s&model=%s&type=%s&version=%s";
    char checkTaskUrl[256] = {0};
    sprintf(checkTaskUrl, (otaUrl + checkTaskParams).c_str(), HE_DEV_ID, HE_MANUF, HE_MODEL, HE_TYPE, HE_VERSION);
    req.url = checkTaskUrl;
    req.method = HTTP_GET;

    json j;
    while (true) {
        http_client_send(&req, &res);
        printf("%d %s\n", res.status_code, res.status_message());

        j = json::parse(res.body);
        int res_errno = j["errno"];
        if (res_errno) {
            string res_error = j["error"];
            cout << "checkTask error: " << res_error << endl;
            sleep(3);
            continue;
        } else
            break;
    }
    string res_target = j["data"]["target"];
    string res_token = j["data"]["token"];
    int res_size = j["data"]["size"];
    string res_md5 = j["data"]["md5"];
    int res_signal = j["data"]["signal"];
    int res_power = j["data"]["power"];
    int res_retry = j["data"]["retry"];
    int res_interval = j["data"]["interval"];
    int res_type = j["data"]["type"];

    //checkToken
    string checkTokenParams = "/ota/south/download/%s/check?dev_id=%s";
    char checkTokenUrl[256] = {0};
    sprintf(checkTokenUrl, (otaUrl + checkTokenParams).c_str(), res_token.c_str(), HE_DEV_ID);
    req.url = checkTokenUrl;
    req.method = HTTP_GET;
    http_client_send(&req, &res);

//    j = json::parse(res.body);
//    printf("%d %s\n", res.status_code, res.status_message());
//    cout << j.dump(4)<<endl;

    //pullFile
    string pullFileParams = "/ota/south/download/%s";
    char pullFileUrl[256] = {0};
    sprintf(pullFileUrl, (otaUrl + pullFileParams).c_str(), res_token.c_str());
    req.url = pullFileUrl;
    req.method = HTTP_GET;
    http_client_send(&req, &res);
    ofstream myfile;
    myfile.open(OTA_FILE);
    myfile << res.body;
    myfile.close();

    //reportState
    string reportStateParams = "/ota/south/report?dev_id=%s&token=%s";
    char reportStateUrl[256] = {0};
    sprintf(reportStateUrl, (otaUrl + reportStateParams).c_str(), HE_DEV_ID, res_token.c_str());
    json reportStateBody;
    reportStateBody["state"] = 101;

    req.url = reportStateUrl;
    req.method = HTTP_POST;
    req.body = reportStateBody.dump();
    http_client_send(&req, &res);

    reportStateBody["state"] = 201;
    req.url = reportStateUrl;
    req.method = HTTP_POST;
    req.body = reportStateBody.dump();
    http_client_send(&req, &res);

    //postVersion
    string postVersionParams = "/ota/device/version?dev_id=%s";
    char postVersionUrl[256] = {0};
    sprintf(postVersionUrl, (otaUrl + postVersionParams).c_str(), HE_DEV_ID);
    json postVersionBody;
    postVersionBody["f_version"] = "f_v2";
    postVersionBody["s_version"] = "s_v1";

    req.url = postVersionUrl;
    req.method = HTTP_POST;
    http_client_send(&req, &res);

    //queryInfo
    string queryInfoParams = "/ota/devInfo";
    char queryInfoUrl[256] = {0};
    sprintf(queryInfoUrl, (otaUrl + queryInfoParams).c_str(), HE_DEV_ID);
    json queryInfoBody;
    queryInfoBody["pid"] = HE_PID;
    queryInfoBody["authInfo"] = "";// 这里不知道参数是什么

    req.url = postVersionUrl;
    req.method = HTTP_GET;
    http_client_send(&req, &res);
}

int main() {
    OTA_Context ota;
    http_headers header;
    header["Authorization"] = Auth;
    ota = {
            .req = HttpRequest(),
            .res = HttpResponse(),
            .header = header,
    };
    bool haveCheckTask = false;
    while (!haveCheckTask) {
        haveCheckTask = ota.checkTask();
        sleep(3);
    }
    ota.checkToken();
    ota.pullFile();
    ota.reportResult(101);
    ota.reportResult(201);
    ota.postVersion();
    ota.queryInfo();
    return 0;
}