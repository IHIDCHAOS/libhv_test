#include <iostream>
#include <fstream>
#include <utility>
#include "hmain.h"
#include "requests.h"
#include "json.hpp"
#include "md5.h"

main_ctx_t  g_main_ctx;

using json = nlohmann::json;
using namespace std;
#define OTA_FILE "firmware.zip"
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

struct OTA_Context {
    HttpRequest req;
    HttpResponse res;
    http_headers header;
    Data data;
    string dev_id;

    bool checkTask();

    bool checkToken();

    bool pullFile();

    bool reportProgress(int step);

    bool reportResult(int result);

    bool postVersion();
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
//    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0) {
        cout << "checkTask error: " << j["error"].get<string>() << endl;
        return false;
    } else {
        data.target = j.at("data").at("target").get<std::string>();
        data.token = j.at("data").at("token").get<std::string>();
        data.size = j.at("data").at("size").get<int64_t>();
        data.md5 = j.at("data").at("md5").get<std::string>();
        data.signal = j.at("data").at("signal").get<int64_t>();
        data.power = j.at("data").at("power").get<int64_t>();
        data.retry = j.at("data").at("retry").get<int64_t>();
        data.interval = j.at("data").at("interval").get<int64_t>();
        data.type = j.at("data").at("type").get<int64_t>();
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
//    cout << j.dump(4)<<endl;
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

    long rangeBase = data.size / 5;
    long rangeNow = 0;
    int step = 0;

    while (rangeNow < data.size){
        long rangeEnd = (rangeNow+rangeBase) >= data.size ? data.size : (rangeNow+rangeBase);
        char rangeStr[64] = {0};
        sprintf(rangeStr,"%ld-%ld",rangeNow,rangeEnd);
        header["Range"] = rangeStr;

        req.Reset();
        req.content_type = APPLICATION_JSON;
        req.url = pullFileUrl;
        req.method = HTTP_GET;
        req.headers = header;
        http_client_send(&req, &res);

        ofstream myfile;
        myfile.open(OTA_FILE,ios_base::app);
        myfile << res.body;
        myfile.close();

        int stepNow = (int)(rangeEnd * 100 / data.size);

        if ((stepNow <= 100) && (stepNow - step >= 10)){
            if (stepNow == 100){
                FILE* fp = fopen(OTA_FILE, "rb");
                if (fp == nullptr) {
                    cout << "Open file" << OTA_FILE << "failed" << endl;
                    return false;
                }
                char md5[33];
                fseek(fp, 0, SEEK_END);
                long filesize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                auto* buf = (unsigned char*)malloc(filesize);
                size_t nread = fread(buf, 1, filesize, fp);
                assert(nread == filesize);
                hv_md5_hex(buf, filesize, md5, sizeof(md5));
                if (strcmp(md5,data.md5.c_str())==0){
                    cout << "Check MD5 Success" << endl;
                    cout << "pullFile Success" << endl;
                    return true;
                } else{
                    cout << "Check MD5 Failed" << endl;
                    pullFile();
                }
            }
            reportProgress(stepNow);
            step = stepNow;
        }
        rangeNow = rangeEnd + 1;
    }
}

bool OTA_Context::reportProgress(int step) {
    string reportStateParams = "/ota/south/device/download/%s/progress?dev_id=%s";
    char reportStateUrl[256] = {0};
    sprintf(reportStateUrl, (otaUrl + reportStateParams).c_str(), data.token.c_str(),HE_DEV_ID);
    json reportStateBody;
    reportStateBody["step"] = step;

    req.Reset();
    req.content_type = APPLICATION_JSON;

    header.erase("Range");
    req.headers = header;
    req.url = reportStateUrl;
    req.method = HTTP_POST;
    req.body = reportStateBody.dump();

    http_client_send(&req, &res);

    auto j = json::parse(res.body);
//    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0){
        cout << "reportProgress error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "reportProgress Success " << step << "%" << endl;
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
//    cout << j.dump(4)<<endl;
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
//    cout << j.dump(4)<<endl;
    if (res.status_code != HTTP_STATUS_OK || j["errno"] != 0){
        cout << "postVersion error: " << j["error"].get<string>() << endl;
        return false;
    }
    cout << "postVersion Success" << endl;
    return true;
}

int main() {
    if (access(OTA_FILE, 0) == 0)
    {
        remove(OTA_FILE);
    }
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
        sleep(5);
    }
    ota.checkToken();
    ota.pullFile();
    ota.reportResult(101);
    ota.reportResult(201);
    ota.postVersion();
    return 0;
}
