#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::launcher {struct Settings;}
namespace openshao::preferences {
void Apply(X86Context& context, uint8_t* base);
void ApplyNow(X86Context& context,const launcher::Settings& settings);
void ReadActive(launcher::Settings& settings);
}
