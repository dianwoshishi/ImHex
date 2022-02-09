
#include <iostream>
#include <memory>
#include <mutex>

#include <selene.h>
#include <cassert>

#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>


// curl -R -O http://www│
// .lua.org/ftp/lua-5.3.0.tar.gz
// tar zxf lua-5.3.0.tar.gz
// cd lua-5.3.0
// make macosx test
// make install

class LuaConfig {

public:

    static std::shared_ptr<LuaConfig> getLuaConfig();

    ~LuaConfig() { }
    template<class T>
    const T get_key_value(const char *dict, const char* key) {
        try
        {
            return static_cast<T>(state[dict][key]);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            hex::log::fatal(std::string(e.what()));
            exit(EXIT_FAILURE);
        }
        
    }
private:

    LuaConfig() {
        std::string path = hex::getExecutablePath() ;
        path += std::string("/config.lua");
        try
        {
            state.Load(path);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            hex::log::fatal(std::string(e.what()));
            exit(EXIT_FAILURE);
        }
        
        
    }
    sel::State state;
};