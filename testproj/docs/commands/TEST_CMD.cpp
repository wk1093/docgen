#include <string>
#include<vector>
#include <iostream>
#include <cstdint>
std::string TEST_CMD(const std::string &code, const std::vector<std::string> &args) 
{
    std::cout << "test command running" << std::endl;
    for (const auto& arg : args) {
        std::cout << arg << std::endl;
    }
    return "test command result";
}