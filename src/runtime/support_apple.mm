#include "support.hpp"

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <Foundation/Foundation.h>

namespace dino::runtime {
    void dispatch_on_ui_thread(std::function<void()> func) {
        dispatch_async(dispatch_get_main_queue(), ^{
            func();
        });
    }

    std::optional<std::filesystem::path> get_application_support_directory() {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if ([paths count] > 0) {
            NSString *path = [paths objectAtIndex:0];
            return std::filesystem::path([path UTF8String]);
        }
        return std::nullopt;
    }

    std::filesystem::path get_bundle_resource_directory() {
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *resourcePath = [bundle resourcePath];
        return std::filesystem::path([resourcePath UTF8String]);
    }

    std::filesystem::path get_bundle_directory() {
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *bundlePath = [bundle bundlePath];
        return std::filesystem::path([bundlePath UTF8String]);
    }
}
#endif