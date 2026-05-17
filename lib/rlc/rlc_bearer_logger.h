#ifndef SRS_RLC_BEARER_LOGGER_H
#define SRS_RLC_BEARER_LOGGER_H

#include <srsran/support/format/prefixed_logger.h>

namespace srsran {

class rlc_bearer_logger {
public:
    rlc_bearer_logger(const std::string& name) : logger(name, "") {}
    
    template <typename Prefix, typename... Args>
    rlc_bearer_logger(const std::string& name, Prefix prefix, const char* sep = "") 
        : logger(name, prefix, sep) {}

    template <typename... Args>
    void log_info(const char* fmt, Args&&... args) {
        logger.log_info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void log_debug(const char* fmt, Args&&... args) {
        logger.log_debug(fmt, std::forward<Args>(args)...);
    }

    template <typename It, typename... Args>
    void log_info(It it_begin, It it_end, const char* fmt, Args&&... args) {
        logger.log_info(it_begin, it_end, fmt, std::forward<Args>(args)...);
    }

    void log(const std::string& msg) {
        logger.log(srslog::basic_levels::info, msg.c_str());
    }

private:
    prefixed_logger<std::string> logger;
};

} // namespace srsran

#endif // SRS_RLC_BEARER_LOGGER_H
