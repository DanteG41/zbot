#include <map>
#include <string>

class ZConfig {
    public:
        const char* configFile;
        ZConfig(const char* c) { configFile = c; };
        ZConfig() { configFile = "./test.ini"; };

        void load();
        void getParam(const char* name, int&);
        void getParam(const char* name, std::string&);
    private:
       std::map <std::string, std::string>::iterator paramsIter_;
       std::map <std::string, std::string> params_ = {
           {"storage", "/var/spool/zbot"},
           {"log", "/var/log/zbot.log"},
           {"wait", "10"}
       };
};

class ZConfigException {
    private:
        std::string m_error;

    public:
        ZConfigException(std::string error) : m_error(error) {}
        const char* getError() { return m_error.c_str(); }
};