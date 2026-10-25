#pragma once
#include "routines.h"
namespace openshao::mods {
void StageRoutines(std::vector<RoutineScript> scripts);
bool StartRoutines(uint8_t* memory,std::string& error);
}
