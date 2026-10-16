/* Internal hooks between ProgramConsolePanel (app) and SDK transport. macOS only. */
#ifndef PROGRAMCONSOLE_INTERNAL_H
#define PROGRAMCONSOLE_INTERNAL_H

#include <wx/string.h>
#include <cstdint>
#include <cstddef>

#if defined(__WXMAC__)
namespace ProgramConsole
{
void BindPanelHooks(void (*clear)(),
                    void (*append)(std::uint64_t, const wxString&),
                    void (*show)(),
                    void (*setGen)(std::uint64_t));
bool SendPanelInput(const void* data, size_t len);
bool ResizeSession(unsigned rows, unsigned cols);
unsigned char GetVIntrByte();
unsigned char GetVEofByte();
}
#endif

#endif
