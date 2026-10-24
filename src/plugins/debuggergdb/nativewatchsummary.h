/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * Native semantic watch summaries over already-parsed GDBWatch trees.
 * std::string for Apple libc++ and MinGW libstdc++, with no GDB/Python dependency.
 */

#ifndef NATIVEWATCHSUMMARY_H
#define NATIVEWATCHSUMMARY_H

#include "debugger_defs.h"

/** Try to set a semantic parent value on an already-parsed watch tree.
 *  Returns true if a summary was applied; false leaves the watch unchanged (raw).
 *  Children are never removed. */
bool TryNativeWatchSummary(cb::shared_ptr<GDBWatch> watch);

#endif // NATIVEWATCHSUMMARY_H
