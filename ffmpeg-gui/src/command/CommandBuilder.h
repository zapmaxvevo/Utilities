#pragma once
#include <string>
#include <vector>
#include "core/FFmpegJob.h"

// Separación de responsabilidades clave:
//   BuildArguments()    → vector<string> listo para pasar a execvp / CreateProcess
//   BuildCommandString() → string legible para mostrar al usuario
//
// Ninguna de estas funciones ejecuta nada ni habla con wx.
class CommandBuilder
{
public:
    static std::vector<std::string> BuildArguments   (const FFmpegJob& job);
    static std::string              BuildCommandString(const FFmpegJob& job);
};
