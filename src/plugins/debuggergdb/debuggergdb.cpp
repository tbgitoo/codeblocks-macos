/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 14024 $
 * $Id: debuggergdb.cpp 14024 2026-09-15 16:10:18Z wh11204 $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/debuggergdb/debuggergdb.cpp $
 */

#include "sdk.h"

#include <algorithm> // std::remove_if

#ifndef CB_PRECOMP
    #include <wx/app.h>
    #include <wx/txtstrm.h>
    #include <wx/regex.h>
    #include <wx/msgdlg.h>
    #include <wx/frame.h> // GetMenuBar
    #include <wx/menu.h>
    #include <wx/filedlg.h>

    #include "cbproject.h"
    #include "manager.h"
    #include "configmanager.h"
    #include "logmanager.h" // for F
    #include "projectmanager.h"
    #include "pluginmanager.h"
    #include "editormanager.h"
    #include "macrosmanager.h"
    #include "cbeditor.h"
    #include "projectbuildtarget.h"
    #include "sdk_events.h"
    #include "compilerfactory.h"
    #include "xtra_res.h"

    #include "scrollingdialog.h"
    #include "globals.h"
#endif

#include <wx/tokenzr.h>
#include "editarraystringdlg.h"
#include "annoyingdialog.h"
#include "cbstyledtextctrl.h"
#include "compilercommandgenerator.h"

#include "cbdebugger_interfaces.h"
#include "editbreakpointdlg.h"

#include "databreakpointdlg.h"
#include "debuggeedescription.h"
#include "programconsole.h"
#include "debuggerdriver.h"
#include "debuggergdb.h"
#if defined(__WXMAC__)
#include "gdbmacprovision.h"
#endif
#include "debuggeroptionsdlg.h"
#include "debuggeroptionsprjdlg.h"
#include "editwatchdlg.h"

// function pointer to DebugBreakProcess under windows (XP+)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501)
#include "Tlhelp32.h"
typedef BOOL WINAPI   (*DebugBreakProcessApiCall)       (HANDLE);
typedef HANDLE WINAPI (*CreateToolhelp32SnapshotApiCall)(DWORD  dwFlags,   DWORD             th32ProcessID);
typedef BOOL WINAPI   (*Process32FirstApiCall)          (HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
typedef BOOL WINAPI   (*Process32NextApiCall)           (HANDLE hSnapshot, LPPROCESSENTRY32W lppe);

DebugBreakProcessApiCall        DebugBreakProcessFunc        = nullptr;
CreateToolhelp32SnapshotApiCall CreateToolhelp32SnapshotFunc = nullptr;
Process32FirstApiCall           Process32FirstFunc           = nullptr;
Process32NextApiCall            Process32NextFunc            = nullptr;

HINSTANCE kernelLib = nullptr;
#endif

#ifdef __WXMSW__
// disable the CTRL_C event
inline BOOL WINAPI HandlerRoutine(cb_unused DWORD dwCtrlType)
{
    return TRUE;
}
#else
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#endif

#if !defined(__WXMSW__)
namespace
{
// Probe companion Darwin GDB for "inferior-stdio-unbuffered", optionally set
// it on, then Start/Attach.  Queued after tty so set precedes "run"/"start".
class GdbCmd_SetupMacOSUnbufferStdio : public DebuggerCmd
{
    public:
        GdbCmd_SetupMacOSUnbufferStdio(DebuggerDriver* driver,
                                       bool breakOnEntry,
                                       int pidToAttach)
            : DebuggerCmd(driver, "show inferior-stdio-unbuffered", true),
              m_breakOnEntry(breakOnEntry),
              m_pidToAttach(pidToAttach)
        {
        }

        void ParseOutput(const wxString& output) override
        {
            const bool unsupported = output.Contains("Undefined")
                                  || output.Contains("No symbol")
                                  || output.Contains("Ambiguous");
            const bool supported = !unsupported
                && (output.Contains("on") || output.Contains("off"));

            if (supported)
            {
                m_pDriver->QueueCommand(new DebuggerCmd(m_pDriver,
                    "set inferior-stdio-unbuffered on", true));
                m_pDriver->DebugLog(_("GDB supports inferior-stdio-unbuffered; "
                    "enabling unbuffered stdout for this Debug session"));
            }
            else
            {
                m_pDriver->DebugLog(_("GDB lacks inferior-stdio-unbuffered; "
                    "leaving line-buffered stdout"));
            }

            if (m_pidToAttach == 0)
                m_pDriver->Start(m_breakOnEntry);
            else
                m_pDriver->Attach(m_pidToAttach);
        }

    private:
        bool m_breakOnEntry;
        int m_pidToAttach;
};

// Probe Darwin GDB for "inferior-tty-claim", optionally disable claiming, then
// attach the Terminal console tty and start/attach (or chain unbuffer probe).
// Must run before Start() is queued so set/tty precede "run".
class GdbCmd_SetupMacOSInferiorTty : public DebuggerCmd
{
    public:
        GdbCmd_SetupMacOSInferiorTty(DebuggerDriver* driver,
                                     const wxString& ttyPath,
                                     bool breakOnEntry,
                                     int pidToAttach,
                                     bool unbufferStdio)
            : DebuggerCmd(driver, "show inferior-tty-claim", true),
              m_ttyPath(ttyPath),
              m_breakOnEntry(breakOnEntry),
              m_pidToAttach(pidToAttach),
              m_unbufferStdio(unbufferStdio)
        {
        }

        void ParseOutput(const wxString& output) override
        {
            const bool unsupported = output.Contains("Undefined")
                                  || output.Contains("No symbol")
                                  || output.Contains("Ambiguous");
            const bool supported = !unsupported
                && (output.Contains("on") || output.Contains("off"));

            if (supported)
            {
                m_pDriver->QueueCommand(new DebuggerCmd(m_pDriver,
                    "set inferior-tty-claim off", true));
                m_pDriver->DebugLog(_("GDB supports inferior-tty-claim; "
                    "disabling claim for Terminal.app console"));
            }
            else
            {
                m_pDriver->DebugLog(_("GDB lacks inferior-tty-claim; "
                    "leaving default tty claim behaviour"));
            }

            wxString gdbTtyCmd;
            gdbTtyCmd << "tty " << m_ttyPath;
            m_pDriver->QueueCommand(new DebuggerCmd(m_pDriver, gdbTtyCmd, true));
            m_pDriver->DebugLog(wxString::Format(_("Queued:[%s]"), gdbTtyCmd.wx_str()));

            if (m_unbufferStdio && m_pidToAttach == 0)
            {
                m_pDriver->QueueCommand(
                    new GdbCmd_SetupMacOSUnbufferStdio(m_pDriver,
                                                       m_breakOnEntry,
                                                       m_pidToAttach));
            }
            else if (m_pidToAttach == 0)
                m_pDriver->Start(m_breakOnEntry);
            else
                m_pDriver->Attach(m_pidToAttach);
        }

    private:
        wxString m_ttyPath;
        bool m_breakOnEntry;
        int m_pidToAttach;
        bool m_unbufferStdio;
};
} // namespace
#endif // !__WXMSW__

// valid debugger command constants
enum DebugCommandConst
{
    CMD_CONTINUE,
    CMD_STEP,
    CMD_STEPIN,
    CMD_STEPOUT,
    CMD_STEP_INSTR,
    CMD_STEP_INTO_INSTR,
    CMD_STOP,
    CMD_BACKTRACE,
    CMD_DISASSEMBLE,
    CMD_REGISTERS,
    CMD_MEMORYDUMP,
    CMD_RUNNINGTHREADS
};

namespace
{
long idMenuInfoFrame = wxNewId();
long idMenuInfoDLL = wxNewId();
long idMenuInfoFiles = wxNewId();
long idMenuInfoFPU = wxNewId();
long idMenuInfoSignals = wxNewId();

long idMenuInfoPrintElementsUnlimited = wxNewId();
long idMenuInfoPrintElements20 = wxNewId();
long idMenuInfoPrintElements50 = wxNewId();
long idMenuInfoPrintElements100 = wxNewId();
long idMenuInfoPrintElements200 = wxNewId();

long idMenuInfoCatchThrow = wxNewId();
long idMenuEnableDebuggingMac = wxNewId();

long idGDBProcess = wxNewId();
long idTimerPollDebugger = wxNewId();

long idMenuWatchDereference = wxNewId();
long idMenuWatchSymbol = wxNewId();

// this auto-registers the plugin
PluginRegistrant<DebuggerGDB> reg("Debugger");
}

BEGIN_EVENT_TABLE(DebuggerGDB, cbDebuggerPlugin)
    EVT_MENU(idMenuInfoFrame, DebuggerGDB::OnInfoFrame)
    EVT_MENU(idMenuInfoDLL, DebuggerGDB::OnInfoDLL)
    EVT_MENU(idMenuInfoFiles, DebuggerGDB::OnInfoFiles)
    EVT_MENU(idMenuInfoFPU, DebuggerGDB::OnInfoFPU)
    EVT_MENU(idMenuInfoSignals, DebuggerGDB::OnInfoSignals)

    EVT_MENU(idMenuWatchDereference, DebuggerGDB::OnMenuWatchDereference)
    EVT_MENU(idMenuWatchSymbol, DebuggerGDB::OnMenuWatchSymbol)

    EVT_PIPEDPROCESS_STDOUT(idGDBProcess, DebuggerGDB::OnGDBOutput)
    EVT_PIPEDPROCESS_STDERR(idGDBProcess, DebuggerGDB::OnGDBError)
    EVT_PIPEDPROCESS_TERMINATED(idGDBProcess, DebuggerGDB::OnGDBTerminated)

    EVT_IDLE(DebuggerGDB::OnIdle)
    EVT_TIMER(idTimerPollDebugger, DebuggerGDB::OnTimer)

    EVT_COMMAND(-1, DEBUGGER_CURSOR_CHANGED, DebuggerGDB::OnCursorChanged)
    EVT_COMMAND(-1, DEBUGGER_SHOW_FILE_LINE, DebuggerGDB::OnShowFile)

    EVT_UPDATE_UI(idMenuInfoPrintElementsUnlimited, DebuggerGDB::OnUpdateTools)
    EVT_UPDATE_UI(idMenuInfoPrintElements20, DebuggerGDB::OnUpdateTools)
    EVT_UPDATE_UI(idMenuInfoPrintElements50, DebuggerGDB::OnUpdateTools)
    EVT_UPDATE_UI(idMenuInfoPrintElements100, DebuggerGDB::OnUpdateTools)
    EVT_UPDATE_UI(idMenuInfoPrintElements200, DebuggerGDB::OnUpdateTools)

    EVT_MENU(idMenuInfoPrintElementsUnlimited, DebuggerGDB::OnPrintElements)
    EVT_MENU(idMenuInfoPrintElements20, DebuggerGDB::OnPrintElements)
    EVT_MENU(idMenuInfoPrintElements50, DebuggerGDB::OnPrintElements)
    EVT_MENU(idMenuInfoPrintElements100, DebuggerGDB::OnPrintElements)
    EVT_MENU(idMenuInfoPrintElements200, DebuggerGDB::OnPrintElements)

    EVT_UPDATE_UI(idMenuInfoCatchThrow, DebuggerGDB::OnUpdateCatchThrow)
    EVT_MENU(idMenuInfoCatchThrow, DebuggerGDB::OnCatchThrow)

    EVT_UPDATE_UI(idMenuEnableDebuggingMac, DebuggerGDB::OnUpdateEnableDebuggingMac)
    EVT_MENU(idMenuEnableDebuggingMac, DebuggerGDB::OnEnableDebuggingMac)
END_EVENT_TABLE()

DebuggerGDB::DebuggerGDB() :
    cbDebuggerPlugin(_("GDB/CDB debugger"), "gdb_debugger"),
    m_State(this),
    m_pProcess(nullptr),
    m_LastExitCode(0),
    m_Pid(0),
    m_PidToAttach(0),
    m_NoDebugInfo(false),
    m_StoppedOnSignal(false),
    m_pProject(nullptr),
    m_UseStandaloneDebuggee(false),
    m_bIsConsole(false),
    m_stopDebuggerConsoleClosed(false),
    m_nConsolePid(0),
    m_programConsoleDebug(false),
    m_TemporaryBreak(false),
    m_printElements(200),
    m_macGdbSigningDiagnosisLogged(false),
    m_macEnableDebuggingSuggested(false),
    m_macEnableDebuggingOfferScheduled(false)
{
    if (!Manager::LoadResource("debugger.zip"))
    {
        NotifyMissingFile("debugger.zip");
    }

    // get a function pointer to DebugBreakProcess under windows (XP+)
    #if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501)
    kernelLib = LoadLibrary(TEXT("kernel32.dll"));
    if (kernelLib)
    {
        DebugBreakProcessFunc = (DebugBreakProcessApiCall)GetProcAddress(kernelLib, "DebugBreakProcess");
        //Windows XP
        CreateToolhelp32SnapshotFunc = (CreateToolhelp32SnapshotApiCall)GetProcAddress(kernelLib, "CreateToolhelp32Snapshot");
        Process32FirstFunc = (Process32FirstApiCall)GetProcAddress(kernelLib, "Process32First");
        Process32NextFunc = (Process32NextApiCall)GetProcAddress(kernelLib, "Process32Next");
    }
    #endif
}

DebuggerGDB::~DebuggerGDB()
{
    #if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501)
    if (kernelLib)
        FreeLibrary(kernelLib);
    #endif
}

void DebuggerGDB::OnAttachReal()
{
    m_TimerPollDebugger.SetOwner(this, idTimerPollDebugger);

    // register event sink
    Manager::Get()->RegisterEventSink(cbEVT_BUILDTARGET_SELECTED, new cbEventFunctor<DebuggerGDB, CodeBlocksEvent>(this, &DebuggerGDB::OnBuildTargetSelected));

    // Menu bar may not be ready yet; retry after the event loop runs.
    EnsureMacEnableDebuggingMenuItem();
    CallAfter([this] { EnsureMacEnableDebuggingMenuItem(); });
}

void DebuggerGDB::OnReleaseReal(cb_unused bool appShutDown)
{
    //Close debug session when appShutDown
    if (m_State.HasDriver())
    {
        Stop();
        wxYieldIfNeeded();
    }

    m_State.CleanUp();
    KillConsole();
}

bool DebuggerGDB::SupportsFeature(cbDebuggerFeature::Flags flag)
{
    DebuggerConfiguration &config = GetActiveConfigEx();

    if (config.IsGDB())
    {
        switch (flag)
        {
        case cbDebuggerFeature::Breakpoints:
        case cbDebuggerFeature::Callstack:
        case cbDebuggerFeature::CPURegisters:
        case cbDebuggerFeature::Disassembly:
        case cbDebuggerFeature::Watches:
        case cbDebuggerFeature::ValueTooltips:
        case cbDebuggerFeature::ExamineMemory:
        case cbDebuggerFeature::Threads:
        case cbDebuggerFeature::RunToCursor:
        case cbDebuggerFeature::SetNextStatement:
            return true;
        default:
            return false;
        }
    }
    else
    {
        switch (flag)
        {
        case cbDebuggerFeature::Breakpoints:
        case cbDebuggerFeature::Callstack:
        case cbDebuggerFeature::CPURegisters:
        case cbDebuggerFeature::Disassembly:
        case cbDebuggerFeature::Watches:
        case cbDebuggerFeature::ValueTooltips:
        case cbDebuggerFeature::ExamineMemory:
        case cbDebuggerFeature::Threads:
            return true;
        case cbDebuggerFeature::RunToCursor:
        case cbDebuggerFeature::SetNextStatement:
        default:
            return false;
        }
    }

    return false;
}

cbDebuggerConfiguration* DebuggerGDB::LoadConfig(const ConfigManagerWrapper &config)
{
    return new DebuggerConfiguration(config);
}

DebuggerConfiguration& DebuggerGDB::GetActiveConfigEx()
{
    return static_cast<DebuggerConfiguration&>(GetActiveConfig());
}

cbConfigurationPanel* DebuggerGDB::GetProjectConfigurationPanel(wxWindow* parent, cbProject* project)
{
    DebuggerOptionsProjectDlg* dlg = new DebuggerOptionsProjectDlg(parent, this, project);
    return dlg;
}

void DebuggerGDB::OnConfigurationChange(cb_unused bool isActive)
{
    DebuggerConfiguration &config = GetActiveConfigEx();
    bool locals = config.GetFlag(DebuggerConfiguration::WatchLocals);
    bool funcArgs = config.GetFlag(DebuggerConfiguration::WatchFuncArgs);

    cbWatchesDlg *watchesDialog = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    bool update = false;

    if (!locals)
    {
        if (m_localsWatch)
        {
            watchesDialog->RemoveWatch(m_localsWatch);
            m_localsWatch = cb::shared_ptr<GDBWatch>();
        }
    }
    else if (!m_localsWatch)
        update = true;

    if (!funcArgs)
    {
        if (m_funcArgsWatch)
        {
            watchesDialog->RemoveWatch(m_funcArgsWatch);
            m_funcArgsWatch = cb::shared_ptr<GDBWatch>();
        }
    }
    else if (!m_funcArgsWatch)
        update = true;

    if (update)
        RequestUpdate(cbDebuggerPlugin::Watches);
}

wxArrayString DebuggerGDB::ParseSearchDirs(const cbProject &project)
{
    wxArrayString dirs;
    const TiXmlElement* elem = static_cast<const TiXmlElement*>(project.GetExtensionsNode());
    if (elem)
    {
        const TiXmlElement* conf = elem->FirstChildElement("debugger");
        if (conf)
        {
            const TiXmlElement* pathsElem = conf->FirstChildElement("search_path");
            while (pathsElem)
            {
                if (pathsElem->Attribute("add"))
                {
                    const wxString &dir = cbC2U(pathsElem->Attribute("add"));
                    if (dirs.Index(dir) == wxNOT_FOUND)
                        dirs.Add(dir);
                }
                pathsElem = pathsElem->NextSiblingElement("search_path");
            }
        }
    }

    return dirs;
}

TiXmlElement* GetElementForSaving(cbProject &project, const char *elementsToClear)
{
    TiXmlElement *elem = static_cast<TiXmlElement*>(project.GetExtensionsNode());

    // since rev4332, the project keeps a copy of the <Extensions> element
    // and re-uses it when saving the project (so to avoid losing entries in it
    // if plugins that use that element are not loaded atm).
    // so, instead of blindly inserting the element, we must first check it's
    // not already there (and if it is, clear its contents)
    TiXmlElement* node = elem->FirstChildElement("debugger");
    if (!node)
        node = elem->InsertEndChild(TiXmlElement("debugger"))->ToElement();

    for (TiXmlElement* child = node->FirstChildElement(elementsToClear);
         child;
         child = node->FirstChildElement(elementsToClear))
    {
        node->RemoveChild(child);
    }
    return node;
}

void DebuggerGDB::SetSearchDirs(cbProject &project, const wxArrayString &dirs)
{
    TiXmlElement* node = GetElementForSaving(project, "search_path");
    if (dirs.GetCount() > 0)
    {
        for (size_t i = 0; i < dirs.GetCount(); ++i)
        {
            TiXmlElement* path = node->InsertEndChild(TiXmlElement("search_path"))->ToElement();
            path->SetAttribute("add", cbU2C(dirs[i]));
        }
    }
}

RemoteDebuggingMap DebuggerGDB::ParseRemoteDebuggingMap(cbProject &project)
{
    RemoteDebuggingMap map;
    const TiXmlElement* elem = static_cast<const TiXmlElement*>(project.GetExtensionsNode());
    if (elem)
    {
        const TiXmlElement* conf = elem->FirstChildElement("debugger");
        if (conf)
        {
            const TiXmlElement* rdElem = conf->FirstChildElement("remote_debugging");
            while (rdElem)
            {
                wxString targetName = cbC2U(rdElem->Attribute("target"));
                ProjectBuildTarget* bt = project.GetBuildTarget(targetName);

                const TiXmlElement* rdOpt = rdElem->FirstChildElement("options");
                if (rdOpt)
                {
                    RemoteDebugging rd;

                    if (rdOpt->Attribute("conn_type"))
                        rd.connType = (RemoteDebugging::ConnectionType)atol(rdOpt->Attribute("conn_type"));
                    if (rdOpt->Attribute("serial_port"))
                        rd.serialPort = cbC2U(rdOpt->Attribute("serial_port"));

                    if (rdOpt->Attribute("serial_baud"))
                        rd.serialBaud = cbC2U(rdOpt->Attribute("serial_baud"));
                    if (rd.serialBaud.empty())
                        rd.serialBaud = "115200";

                    if (rdOpt->Attribute("ip_address"))
                        rd.ip = cbC2U(rdOpt->Attribute("ip_address"));
                    if (rdOpt->Attribute("ip_port"))
                        rd.ipPort = cbC2U(rdOpt->Attribute("ip_port"));
                    if (rdOpt->Attribute("additional_cmds"))
                        rd.additionalCmds = cbC2U(rdOpt->Attribute("additional_cmds"));
                    if (rdOpt->Attribute("additional_cmds_before"))
                        rd.additionalCmdsBefore = cbC2U(rdOpt->Attribute("additional_cmds_before"));
                    if (rdOpt->Attribute("skip_ld_path"))
                        rd.skipLDpath = cbC2U(rdOpt->Attribute("skip_ld_path")) != "0";
                    if (rdOpt->Attribute("extended_remote"))
                        rd.extendedRemote = cbC2U(rdOpt->Attribute("extended_remote")) != "0";
                    if (rdOpt->Attribute("additional_shell_cmds_after"))
                        rd.additionalShellCmdsAfter = cbC2U(rdOpt->Attribute("additional_shell_cmds_after"));
                    if (rdOpt->Attribute("additional_shell_cmds_before"))
                        rd.additionalShellCmdsBefore = cbC2U(rdOpt->Attribute("additional_shell_cmds_before"));

                    map.insert(map.end(), std::make_pair(bt, rd));
                }

                rdElem = rdElem->NextSiblingElement("remote_debugging");
            }
        }
    }
    return map;
}

void DebuggerGDB::SetRemoteDebuggingMap(cbProject &project, const RemoteDebuggingMap &rdMap)
{
    TiXmlElement* node = GetElementForSaving(project, "remote_debugging");

    if (!rdMap.empty())
    {
        typedef std::map<wxString, const RemoteDebugging*> MapTargetNameToRD;
        MapTargetNameToRD mapTargetNameToRD;

        for (RemoteDebuggingMap::const_iterator it = rdMap.begin(); it != rdMap.end(); ++it)
        {
            wxString targetName = (it->first ? it->first->GetTitle() : wxString());
            const RemoteDebugging& rd = it->second;
            mapTargetNameToRD.emplace(targetName, &rd);
        }

        for (MapTargetNameToRD::const_iterator it = mapTargetNameToRD.begin();
             it != mapTargetNameToRD.end();
             ++it)
        {
            const RemoteDebugging& rd = *it->second;

            // if no different than defaults, skip it
            if (rd.serialPort.IsEmpty() && rd.serialBaud == "115200"
                && rd.ip.IsEmpty() && rd.ipPort.IsEmpty()
                && !rd.skipLDpath && !rd.extendedRemote
                && rd.additionalCmds.IsEmpty() && rd.additionalCmdsBefore.IsEmpty()
                && rd.additionalShellCmdsAfter.IsEmpty()
                && rd.additionalShellCmdsBefore.IsEmpty())
            {
                continue;
            }

            TiXmlElement* rdnode = node->InsertEndChild(TiXmlElement("remote_debugging"))->ToElement();
            if (!it->first.empty())
                rdnode->SetAttribute("target", cbU2C(it->first));

            TiXmlElement* tgtnode = rdnode->InsertEndChild(TiXmlElement("options"))->ToElement();
            tgtnode->SetAttribute("conn_type", (int)rd.connType);
            if (!rd.serialPort.IsEmpty())
                tgtnode->SetAttribute("serial_port", cbU2C(rd.serialPort));
            if (rd.serialBaud != "115200")
                tgtnode->SetAttribute("serial_baud", cbU2C(rd.serialBaud));
            if (!rd.ip.IsEmpty())
                tgtnode->SetAttribute("ip_address", cbU2C(rd.ip));
            if (!rd.ipPort.IsEmpty())
                tgtnode->SetAttribute("ip_port", cbU2C(rd.ipPort));
            if (!rd.additionalCmds.IsEmpty())
                tgtnode->SetAttribute("additional_cmds", cbU2C(rd.additionalCmds));
            if (!rd.additionalCmdsBefore.IsEmpty())
                tgtnode->SetAttribute("additional_cmds_before", cbU2C(rd.additionalCmdsBefore));
            if (rd.skipLDpath)
                tgtnode->SetAttribute("skip_ld_path", "1");
            if (rd.extendedRemote)
                tgtnode->SetAttribute("extended_remote", "1");
            if (!rd.additionalShellCmdsAfter.IsEmpty())
                tgtnode->SetAttribute("additional_shell_cmds_after", cbU2C(rd.additionalShellCmdsAfter));
            if (!rd.additionalShellCmdsBefore.IsEmpty())
                tgtnode->SetAttribute("additional_shell_cmds_before", cbU2C(rd.additionalShellCmdsBefore));
        }
    }
}

void DebuggerGDB::DoWatches()
{
    if (!m_pProcess)
        return;

    DebuggerConfiguration &config = GetActiveConfigEx();

    bool locals = config.GetFlag(DebuggerConfiguration::WatchLocals);
    bool funcArgs = config.GetFlag(DebuggerConfiguration::WatchFuncArgs);

    if (locals)
    {
        if (m_localsWatch == nullptr)
        {
            m_localsWatch = cb::shared_ptr<GDBWatch>(new GDBWatch(_("Locals")));
            m_localsWatch->Expand(true);
            m_localsWatch->MarkAsChanged(false);
            cbWatchesDlg *watchesDialog = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
            watchesDialog->AddSpecialWatch(m_localsWatch, true);
        }
    }

    if (funcArgs)
    {
        if (m_funcArgsWatch == nullptr)
        {
            m_funcArgsWatch = cb::shared_ptr<GDBWatch>(new GDBWatch(_("Function arguments")));
            m_funcArgsWatch->Expand(true);
            m_funcArgsWatch->MarkAsChanged(false);
            cbWatchesDlg *watchesDialog = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
            watchesDialog->AddSpecialWatch(m_funcArgsWatch, true);
        }
    }

    m_State.GetDriver()->UpdateWatches(m_localsWatch, m_funcArgsWatch, m_watches, false);
}

static wxString GetShellString()
{
    if (platform::windows)
        return wxEmptyString;
    wxString shell = Manager::Get()->GetConfigManager("app")->Read("/console_shell",
                                                                       DEFAULT_CONSOLE_SHELL);
    // GDB expects the SHELL variable's value to be a path to the shell's executable, so we need to
    // remove all parameters and do some trimming.
    shell.Trim(false);
    wxString::size_type pos = shell.find(' ');
    if (pos != wxString::npos)
        shell.erase(pos);
    shell.Trim();
    return shell;
}

int DebuggerGDB::LaunchProcessWithShell(const wxString &cmd, wxProcess *process,
                                        const wxString &cwd)
{
    wxString shell = GetShellString();
    wxExecuteEnv execEnv;
    execEnv.cwd = cwd;
    // Read the current environment variables and then make changes to them.
    wxGetEnvMap(&execEnv.env);
    if (!shell.empty())
    {
        Log(wxString::Format(_("Setting SHELL to '%s'"), shell));
        execEnv.env["SHELL"] = shell;
    }
    return wxExecute(cmd, wxEXEC_ASYNC, process, &execEnv);
}

int DebuggerGDB::LaunchProcess(const wxString& cmd, const wxString& cwd)
{
    if (m_pProcess)
        return -1;

    // start the gdb process
    m_pProcess = new PipedProcess(&m_pProcess, this, idGDBProcess, true, cwd);
    Log(_("Starting debugger: ") + cmd);
    m_Pid = LaunchProcessWithShell(cmd, m_pProcess, cwd);

#ifdef __WXMAC__
    if (m_Pid == -1)
    {
        // Great! We got a fake PID. Time to Go Fish with our "ps" rod:

        m_Pid = 0;
        pid_t mypid = getpid();
        wxString mypidStr;
        mypidStr << mypid;

        long pspid = 0;
        wxString psCmd;
        wxArrayString psOutput;
        wxArrayString psErrors;

        psCmd << "/bin/ps -o ppid,pid,command";
        DebugLog(wxString::Format( _("Executing: %s"), psCmd.wx_str()) );
        int result = wxExecute(psCmd, psOutput, psErrors, wxEXEC_SYNC);

        mypidStr << ' ';

        for (int i = 0; i < psOutput.GetCount(); ++i)
        { //  PPID   PID COMMAND
           wxString psLine = psOutput.Item(i);
           if (psLine.StartsWith(mypidStr) && psLine.Contains("gdb"))
           {
               wxString pidStr = psLine.Mid(mypidStr.Length());
               pidStr = pidStr.BeforeFirst(' ');
               if (pidStr.ToLong(&pspid))
               {
                   m_Pid = pspid;
                   break;
               }
           }
         }

        for (int i = 0; i < psErrors.GetCount(); ++i)
            DebugLog(wxString::Format( _("PS Error:%s"), psErrors.Item(i).wx_str()) );
    }
#endif

    if (!m_Pid)
    {
        delete m_pProcess;
        m_pProcess = nullptr;
        Log(_("failed"), Logger::error);
        m_macGdbSessionEvidence.launchFailed = true;
        LogMacGdbSigningDiagnosisOnce(_("debugger launch failed"));
        return -1;
    }
    else if (!m_pProcess->GetOutputStream())
    {
        delete m_pProcess;
        m_pProcess = nullptr;
        Log(_("failed (to get debugger's stdin)"), Logger::error);
        m_macGdbSessionEvidence.launchFailed = true;
        LogMacGdbSigningDiagnosisOnce(_("debugger launch failed (stdin)"));
        return -2;
    }
    else if (!m_pProcess->GetInputStream())
    {
        delete m_pProcess;
        m_pProcess = nullptr;
        Log(_("failed (to get debugger's stdout)"), Logger::error);
        m_macGdbSessionEvidence.launchFailed = true;
        LogMacGdbSigningDiagnosisOnce(_("debugger launch failed (stdout)"));
        return -2;
    }
    else if (!m_pProcess->GetErrorStream())
    {
        delete m_pProcess;
        m_pProcess = nullptr;
        Log(_("failed (to get debugger's stderr)"), Logger::error);
        m_macGdbSessionEvidence.launchFailed = true;
        LogMacGdbSigningDiagnosisOnce(_("debugger launch failed (stderr)"));
        return -2;
    }
    Log(_("done"));
    return 0;
}

bool DebuggerGDB::IsStopped() const
{
    return !m_State.HasDriver() || m_State.GetDriver()->IsProgramStopped();
}

bool DebuggerGDB::IsBusy() const
{
    return m_State.HasDriver() && m_State.GetDriver()->IsQueueBusy();
}


bool DebuggerGDB::Debug(bool breakOnEntry)
{
    // if already running, return
    if (m_pProcess || WaitingCompilerToFinish())
        return false;

    m_pProject = nullptr;
    m_NoDebugInfo = false;
    m_UseStandaloneDebuggee = false;
    m_StandaloneDebuggee = DebuggeeDescription();

    ProjectManager* prjMan = Manager::Get()->GetProjectManager();
    cbProject* project = prjMan->GetActiveProject();

    // Ownership routing: standalone candidate (incl. with empty/unrelated project
    // open) uses v2.1 DebuggeeDescription path. Empty-project refuse applies only
    // to the project route below — never suppresses a valid standalone identity.
    if (m_PidToAttach == 0 && DebuggeeDescriptionUtil::IsStandaloneDebugCandidate())
    {
        wxString sourcePath;
        if (!DebuggeeDescriptionUtil::SaveActiveSourceForStandaloneDebug(sourcePath))
        {
            Log(_("Cannot debug: source must be saved to a filesystem path first."), Logger::error);
            return false;
        }
        // Re-check after Save As: file may have been saved into the active project.
        if (DebuggeeDescriptionUtil::BelongsToActiveProject(
                Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor(), sourcePath))
        {
            // Fall through to project Debug with the post-save membership.
            project = prjMan->GetActiveProject();
        }
        else
        {
            if (!DebuggeeDescriptionUtil::MakeStandalone(sourcePath, m_StandaloneDebuggee))
            {
                Log(_("Cannot build standalone debuggee description for this file."), Logger::error);
                return false;
            }
            m_UseStandaloneDebuggee = true;

            m_Canceled = false;
            if (!EnsureStandaloneRebuildForDebug(breakOnEntry ? StartTypeStepInto : StartTypeRun, sourcePath))
                return false;

            if (!WaitingCompilerToFinish() && !m_State.HasDriver() && !m_Canceled)
                return DoDebug(breakOnEntry) == 0;
            return true;
        }
    }

    m_pProject = project;
    if (m_pProject && m_ActiveBuildTarget.IsEmpty())
        m_ActiveBuildTarget = m_pProject->GetActiveBuildTarget();

    // Project route only: empty project must not launch a leftover binary.
    if (m_PidToAttach == 0)
    {
        if (!m_pProject)
            return false;
        if (!DebuggeeDescriptionUtil::ProjectHasDebuggeeSources(m_pProject))
        {
            Log(_("Cannot debug: the active project has no source files."), Logger::error);
            cbMessageBox(_("The active project has no source files to debug.\n"
                           "Add a source file to the project, or open a standalone "
                           "source file and Debug that instead."),
                         _("Debug"), wxICON_WARNING);
            return false;
        }
        if (m_pProject->GetModified() && !m_pProject->Save())
        {
            Log(_("Cannot debug: project file was not saved."), Logger::error);
            return false;
        }
        if (!m_pProject->SaveAllFiles())
        {
            Log(_("Cannot debug: one or more project files were not saved "
                  "(save canceled?)."), Logger::error);
            return false;
        }
    }

    m_Canceled = false;
    if (!EnsureBuildUpToDate(breakOnEntry ? StartTypeStepInto : StartTypeRun))
        return false;

    // if not waiting for the compiler, start debugging now
    // but first check if the driver has already been started:
    // if the build process was ultra-fast (i.e. nothing to be done),
    // it may have already called DoDebug() and m_WaitingCompilerToFinish
    // would already be set to false
    // by checking the driver availability, we avoid calling DoDebug
    // a second consecutive time...
    // the same applies for m_Canceled: it is true if DoDebug() was launched but
    // returned an error
    if (!WaitingCompilerToFinish() && !m_State.HasDriver() && !m_Canceled)
    {
        return DoDebug(breakOnEntry) == 0;
    }

    return true;
}

int DebuggerGDB::DoDebug(bool breakOnEntry)
{
    // set this to true before every error exit point in this function
    m_Canceled = false;
    // Init these just in case.
    m_bIsConsole = false;
    m_nConsolePid = 0;
    m_programConsoleDebug = false;
    ResetMacGdbSigningDiagnosis();
    ProjectManager* prjMan = Manager::Get()->GetProjectManager();

    // select the build target to debug (project path only; standalone uses DebuggeeDescription)
    ProjectBuildTarget* target = nullptr;
    Compiler* actualCompiler = nullptr;
    if ( (m_PidToAttach == 0) && m_pProject && !m_UseStandaloneDebuggee)
    {
        Log(_("Selecting target: "));
        if (!m_pProject->BuildTargetValid(m_ActiveBuildTarget, false))
        {
            int tgtIdx = m_pProject->SelectTarget();
            if (tgtIdx == -1)
            {
                Log(_("canceled"));
                m_Canceled = true;
                return 3;
            }
            target = m_pProject->GetBuildTarget(tgtIdx);
            m_ActiveBuildTarget = (target ? target->GetTitle() : wxString(wxEmptyString));
        }
        else
            target = m_pProject->GetBuildTarget(m_ActiveBuildTarget);

        // make sure it's not a commands-only target
        if (target && target->GetTargetType() == ttCommandsOnly)
        {
            cbMessageBox(_("The selected target is only running pre/post build step commands\n"
                           "Can't debug such a target..."), _("Information"), wxICON_INFORMATION);
            Log(_("aborted"));
            return 3;
        }
        if (target) Log(target->GetTitle());

        // find the target's compiler (to see which debugger to use)
        actualCompiler = CompilerFactory::GetCompiler(target ? target->GetCompilerID()
                                                             : m_pProject->GetCompilerID());
    }
    else
        actualCompiler = CompilerFactory::GetDefaultCompiler();

    if (!actualCompiler)
    {
        wxString msg;
        const wxChar* what = _("project");
        if (target)
            what = _("target");
        else if (m_UseStandaloneDebuggee)
            what = _("file");
        msg.Printf(_("This %s is configured to use an invalid debugger.\nThe operation failed..."), what);
        cbMessageBox(msg, _("Error"), wxICON_ERROR);
        m_Canceled = true;
        return 9;
    }

    // is gdb accessible, i.e. can we find it?
    wxString cmdexe;
    cmdexe = GetActiveConfigEx().GetDebuggerExecutable();
    cmdexe.Trim();
    cmdexe.Trim(true);
    if (cmdexe.IsEmpty())
    {
        Log(_("ERROR: You need to specify a debugger program in the debuggers's settings."), Logger::error);

        if (platform::windows)
        {
            Log(_("(For MinGW compilers, it's 'gdb.exe' (without the quotes))"), Logger::error);
            Log(_("(For MSVC compilers, it's 'cdb.exe' (without the quotes))"), Logger::error);
        }
        else
        {
            Log(_("(For GCC compilers, it's 'gdb' (without the quotes))"), Logger::error);
        }

        m_Canceled = true;
        return -1;
    }

    // start debugger driver based on target compiler, or default compiler if no target
    if (!m_State.StartDriver(target))
    {
        cbMessageBox(_("Could not decide which debugger to use!"), _("Error"), wxICON_ERROR);
        m_Canceled = true;
        return -1;
    }

    // Notify debugger plugins so they could start a GDB server process
    PluginManager *plm = Manager::Get()->GetPluginManager();
    CodeBlocksEvent evt(cbEVT_DEBUGGER_STARTED);
    plm->NotifyPlugins(evt);
    int nRet = evt.GetInt();
    if (nRet < 0)
    {
        cbMessageBox(_("A plugin interrupted the debug process."));
        Log(_("Aborted by plugin"));
        m_Canceled = true;
        return -1;
    }
    // Continue

    // create gdb launch command

    // prepare the driver
    wxString cmdline;
    if (m_PidToAttach == 0)
    {
        m_State.GetDriver()->ClearDirectories();
        // add other open projects dirs as search dirs (only if option is enabled)
        if (GetActiveConfigEx().GetFlag(DebuggerConfiguration::AddOtherProjectDirs))
        {
            // add as include dirs all open project base dirs
            ProjectsArray* projects = prjMan->GetProjects();
            for (unsigned int i = 0; i < projects->GetCount(); ++i)
            {
                cbProject* it = projects->Item(i);
                // skip if it's THE project (added last)
                if (it == m_pProject)
                    continue;
                AddSourceDir(it->GetBasePath());
                AddSourceDir(it->GetCommonTopLevelPath());
            }
        }

        // Note: I've done some testing and parsing 1000 strings takes 2-3ms, so this isn't really
        // a problem. The code adding the source dirs to the debugger driver is a lot slower!
        // now add all per-project user-set search dirs
        if (m_pProject)
        {
            const wxArrayString& pdirs = ParseSearchDirs(*m_pProject);
            for (size_t i = 0; i < pdirs.GetCount(); ++i)
                AddSourceDir(pdirs[i]);

            AddSourceDir(m_pProject->GetBasePath());
            AddSourceDir(m_pProject->GetCommonTopLevelPath());
        }

        // set the file to debug (project target, or standalone DebuggeeDescription)
        wxString debuggee, path;
        if (m_UseStandaloneDebuggee)
        {
            if (!m_StandaloneDebuggee.IsValid())
            {
                m_Canceled = true;
                return -3;
            }
            debuggee = m_StandaloneDebuggee.executablePath;
            path = m_StandaloneDebuggee.workingDirectory;
            if (!wxFileExists(debuggee))
            {
                Log(wxString::Format(_("Standalone debuggee not found after rebuild: '%s'"),
                                     debuggee), Logger::error);
                m_Canceled = true;
                return -3;
            }
            // So GDB can resolve sources next to the single file (breakpoints / SyncEditor).
            AddSourceDir(path);
            Log(_("Standalone debug (no project)"));
            Log(_("Adding file: ") + debuggee);
            ConvertDirectory(debuggee, wxEmptyString, true);
            if (!m_StandaloneDebuggee.arguments.empty())
                m_State.GetDriver()->SetArguments(m_StandaloneDebuggee.arguments);
        }
        else if ( !GetDebuggee(debuggee, path, target) )
        {
            m_Canceled = true;
            return -3;
        }

        if (!path.empty())
        {
            ConvertToGDBDirectory(path);
            if (path != ".") // avoid silly message "changing to ."
            {
                Log(_("Changing directory to: ") + path);
                m_State.GetDriver()->SetWorkingDirectory(path);
            }
        }

        if (!m_UseStandaloneDebuggee && target && !target->GetExecutionParameters().IsEmpty())
            m_State.GetDriver()->SetArguments(target->GetExecutionParameters());

        cmdline = m_State.GetDriver()->GetCommandLine(cmdexe, debuggee, GetActiveConfigEx().GetUserArguments());
    }
    else // m_PidToAttach != 0
        cmdline = m_State.GetDriver()->GetCommandLine(cmdexe, m_PidToAttach, GetActiveConfigEx().GetUserArguments());

    RemoteDebugging rd;

    if (m_pProject)
    {
        // Note: This is parsing the remote debugging info on every start. It is not fast but it is
        //       not slow - parsing the info for 500 targets takes 2-3ms, which is fine.
        //       One easy optimization is to parse  the info only for the project and the target
        //       which is being debugged.
        const RemoteDebuggingMap &remoteDebuggingMap = ParseRemoteDebuggingMap(*m_pProject);
        // project settings
        RemoteDebuggingMap::const_iterator it = remoteDebuggingMap.find(nullptr);
        if (it != remoteDebuggingMap.end())
            rd = it->second;

        // target settings
        it = remoteDebuggingMap.find(target);
        if (it != remoteDebuggingMap.end())
            rd.MergeWith(it->second);
    }

//////////////////killerbot : most probably here : execute the shell commands (we could access the per target debugger settings)
    wxString oldLibPath; // keep old PATH/LD_LIBRARY_PATH contents
    if (!rd.skipLDpath)
    {
        wxGetEnv(CB_LIBRARY_ENVVAR, &oldLibPath);

        // setup dynamic linker path
        if (actualCompiler && target)
        {
            wxString newLibPath;
            const wxString libPathSep = platform::windows ? ";" : ":";
            newLibPath << "." << libPathSep;

            CompilerCommandGenerator *generator = actualCompiler->GetCommandGenerator(m_pProject);
            newLibPath << GetStringFromArray(generator->GetLinkerSearchDirs(target), libPathSep);
            delete generator;

            if (newLibPath.Mid(newLibPath.Length() - 1, 1) != libPathSep)
                newLibPath << libPathSep;
            newLibPath << oldLibPath;
            wxSetEnv(CB_LIBRARY_ENVVAR, newLibPath);
            Log(wxString(_("Set variable: ")) + CB_LIBRARY_ENVVAR "=" + newLibPath);
        }
    }

    #ifdef __WXMSW__
    if (!m_State.GetDriver()->UseDebugBreakProcess())
    {
        AllocConsole();
        // c_str() is needed when wxUSE_STL = 1
        SetConsoleTitle(_("Codeblocks debug console - DO NOT CLOSE!").c_str());
        SetConsoleCtrlHandler(HandlerRoutine, TRUE);
        m_bIsConsole = true;

        HWND windowHandle = GetConsoleWindow();
        if (windowHandle)
            ShowWindow(windowHandle, SW_HIDE);
    }
    #endif

    // start the gdb process
    wxString wdir = m_State.GetDriver()->GetDebuggersWorkingDirectory();
    if (wdir.empty())
    {
        if (m_UseStandaloneDebuggee && !m_StandaloneDebuggee.workingDirectory.empty())
            wdir = m_StandaloneDebuggee.workingDirectory;
        else if (m_pProject)
            wdir = m_pProject->GetBasePath();
        else
            wdir = ".";
    }
    DebugLog(_("Command-line: ") + cmdline);
    DebugLog(_("Working dir : ") + wdir);
    int ret = LaunchProcess(cmdline, wdir);

    if (!rd.skipLDpath)
    {
        // restore dynamic linker path
        wxSetEnv(CB_LIBRARY_ENVVAR, oldLibPath);
    }

    if (ret != 0)
    {
        m_Canceled = true;
        return ret;
    }

    // start polling gdb's output
    m_TimerPollDebugger.Start(20);

    // although I don't really like these do-nothing loops, we must wait a small amount of time
    // for gdb to see if it really started: it may fail to load shared libs or whatever
    // the reason this is added is because I had a case where gdb would error and bail out
    // *while* the driver->Prepare() call was running below and hell broke loose...
    int i = 50;
    while (i)
    {
        wxMilliSleep(1);
        Manager::Yield();
        --i;
    }
    if (!m_State.HasDriver())
        return -1;

    bool isConsole = m_UseStandaloneDebuggee
                     ? m_StandaloneDebuggee.isConsole
                     : (target && target->GetTargetType() == ttConsoleOnly);
    m_State.GetDriver()->Prepare(isConsole, m_printElements, rd);
    m_State.ApplyBreakpoints();

   #ifndef __WXMSW__
    // Console Debug: either Program Console (CB-owned PTY, claim ON) or
    // Terminal.app / xterm + holder (macOS claim-off). Issue tty to GDB.
    m_bIsConsole = m_UseStandaloneDebuggee
                   ? m_StandaloneDebuggee.isConsole
                   : (target && target->GetUseConsoleRunner());
    m_programConsoleDebug = false;
    bool deferStartForMacOSTty = false;
    // M5: macOS console Debug may probe companion GDB for unbuffered stdout.
    // Attach never gets the set.  Claim-off path is unchanged for Terminal.app.
    const bool wantMacOSUnbufferStdio =
        platform::macosx
        && m_PidToAttach == 0
        && GetActiveConfigEx().GetFlag(DebuggerConfiguration::UnbufferStdio);
    if (m_bIsConsole)
    {
        wxString consoleTty;
        if (platform::macosx && ProgramConsole::PreferProgramConsole())
        {
            /* Program Console: claim stays ON.  Optionally defer Start through
               the unbuffer probe so set precedes run/start. */
            if (ProgramConsole::BeginDebugSession(consoleTty))
            {
                m_programConsoleDebug = true;
                m_nConsolePid = 0;
                m_stopDebuggerConsoleClosed = false;
                ProgramConsole::ActivateDebugSession();
                wxString gdbTtyCmd;
                gdbTtyCmd << "tty " << consoleTty;
                m_State.GetDriver()->QueueCommand(
                    new DebuggerCmd(m_State.GetDriver(), gdbTtyCmd, true));
                DebugLog(wxString::Format(_("Queued:[%s] (Program Console, claim ON)"),
                                          gdbTtyCmd.wx_str()));
                if (wantMacOSUnbufferStdio)
                {
                    m_State.GetDriver()->QueueCommand(
                        new GdbCmd_SetupMacOSUnbufferStdio(m_State.GetDriver(),
                                                           breakOnEntry,
                                                           m_PidToAttach));
                    deferStartForMacOSTty = true;
                }
            }
            else
            {
                Log(_("Program Console: failed to allocate PTY for Debug "
                      "(previous output kept)."));
                m_bIsConsole = false;
            }
        }
        else
        {
            m_nConsolePid = RunNixConsole(consoleTty);
            if (m_nConsolePid > 0)
            {
                m_stopDebuggerConsoleClosed = true;
                // On macOS the Terminal+sleep holder already owns the pty.  Probe
                // for set inferior-tty-claim (Darwin GDB) and disable claiming so
                // the long TIOCSCTTY warning is not printed.  Start/Attach run from
                // the probe's ParseOutput so they stay after set/tty in the queue.
                if (platform::macosx)
                {
                    m_State.GetDriver()->QueueCommand(
                        new GdbCmd_SetupMacOSInferiorTty(m_State.GetDriver(),
                                                         consoleTty,
                                                         breakOnEntry,
                                                         m_PidToAttach,
                                                         wantMacOSUnbufferStdio));
                    deferStartForMacOSTty = true;
                }
                else
                {
                    wxString gdbTtyCmd;
                    gdbTtyCmd << "tty " << consoleTty;
                    m_State.GetDriver()->QueueCommand(new DebuggerCmd(m_State.GetDriver(), gdbTtyCmd, true));
                    DebugLog(wxString::Format( _("Queued:[%s]"), gdbTtyCmd.wx_str()) );
                }
            }
        }
    }//if
   #endif//ndef __WXMSW__

    // Don't issue 'run' if attaching to a process (Bug #1391904)
#ifndef __WXMSW__
    if (!deferStartForMacOSTty)
#endif
    {
        if (m_PidToAttach == 0)
            m_State.GetDriver()->Start(breakOnEntry);
        else
            m_State.GetDriver()->Attach(m_PidToAttach);
    }

    // switch to the user-defined layout for debugging
    if (m_pProcess)
        SwitchToDebuggingLayout();

    return 0;
} // Debug

void DebuggerGDB::AddSourceDir(const wxString& dir)
{
    if (!m_State.HasDriver() || dir.IsEmpty())
        return;
    wxString filename = dir;
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(filename); // apply env vars
    Log(_("Adding source dir: ") + filename);
    ConvertToGDBDirectory(filename, "", false);
    m_State.GetDriver()->AddDirectory(filename);
}

// static
void DebuggerGDB::StripQuotes(wxString& str)
{
    if (str.GetChar(0) == _T('\"') && str.GetChar(str.Length() - 1) == _T('\"'))
        str = str.Mid(1, str.Length() - 2);
}

// static
void DebuggerGDB::ConvertToGDBFriendly(wxString& str)
{
    if (str.IsEmpty())
        return;

    str = UnixFilename(str);
    while (str.Replace("\\", "/"))
        ;
    while (str.Replace("//", "/"))
        ;
    if (str.Find(_T(' ')) != -1 && str.GetChar(0) != _T('"'))
        str = "\"" + str + "\"";
}

// static
void DebuggerGDB::ConvertToGDBFile(wxString& str)
{
    wxFileName fname = str;
    str = fname.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
    DebuggerGDB::ConvertToGDBDirectory(str);
    str << fname.GetFullName();
}

void DebuggerGDB::ConvertDirectory(wxString& str, wxString base, bool relative)
{
    ConvertToGDBDirectory(str, base, relative);
}

// static
//if relative == false, try to leave as an absolute path
void DebuggerGDB::ConvertToGDBDirectory(wxString& str, wxString base, bool relative)
{
    if (str.IsEmpty())
        return;

    ConvertToGDBFriendly(str);
    ConvertToGDBFriendly(base);
    StripQuotes(str);
    StripQuotes(base);

    if (platform::windows)
    {
        int  ColonLocation   = str.Find(_T(':'));
        bool convert_path_83 = false;
        if (ColonLocation != wxNOT_FOUND)
            convert_path_83 = true;
        else if (!base.IsEmpty() && str.GetChar(0) != _T('/'))
        {
            if (base.GetChar(base.Length()) == _T('/'))
                base = base.Mid(0, base.Length() - 2);

            while (!str.IsEmpty())
            {
                base += "/" + str.BeforeFirst(_T('/'));
                if (str.Find(_T('/')) != wxNOT_FOUND) str = str.AfterFirst(_T('/'));
                else                                  str.Clear();
            }
            convert_path_83 = true;
        }

        // If can, get 8.3 name for path (Windows only)
        if (convert_path_83 && str.Contains(_T(' '))) // only if has spaces
        {
            wxFileName fn(str); // might contain a file name, too
            wxString path_83 = fn.GetShortPath();
            if (!path_83.IsEmpty())
                str = path_83; // construct filename again
        }

        if (ColonLocation == wxNOT_FOUND || base.IsEmpty())
            relative = false; // Can't do it
    }
    else
    {
        if ((str.GetChar(0) != _T('/') && str.GetChar(0) != _T('~')) || base.IsEmpty())
            relative = false;
    }

    if (relative)
    {
        if (platform::windows)
        {
            if (str.Find(_T(':')) != wxNOT_FOUND)
                str = str.Mid(str.Find(_T(':')) + 2, str.Length());
            if (base.Find(_T(':')) != wxNOT_FOUND)
                base = base.Mid(base.Find(_T(':')) + 2, base.Length());
        }
        else
        {
            if      (str.GetChar(0) == _T('/'))  str  = str.Mid(1, str.Length());
            else if (str.GetChar(0) == _T('~'))  str  = str.Mid(2, str.Length());

            if      (base.GetChar(0) == _T('/')) base = base.Mid(1, base.Length());
            else if (base.GetChar(0) == _T('~')) base = base.Mid(2, base.Length());
        }

        while (!base.IsEmpty() && !str.IsEmpty())
        {
            if (str.BeforeFirst(_T('/')) == base.BeforeFirst(_T('/')))
            {
                if (str.Find(_T('/')) == wxNOT_FOUND) str.Clear();
                else                                  str = str.AfterFirst(_T('/'));

                if (base.Find(_T('/')) == wxNOT_FOUND) base.Clear();
                else                                   base = base.AfterFirst(_T('/'));
            }
            else break;
        }
        while (!base.IsEmpty())
        {
            str = "../" + str;
            if (base.Find(_T('/')) == wxNOT_FOUND) base.Clear();
            else                                   base = base.AfterFirst(_T('/'));
        }
    }
    ConvertToGDBFriendly(str);
}

void DebuggerGDB::SendCommand(const wxString& cmd, bool debugLog)
{
    const wxString &cleandCmd = CleanStringValue(cmd);
    if (!debugLog)
        Log("> " + cleandCmd);

    if (debugLog)
        DoSendCommand(cleandCmd);
    else if (m_State.HasDriver())
        m_State.GetDriver()->QueueCommand(new DebuggerCmd(m_State.GetDriver(), cleandCmd, true));
}

void DebuggerGDB::DoSendCommand(const wxString& cmd)
{
    if (!m_pProcess || !IsStopped())
        return;

    if (HasDebugLog())
        DebugLog("> " + cmd);

    m_pProcess->SendString(cmd);
}

void DebuggerGDB::RequestUpdate(DebugWindows window)
{
    switch (window)
    {
        case Backtrace:
            RunCommand(CMD_BACKTRACE);
            break;
        case CPURegisters:
            RunCommand(CMD_REGISTERS);
            break;
        case Disassembly:
            RunCommand(CMD_DISASSEMBLE);
            break;
        case ExamineMemory:
            RunCommand(CMD_MEMORYDUMP);
            break;
        case MemoryRange:
            m_State.GetDriver()->UpdateMemoryRangeWatches(m_memoryRanges, false);
            break;
        case Threads:
            RunCommand(CMD_RUNNINGTHREADS);
            break;
        case Watches:
            if (IsWindowReallyShown(Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->GetWindow()))
                DoWatches();
            break;
        default:
            break;
    }
}

void DebuggerGDB::RunCommand(int cmd)
{
    // just check for the process
    if (!m_pProcess)
        return;

    bool debuggerContinued = false;

    switch (cmd)
    {
        case CMD_CONTINUE:
        {
            ClearActiveMarkFromAllEditors();
            if (m_State.HasDriver())
            {
                Log(_("Continuing..."));
                m_State.GetDriver()->Continue();
                m_State.GetDriver()->ResetCurrentFrame();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STEP:
        {
            ClearActiveMarkFromAllEditors();
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->Step();
                m_State.GetDriver()->ResetCurrentFrame();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STEP_INSTR:
        {
            ClearActiveMarkFromAllEditors();
            if (!Manager::Get()->GetDebuggerManager()->UpdateDisassembly())
            {
                // first time users should have some help from us ;)
                RunCommand(CMD_DISASSEMBLE);
            }
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->StepInstruction();
                m_State.GetDriver()->ResetCurrentFrame();
                m_State.GetDriver()->NotifyCursorChanged();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STEP_INTO_INSTR:
        {
            ClearActiveMarkFromAllEditors();
            if (!Manager::Get()->GetDebuggerManager()->UpdateDisassembly())
            {
                // first time users should have some help from us ;)
                RunCommand(CMD_DISASSEMBLE);
            }
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->StepIntoInstruction();
                m_State.GetDriver()->ResetCurrentFrame();
                m_State.GetDriver()->NotifyCursorChanged();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STEPIN:
        {
            ClearActiveMarkFromAllEditors();
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->StepIn();
                m_State.GetDriver()->ResetCurrentFrame();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STEPOUT:
        {
            ClearActiveMarkFromAllEditors();
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->StepOut();
                m_State.GetDriver()->ResetCurrentFrame();
                debuggerContinued = true;
            }
            break;
        }

        case CMD_STOP:
        {
            ClearActiveMarkFromAllEditors();
            if (m_State.HasDriver())
            {
                m_State.GetDriver()->Stop();
                m_State.GetDriver()->ResetCurrentFrame();
                MarkAsStopped();
            }
            break;
        }

        case CMD_BACKTRACE:
        {
            if (m_State.HasDriver())
                m_State.GetDriver()->Backtrace();
            break;
        }

        case CMD_DISASSEMBLE:
        {
            if (m_State.HasDriver())
                m_State.GetDriver()->Disassemble();
            break;
        }

        case CMD_REGISTERS:
        {
            if (m_State.HasDriver())
                m_State.GetDriver()->CPURegisters();
            break;
        }

        case CMD_MEMORYDUMP:
        {
            if (m_State.HasDriver())
                m_State.GetDriver()->MemoryDump();
            break;
        }

        case CMD_RUNNINGTHREADS:
        {
            if (m_State.HasDriver())
                m_State.GetDriver()->RunningThreads();
            break;
        }

        default: break;
    }

    if (debuggerContinued)
    {
        PluginManager *plm = Manager::Get()->GetPluginManager();
        CodeBlocksEvent evt(cbEVT_DEBUGGER_CONTINUED);
        evt.SetPlugin(this);
        plm->NotifyPlugins(evt);
    }
}

int DebuggerGDB::GetStackFrameCount() const
{
    return m_State.GetDriver()->GetStackFrames().size();
}

cb::shared_ptr<const cbStackFrame> DebuggerGDB::GetStackFrame(int index) const
{
    return m_State.GetDriver()->GetStackFrames()[index];
}

void DebuggerGDB::SwitchToFrame(int number)
{
    if (m_State.HasDriver())
    {
        m_State.GetDriver()->SetCurrentFrame(number, true);
        m_State.GetDriver()->SwitchToFrame(number);

        if (Manager::Get()->GetDebuggerManager()->UpdateBacktrace())
           Manager::Get()->GetDebuggerManager()->GetBacktraceDialog()->Reload();
    }
}

int DebuggerGDB::GetActiveStackFrame() const
{
    return m_State.HasDriver() ? m_State.GetDriver()->GetCurrentFrame() : 0;
}

int DebuggerGDB::GetThreadsCount() const
{
    if (!m_State.HasDriver())
        return 0;
    else
        return m_State.GetDriver()->GetThreads().size();
}

cb::shared_ptr<const cbThread> DebuggerGDB::GetThread(int index) const
{
    return m_State.GetDriver()->GetThreads()[index];
}

bool DebuggerGDB::SwitchToThread(int thread_number)
{
    if (!m_State.HasDriver())
        return false;
    DebuggerDriver *driver = m_State.GetDriver();
    DebuggerDriver::ThreadsContainer const &threads = driver->GetThreads();

    for (DebuggerDriver::ThreadsContainer::const_iterator it = threads.begin(); it != threads.end(); ++it)
    {
        if ((*it)->GetNumber() == thread_number)
        {
            if (!(*it)->IsActive())
                driver->SwitchThread(thread_number);
            return true;
        }
    }
    return false;
}

cb::shared_ptr<cbBreakpoint> DebuggerGDB::AddBreakpoint(const wxString& filename, int line)
{
    bool debuggerIsRunning = !IsStopped();
    if (debuggerIsRunning)
        DoBreak(true);

    cb::shared_ptr<DebuggerBreakpoint> bp = m_State.AddBreakpoint(filename, line, false);

    if (debuggerIsRunning)
        Continue();

    return bp;
}

cb::shared_ptr<cbBreakpoint> DebuggerGDB::AddDataBreakpoint(const wxString& dataExpression)
{
    DataBreakpointDlg dlg(Manager::Get()->GetAppWindow(), dataExpression, true, 1);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        const wxString& newDataExpression = dlg.GetDataExpression();
        int sel = dlg.GetSelection();
        cb::shared_ptr<DebuggerBreakpoint> bp = m_State.AddBreakpoint(newDataExpression, sel != 1, sel != 0);
        return bp;
    }
    else
        return cb::shared_ptr<cbBreakpoint>();
}

int DebuggerGDB::GetBreakpointsCount() const
{
    return m_State.GetBreakpoints().size();
}

cb::shared_ptr<cbBreakpoint> DebuggerGDB::GetBreakpoint(int index)
{
    BreakpointsList::const_iterator it = m_State.GetBreakpoints().begin();
    std::advance(it, index);
    cbAssert(it != m_State.GetBreakpoints().end());
    return *it;
}

cb::shared_ptr<const cbBreakpoint> DebuggerGDB::GetBreakpoint(int index) const
{
    BreakpointsList::const_iterator it = m_State.GetBreakpoints().begin();
    std::advance(it, index);
    cbAssert(it != m_State.GetBreakpoints().end());
    return *it;
}

void DebuggerGDB::UpdateBreakpoint(cb::shared_ptr<cbBreakpoint> breakpoint)
{
    const BreakpointsList &breakpoints = m_State.GetBreakpoints();
    BreakpointsList::const_iterator it = std::find(breakpoints.begin(), breakpoints.end(), breakpoint);
    if (it == breakpoints.end())
        return;
    cb::shared_ptr<DebuggerBreakpoint> bp = cb::static_pointer_cast<DebuggerBreakpoint>(breakpoint);
    bool reset = false;
    switch (bp->type)
    {
        case DebuggerBreakpoint::bptCode:
        {
            EditBreakpointDlg dlg(*bp, Manager::Get()->GetAppWindow());
            PlaceWindow(&dlg);
            if (dlg.ShowModal() == wxID_OK)
            {
                *bp = dlg.GetBreakpoint();
                reset = true;
            }
            break;
        }
        case DebuggerBreakpoint::bptData:
        {
            int old_sel = 0;
            if (bp->breakOnRead && bp->breakOnWrite)
                old_sel = 2;
            else if (!bp->breakOnRead && bp->breakOnWrite)
                old_sel = 1;
            DataBreakpointDlg dlg(Manager::Get()->GetAppWindow(), bp->breakAddress, bp->enabled, old_sel);
            PlaceWindow(&dlg);
            if (dlg.ShowModal() == wxID_OK)
            {
                bp->enabled = dlg.IsEnabled();
                bp->breakOnRead = dlg.GetSelection() != 1;
                bp->breakOnWrite = dlg.GetSelection() != 0;
                bp->breakAddress = dlg.GetDataExpression();
                reset = true;
            }
            break;
        }
        case DebuggerBreakpoint::bptFunction:
        default:
            return;
    }

    if (reset)
    {
        bool debuggerIsRunning = !IsStopped();
        if (debuggerIsRunning)
            DoBreak(true);

        m_State.ResetBreakpoint(bp);

        if (debuggerIsRunning)
            Continue();
    }
}

void DebuggerGDB::DeleteBreakpoint(cb::shared_ptr<cbBreakpoint> breakpoint)
{
    bool debuggerIsRunning = !IsStopped();
    if (debuggerIsRunning)
        DoBreak(true);

    m_State.RemoveBreakpoint(cb::static_pointer_cast<DebuggerBreakpoint>(breakpoint));

    if (debuggerIsRunning)
        Continue();
}

void DebuggerGDB::DeleteAllBreakpoints()
{
    bool debuggerIsRunning = !IsStopped();
    if (debuggerIsRunning)
        DoBreak(true);
    m_State.RemoveAllBreakpoints();

    if (debuggerIsRunning)
        Continue();
}

void DebuggerGDB::ShiftBreakpoint(int index, int lines_to_shift)
{
    BreakpointsList breakpoints = m_State.GetBreakpoints();
    BreakpointsList::iterator it = breakpoints.begin();
    std::advance(it, index);
    if (it != breakpoints.end())
        m_State.ShiftBreakpoint(*it, lines_to_shift);
}

void DebuggerGDB::EnableBreakpoint(cb::shared_ptr<cbBreakpoint> breakpoint, bool enable)
{
    bool debuggerIsRunning = !IsStopped();
    DebugLog(wxString::Format("DebuggerGDB::EnableBreakpoint(running=%d);", debuggerIsRunning?1:0));
    if (debuggerIsRunning)
        DoBreak(true);

    cb::shared_ptr<DebuggerBreakpoint> bp = cb::static_pointer_cast<DebuggerBreakpoint>(breakpoint);
    bp->enabled = enable;
    m_State.ResetBreakpoint(bp);

    if (debuggerIsRunning)
        Continue();
}

void DebuggerGDB::DeleteAllProjectBreakpoints(cbProject* project)
{
    m_State.RemoveAllProjectBreakpoints(project);
}

void DebuggerGDB::Continue()
{
    RunCommand(CMD_CONTINUE);
}

void DebuggerGDB::Next()
{
    RunCommand(CMD_STEP);
}

void DebuggerGDB::NextInstruction()
{
    RunCommand(CMD_STEP_INSTR);
}

void DebuggerGDB::StepIntoInstruction()
{
    RunCommand(CMD_STEP_INTO_INSTR);
}

void DebuggerGDB::Step()
{
    RunCommand(CMD_STEPIN);
}

bool DebuggerGDB::Validate(const wxString& line, const char cb)
{
    bool bResult = false;

    int bep = line.Find(cb)+1;
    int scs = line.Find(_T('\''))+1;
    int sce = line.Find(_T('\''),true)+1;
    int dcs = line.Find(_T('"'))+1;
    int dce = line.Find(_T('"'),true)+1;
    //No single and double quote
    if (!scs && !sce && !dcs && !dce) bResult = true;
    //No single/double quote in pair
    if (!(sce-scs) && !(dce-dcs)) bResult = true;
    //Outside of single quote
    if ((sce-scs) && ((bep < scs)||(bep >sce))) bResult = true;
    //Outside of double quote
    if ((dce-dcs) && ((bep < dcs)||(bep >dce))) bResult = true;

    return bResult;
}

void DebuggerGDB::StepOut()
{
    RunCommand(CMD_STEPOUT);
}

bool DebuggerGDB::RunToCursor(const wxString& filename, int line, const wxString& line_text)
{
    if (m_pProcess)
    {
        m_State.AddBreakpoint(filename, line, true, line_text);
        Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload();
        Continue();
        return true;
    }

    // Cold start: arm the one-shot BP only after the buffer has a real filesystem
    // path. Otherwise an unsaved Untitled* location is recorded, the red marker
    // appears while Save As is still open, and after save GDB gets a stale path
    // so the first Run to Cursor never stops (later rounds work once saved).
    wxString bpFile = filename;
    if (m_PidToAttach == 0)
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (ed && !ed->IsOK())
        {
            wxString sourcePath;
            if (!DebuggeeDescriptionUtil::SaveActiveSourceForStandaloneDebug(sourcePath))
                return false;
            bpFile = sourcePath;
        }
        else if (ed && !ed->GetFilename().empty())
            bpFile = ed->GetFilename();
    }

    if (!GetActiveConfigEx().GetFlag(DebuggerConfiguration::DoNotRun))
    {
        m_State.AddBreakpoint(bpFile, line, true, line_text);
        Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload();
    }
    return Debug(false);
}

void DebuggerGDB::SetNextStatement(const wxString& filename, int line)
{
    if (m_State.HasDriver() && IsStopped())
    {
        m_State.GetDriver()->SetNextStatement(filename, line);
    }
}

void DebuggerGDB::Break()
{
    DoBreak(false);
}

void DebuggerGDB::DoBreak(bool temporary)
{
    m_TemporaryBreak = temporary;

    // m_Process is PipedProcess I/O; m_Pid is debugger pid
    if (m_pProcess && m_Pid && !IsStopped())
    {
        long childPid = m_State.GetDriver()->GetChildPID();
        long pid = childPid;
    #ifndef __WXMSW__
        if (pid > 0 && !wxProcess::Exists(pid))
        {
            DebugLog(wxString::Format(_("Child process (pid:%ld) doesn't exist"), pid), Logger::warning);
            pid = 0;
        }
        if (pid <= 0)
            pid = m_Pid; // try poking gdb directly
        // non-windows gdb can interrupt the running process. yay!
        if (pid <= 0) // look out for the "fake" PIDs (killall)
            cbMessageBox(_("Unable to stop the debug process!"), _("Error"), wxOK | wxICON_WARNING);
        else
        {
            if (!wxProcess::Exists(pid))
                DebugLog(wxString::Format(_("GDB process (pid:%ld) doesn't exist"), pid), Logger::error);

            Log(wxString::Format(_("Trying to interrupt process with pid: %ld; child pid: %ld gdb pid: %ld"),
                                 pid, childPid, static_cast<long>(m_Pid)));
            wxKillError error;
            if (wxKill(pid, wxSIGINT, &error) != 0)
                DebugLog(wxString::Format(_("Can't kill process (%ld) %d"), pid, (int)(error)));
        }
    #else
        // windows gdb can interrupt the running process too. yay!
        if (   (pid <=0)
            && (CreateToolhelp32SnapshotFunc!=NULL)
            && (Process32FirstFunc!=NULL)
            && (Process32NextFunc!=NULL) )
        {
            HANDLE snap = CreateToolhelp32SnapshotFunc(TH32CS_SNAPALL,0);
            if (snap!=INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32 lppe;
                lppe.dwSize = sizeof(PROCESSENTRY32);
                BOOL ok = Process32FirstFunc(snap, &lppe);
                while ( ok == TRUE)
                {
                    if (static_cast<int>(lppe.th32ParentProcessID) == m_Pid) // Have my Child...
                    {
                        pid = lppe.th32ProcessID;
                        DebugLog(wxString::Format(_("Found child: %ld"),  pid));
                    }
                    lppe.dwSize = sizeof(PROCESSENTRY32);
                    ok = Process32NextFunc(snap, &lppe);
                }
                CloseHandle(snap);
            }
            else
                Log(_("No handle created. Trying to pause directly with cbd.exe..."), Logger::warning);
        }

        if (m_State.GetDriver()->UseDebugBreakProcess())
        {
            if (!DebugBreakProcessFunc)
                Log(_("DebugBreakProcess is not supported, you need Windows XP or newer..."), Logger::error);
            else if (pid > 0)
            {
                Log(wxString::Format(_("Trying to interrupt process with pid: %ld; child pid: %ld gdb pid: %ld"),
                                     pid, childPid, static_cast<long>(m_Pid)));
                HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)pid);
                if (proc)
                {
                    DebugBreakProcessFunc(proc); // yay!
                    CloseHandle(proc);
                }
                else
                    Log(_("Interrupting debugger failed :("), Logger::error);
            }
        }
        else
        {
            if (m_Pid > 0)
            {
                Log(_("Trying to interrupt the process by sending CTRL-C event to the console!"));
                if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0) == 0)
                {
                    Log(_("Interrupting debugger failed :("), Logger::error);
                    return;
                }
            }
        }
    #endif
        // Notify debugger plugins for end of debug session
        PluginManager *plm = Manager::Get()->GetPluginManager();
        CodeBlocksEvent evt(cbEVT_DEBUGGER_PAUSED);
        plm->NotifyPlugins(evt);
    }
}

void DebuggerGDB::Stop()
{
    // m_Process is PipedProcess I/O; m_Pid is debugger pid
    if (m_pProcess && m_Pid)
    {
        if (!IsStopped())
        {
            // TODO (obfuscated#): Check if this can be implemented on Windows
#ifdef __WXGTK__
            int childPID=m_State.GetDriver()->GetChildPID();
            if (childPID == 0)
            {
                DebugLog(_("Child pid is 0, so we will terminate GDB directly"));
                wxKill(m_Pid, wxSIGTERM);
                return;
            }
#endif
            Break();
        }
        RunCommand(CMD_STOP);
    }
}

void DebuggerGDB::ParseOutput(const wxString& output)
{
    if (!output.IsEmpty() && m_State.HasDriver())
    {
        m_State.GetDriver()->ParseOutput(output);
    }
}

void DebuggerGDB::ResetMacGdbSigningDiagnosis()
{
    m_macGdbSessionEvidence = GdbMacSessionEvidence();
    m_macGdbSigningDiagnosisLogged = false;
    // Keep m_macEnableDebuggingSuggested across a new Debug attempt until repair or healthy.
}

bool DebuggerGDB::MacEnableDebuggingMenuWanted()
{
    if (!platform::macosx)
        return false;
    // Confirmed bundled signing / task-port failure from a Debug attempt.
    // A bare "No stack", a missing debugger, or a healthy session does not qualify.
    if (m_macEnableDebuggingSuggested)
        return true;
    // Lab only: the force switch is how Tier 2 is reached when attach already works.
    return IsMacGdbForceTier2FromEnv()
        || GetActiveConfigEx().GetFlag(DebuggerConfiguration::ForceMacGdbTier2);
}

void DebuggerGDB::EnsureMacEnableDebuggingMenuItem()
{
    if (!platform::macosx)
        return;
    wxFrame *frame = Manager::Get()->GetAppFrame();
    if (!frame)
        return;
    wxMenuBar *mbar = frame->GetMenuBar();
    if (!mbar)
        return;
    // Anchor on Attach: the item sits just above it on the Debug menu.
    wxMenuItem *attach = mbar->FindItem(XRCID("idDebuggerMenuAttachToProcess"));
    if (!attach)
        return;
    wxMenu *debugMenu = attach->GetMenu();
    if (!debugMenu)
        return;

    wxMenuItem *existing = debugMenu->FindItem(idMenuEnableDebuggingMac);
    if (!MacEnableDebuggingMenuWanted())
    {
        if (existing)
            debugMenu->Delete(idMenuEnableDebuggingMac);
        return;
    }
    if (existing)
        return;

    size_t pos = 0;
    for (; pos < debugMenu->GetMenuItemCount(); ++pos)
    {
        if (debugMenu->FindItemByPosition(pos) == attach)
            break;
    }
    debugMenu->Insert(pos, idMenuEnableDebuggingMac,
                      _("Enable debugging on this Mac…"),
                      _("Restore ad-hoc signature and debugger entitlement on the bundled GDB when signing is defective"));
}

void DebuggerGDB::ObserveMacGdbOutputLine(const wxString &line)
{
    if (!platform::macosx)
        return;
    if (!ClassifyGdbMacSessionLine(line, m_macGdbSessionEvidence))
        return;
    // Log as soon as we have No stack or task-port evidence (do not wait for terminate).
    if (m_macGdbSessionEvidence.bareNoStack || m_macGdbSessionEvidence.taskPortOrTaskgated)
        LogMacGdbSigningDiagnosisOnce(_("GDB session evidence"));
}

void DebuggerGDB::LogMacGdbSigningDiagnosisOnce(const wxString &reason)
{
    if (m_macGdbSigningDiagnosisLogged)
        return;
    if (!platform::macosx)
        return;
    m_macGdbSigningDiagnosisLogged = true;

    const wxString exe = GetActiveConfigEx().GetDebuggerExecutable();
    const GdbMacSigningDiagnosis diag = DiagnoseMacGdbSigning(exe, m_macGdbSessionEvidence);
    // Show the Debug-menu item only for a confirmed signing / task-port failure.
    m_macEnableDebuggingSuggested = diag.WouldOfferEnableDebugging();
    EnsureMacEnableDebuggingMenuItem();

    DebugLog(wxString::Format(_("macOS GDB signing diagnosis trigger: %s"), reason));
    // Split multi-line summary so the debugger log stays readable.
    const wxString summary = FormatGdbMacSigningDiagnosisForLog(diag);
    wxStringTokenizer tok(summary, "\n", wxTOKEN_RET_EMPTY_ALL);
    while (tok.HasMoreTokens())
        Log(tok.GetNextToken());

    if (m_macEnableDebuggingSuggested)
    {
        // Prominent, red debugger-log guidance (do not require the user to hunt the menu).
        // Stay on / switch to the Debugger pane so Program Console does not hide this.
        ShowLog(false);
        Log(_("✕ Debugging cannot attach because the bundled debugger is not authorized."),
            Logger::error);
        const bool offer = GetActiveConfigEx().GetFlag(DebuggerConfiguration::OfferMacEnableDebugging);
        if (offer)
        {
            Log(_("   Code::Blocks will ask whether to enable debugging on this Mac."),
                Logger::error);
        }
        else
        {
            Log(_("   Automatic offers are off. Use Debug → Enable debugging on this Mac…, "
                  "or turn them back on under Settings → Debugger → GDB/CDB debugger."),
                Logger::error);
        }
        ShowLog(false);
        if (offer && !m_macEnableDebuggingOfferScheduled)
        {
            m_macEnableDebuggingOfferScheduled = true;
            CallAfter([this]() { OfferMacEnableDebugging(); });
        }
    }
}

namespace
{
int ShowMacEnableDebuggingOffer(bool *dontAskAgain)
{
    if (dontAskAgain)
        *dontAskAgain = false;
    wxWindow *parent = Manager::Get()->GetAppFrame();
    wxDialog dlg(parent, wxID_ANY, _("Enable debugging on this Mac"),
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);
    wxStaticText *text = new wxStaticText(&dlg, wxID_ANY,
        _("Debugging cannot attach on this Mac because the bundled debugger is not allowed to control the program.\n\n"
          "Code::Blocks can repair that. macOS may ask for an administrator password.\n\n"
          "You can also do this later with Debug → Enable debugging on this Mac…"));
    text->Wrap(460);
    root->Add(text, 0, wxALL | wxEXPAND, 12);
    wxCheckBox *silence = new wxCheckBox(&dlg, wxID_ANY, _("Don't ask again"));
    root->Add(silence, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    wxStdDialogButtonSizer *buttons = new wxStdDialogButtonSizer();
    wxButton *enable = new wxButton(&dlg, wxID_OK, _("Enable debugging"));
    wxButton *notNow = new wxButton(&dlg, wxID_CANCEL, _("Not now"));
    enable->SetDefault();
    buttons->AddButton(enable);
    buttons->AddButton(notNow);
    buttons->Realize();
    root->Add(buttons, 0, wxALL | wxALIGN_RIGHT, 12);
    dlg.SetSizerAndFit(root);
    dlg.CenterOnParent();
    const int answer = dlg.ShowModal();
    if (dontAskAgain)
        *dontAskAgain = silence->GetValue();
    return answer;
}
} // namespace

void DebuggerGDB::OfferMacEnableDebugging()
{
    m_macEnableDebuggingOfferScheduled = false;
    if (!platform::macosx || !m_macEnableDebuggingSuggested)
        return;
    if (!GetActiveConfigEx().GetFlag(DebuggerConfiguration::OfferMacEnableDebugging))
        return;

    const wxString exe = GetActiveConfigEx().GetDebuggerExecutable();
    const GdbMacSigningDiagnosis diag = DiagnoseMacGdbSigning(exe, m_macGdbSessionEvidence);
    if (diag.pathKind != GdbMacPathKind::Bundled)
        return;

    bool dontAskAgain = false;
    const int answer = ShowMacEnableDebuggingOffer(&dontAskAgain);
    if (dontAskAgain)
    {
        GetActiveConfigEx().SetFlag(DebuggerConfiguration::OfferMacEnableDebugging, false);
        Log(_("Enable debugging: automatic offers turned off. "
              "Turn them back on under Settings → Debugger → GDB/CDB debugger."));
    }
    if (answer != wxID_OK)
    {
        Log(_("Enable debugging: not now."));
        return;
    }
    RunMacEnableDebuggingRepair(true);
}

void DebuggerGDB::GetCurrentPosition(wxString &filename, int &line)
{
    if (m_State.HasDriver())
    {
        const Cursor& cursor = m_State.GetDriver()->GetCursor();
        filename = cursor.file;
        line = cursor.line;
    }
    else
    {
        filename = wxEmptyString;
        line = -1;
    }
}

// TODO: should reimplement
void DebuggerGDB::OnAddSymbolFile(wxCommandEvent& WXUNUSED(event))
{
    wxString file = wxFileSelector(_("Choose file to read symbols from"),
                                    "",
                                    "",
                                    "",
                                    _("Executables and libraries|*.exe;*.dll"),
                                    wxFD_OPEN | wxFD_FILE_MUST_EXIST | compatibility::wxHideReadonly);
    if (file.IsEmpty())
        return;
//    Manager::Get()->GetLogManager()->Log(m_PageIndex, _("Adding symbol file: %s"), file.wx_str());
    ConvertToGDBDirectory(file);
//    QueueCommand(new DbgCmd_AddSymbolFile(this, file));
}

void DebuggerGDB::SetupToolsMenu(wxMenu &menu)
{
    if (!GetActiveConfigEx().IsGDB())
        return;
    menu.Append(idMenuInfoFrame,   _("Current stack frame"), _("Displays info about the current (selected) stack frame"));
    menu.Append(idMenuInfoDLL,     _("Loaded libraries"), _("List dynamically loaded libraries (DLL/SO)"));
    menu.Append(idMenuInfoFiles,   _("Targets and files"), _("Displays info on the targets and files being debugged"));
    menu.Append(idMenuInfoFPU,     _("FPU status"), _("Displays the status of the floating point unit"));
    menu.Append(idMenuInfoSignals, _("Signal handling"), _("Displays how the debugger handles various signals"));
    menu.AppendSeparator();

    wxMenu *menuPrint = new wxMenu;
    menuPrint->AppendRadioItem(idMenuInfoPrintElementsUnlimited, _("Unlimited"),
                               _("The full arrays are printed (could lead to lock-ups if uninitialised data is printed)"));
    menuPrint->AppendRadioItem(idMenuInfoPrintElements20, "20");
    menuPrint->AppendRadioItem(idMenuInfoPrintElements50, "50");
    menuPrint->AppendRadioItem(idMenuInfoPrintElements100, "100");
    menuPrint->AppendRadioItem(idMenuInfoPrintElements200, _("200 (default)"));
    menu.AppendSubMenu(menuPrint, _("Print Elements"), _("Set limit on string chars or array elements to print"));
    menu.AppendCheckItem(idMenuInfoCatchThrow, _("Catch throw"),
                         _("If enabled the debugger will break when an exception is thrown"));
}

void DebuggerGDB::OnUpdateTools(wxUpdateUIEvent &event)
{
    bool checked = (event.GetId() == idMenuInfoPrintElementsUnlimited && m_printElements==0) ||
                   (event.GetId() == idMenuInfoPrintElements20 && m_printElements==20) ||
                   (event.GetId() == idMenuInfoPrintElements50 && m_printElements==50) ||
                   (event.GetId() == idMenuInfoPrintElements100 && m_printElements==100) ||
                   (event.GetId() == idMenuInfoPrintElements200 && m_printElements==200);
    event.Check(checked);
    event.Enable(IsRunning() && IsStopped());
}

void DebuggerGDB::OnPrintElements(wxCommandEvent &event)
{
    if (event.GetId() == idMenuInfoPrintElementsUnlimited)
        m_printElements = 0;
    else if (event.GetId() == idMenuInfoPrintElements20)
        m_printElements = 20;
    else if (event.GetId() == idMenuInfoPrintElements50)
        m_printElements = 50;
    else if (event.GetId() == idMenuInfoPrintElements100)
        m_printElements = 100;
    else if (event.GetId() == idMenuInfoPrintElements200)
        m_printElements = 200;
    else
        return;

    wxString cmd = wxString::Format("set print elements %d", m_printElements);
    m_State.GetDriver()->QueueCommand(new DebuggerCmd(m_State.GetDriver(), cmd));
    RequestUpdate(Watches);
}

void DebuggerGDB::OnUpdateCatchThrow(wxUpdateUIEvent &event)
{
    DebuggerConfiguration &config = GetActiveConfigEx();
    event.Enable(config.IsGDB() && IsStopped());
    event.Check(config.GetFlag(DebuggerConfiguration::CatchExceptions));
}

void DebuggerGDB::OnCatchThrow(wxCommandEvent &event)
{
    bool flag = event.IsChecked();
    GetActiveConfigEx().SetFlag(DebuggerConfiguration::CatchExceptions, flag);
    m_State.GetDriver()->EnableCatchingThrow(flag);
}

void DebuggerGDB::OnUpdateEnableDebuggingMac(wxUpdateUIEvent &event)
{
    // Presence is decided in Ensure (idle / after diagnosis). Do not delete the
    // item from inside its own update event.
    event.Enable(MacEnableDebuggingMenuWanted());
}

void DebuggerGDB::OnEnableDebuggingMac(cb_unused wxCommandEvent &event)
{
    RunMacEnableDebuggingRepair(false);
}

void DebuggerGDB::RunMacEnableDebuggingRepair(bool alreadyConsented)
{
    if (!platform::macosx)
        return;

    const wxString exe = GetActiveConfigEx().GetDebuggerExecutable();
    const bool forceTier2 = IsMacGdbForceTier2FromEnv()
                            || GetActiveConfigEx().GetFlag(DebuggerConfiguration::ForceMacGdbTier2);

    if (IsRunning())
    {
        Log(_("Enable debugging: stopping the current debugger session…"));
        Stop();
        int wait = 200; // ~2s
        while (IsRunning() && wait-- > 0)
        {
            wxMilliSleep(10);
            Manager::Yield();
        }
        if (IsRunning())
        {
            cbMessageBox(_("Could not stop the debugger.\n"
                           "Use Debug → Stop debugger, then try Enable debugging again."),
                         _("Enable debugging"), wxOK | wxICON_WARNING);
            return;
        }
        Log(_("Enable debugging: debugger stopped."));
    }

    const GdbMacSigningDiagnosis probe = DiagnoseMacGdbSigning(exe, m_macGdbSessionEvidence);
    if (probe.pathKind == GdbMacPathKind::Missing
        || probe.pathKind == GdbMacPathKind::NotBundled
        || probe.pathKind == GdbMacPathKind::NotOnMac)
    {
        m_macEnableDebuggingSuggested = false;
        EnsureMacEnableDebuggingMenuItem();
        const wxString msg = (probe.pathKind == GdbMacPathKind::NotBundled)
            ? _("Enable debugging only applies to the debugger bundled inside Code::Blocks.\n"
                "Your configured debugger path is elsewhere and will not be modified.")
            : _("The bundled debugger was not found. Enable debugging cannot run.");
        Log(msg, Logger::warning);
        cbMessageBox(msg, _("Enable debugging"), wxOK | wxICON_WARNING);
        return;
    }

    // Healthy ad-hoc and not forcing Tier 2 and no repair suggested → no-op.
    if (!forceTier2 && !probe.SigningDefective() && !m_macEnableDebuggingSuggested
        && !probe.session.taskPortOrTaskgated)
    {
        m_macEnableDebuggingSuggested = false;
        EnsureMacEnableDebuggingMenuItem();
        Log(_("Enable debugging: bundled GDB already looks healthy (no change)."));
        cbMessageBox(_("The bundled debugger already looks correctly signed.\n"
                       "No repair was needed."),
                     _("Enable debugging"), wxOK | wxICON_INFORMATION);
        return;
    }

    if (!alreadyConsented)
    {
        wxString prompt =
            _("Code::Blocks can repair signing of the bundled debugger on this Mac.\n\n");
        if (probe.SigningDefective())
            prompt += _("Tier 1: restore ad-hoc signature and debugger entitlement.\n");
        if (forceTier2 || (!probe.SigningDefective() && probe.session.taskPortOrTaskgated))
            prompt += _("Tier 2: sign with a local trusted identity in the System keychain "
                        "(macOS will ask for an administrator password to install it, "
                        "then again to trust it for code signing).\n");
        prompt += _("\nContinue?");

        AnnoyingDialog dlg(_("Enable debugging on this Mac"), prompt,
                           wxART_QUESTION, AnnoyingDialog::YES_NO, AnnoyingDialog::rtYES);
        if (dlg.ShowModal() != AnnoyingDialog::rtYES)
        {
            Log(_("Enable debugging: cancelled by user."));
            return;
        }
    }

    ShowLog(false);
    Log(_("Enable debugging: running repair (Tier 1 / Tier 2 as needed)…"));
    if (forceTier2)
        Log(_("Enable debugging: force Tier 2 is ON (lab)."));

#if !defined(__WXMAC__)
    return;
#else
    // Tier 2 is indicated by task-port evidence or the lab force switch.
    // The component does not see the debugger session.
    const bool runTier2 = forceTier2 || probe.session.taskPortOrTaskgated;
    const GdbMacProvisionResult repair = ProvisionMacBundledGdb(runTier2, forceTier2);
    if (!repair.log.empty())
    {
        wxStringTokenizer tok(repair.log, "\n", wxTOKEN_RET_EMPTY_ALL);
        while (tok.HasMoreTokens())
            Log(tok.GetNextToken());
    }
    if (!repair.tier2Attempted)
        Log(_("Enable debugging: Tier 2 not required."));

    const bool ok = repair.outcome == GdbMacProvisionOutcome::AlreadyHealthy
                 || repair.outcome == GdbMacProvisionOutcome::Tier1Restored
                 || repair.outcome == GdbMacProvisionOutcome::Tier2Provisioned;
    if (ok)
    {
        m_macEnableDebuggingSuggested = false;
        EnsureMacEnableDebuggingMenuItem();
        cbMessageBox(_("Debugger signing repair finished.\n"
                       "Try Debug again."),
                     _("Enable debugging"), wxOK | wxICON_INFORMATION);
    }
    else
    {
        // Cancelled trust is still "repair did not finish" — same dialog as a failure.
        cbMessageBox(_("Could not fully repair bundled debugger signing.\n"
                       "See the Debugger log for details.\n"
                       "macOS may have prompted for Keychain or admin access."),
                     _("Enable debugging"), wxOK | wxICON_ERROR);
    }
#endif
}

void DebuggerGDB::OnInfoFrame(wxCommandEvent& WXUNUSED(event))
{
    if (m_State.HasDriver())
        m_State.GetDriver()->InfoFrame();
}

void DebuggerGDB::OnInfoDLL(wxCommandEvent& WXUNUSED(event))
{
    if (m_State.HasDriver())
    {
        m_State.GetDriver()->InfoDLL();
    }
}

void DebuggerGDB::OnInfoFiles(wxCommandEvent& WXUNUSED(event))
{
    if (m_State.HasDriver())
        m_State.GetDriver()->InfoFiles();
}

void DebuggerGDB::OnInfoFPU(wxCommandEvent& WXUNUSED(event))
{
    if (m_State.HasDriver())
        m_State.GetDriver()->InfoFPU();
}

void DebuggerGDB::OnInfoSignals(wxCommandEvent& WXUNUSED(event))
{
    if (m_State.HasDriver())
        m_State.GetDriver()->InfoSignals();
}

void DebuggerGDB::OnGDBOutput(wxCommandEvent& event)
{
    wxString msg = event.GetString();
    if (!msg.IsEmpty())
        ParseOutput(msg);
}

void DebuggerGDB::OnGDBError(wxCommandEvent& event)
{
    wxString msg = event.GetString();
    if (!msg.IsEmpty())
        ParseOutput(msg);
}

void DebuggerGDB::OnGDBTerminated(wxCommandEvent& event)
{
    m_PidToAttach = 0;

    m_TimerPollDebugger.Stop();
    m_LastExitCode = event.GetInt();
    //the process deletes itself
//    m_pProcess = nullptr;

    ClearActiveMarkFromAllEditors();
    m_State.StopDriver(); // drops temporary BPs from the list
    // Reload syncs the breakpoints dialog and editor gutter markers (see
    // BreakpointsDlg::Reload) so Step Into / GDB `start` temps leave no orphan
    // red markers after the session ends.
    Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload();
    if (!Manager::IsAppShuttingDown())
    {
        Log(wxString::Format(_("Debugger finished with status %d"), m_LastExitCode));

        // v2.3 M2: diagnose bundled gdb signing when the session looked unhealthy.
        if (m_macGdbSessionEvidence.bareNoStack
            || m_macGdbSessionEvidence.taskPortOrTaskgated
            || m_macGdbSessionEvidence.launchFailed
            || m_LastExitCode != 0)
        {
            LogMacGdbSigningDiagnosisOnce(
                wxString::Format(_("debugger finished (status %d)"), m_LastExitCode));
        }

        if (m_NoDebugInfo)
        {
            if (m_UseStandaloneDebuggee)
            {
                cbMessageBox(_("This program appears to have no debugging information.\n"
                               "Standalone Debug rebuilds with the compiler's "
                               "'Produce debugging symbols' option; check the default "
                               "compiler settings, re-compile and retry..."),
                             _("Error"), wxICON_STOP);
            }
            else
            {
                cbMessageBox(_("This project/target has no debugging info."
                               "Please change this in the project's build options, re-compile and retry..."),
                             _("Error"), wxICON_STOP);
            }
        }
    }

    // Notify debugger plugins for end of debug session
    PluginManager *plm = Manager::Get()->GetPluginManager();
    CodeBlocksEvent evt(cbEVT_DEBUGGER_FINISHED);
    plm->NotifyPlugins(evt);

    // switch to the user-defined layout when finished debugging
    if (!Manager::IsAppShuttingDown())
        SwitchToPreviousLayout();
    KillConsole();
    MarkAsStopped();

    // Allow a clean Debug/Step Into restart of the same standalone file.
    m_UseStandaloneDebuggee = false;
    m_StandaloneDebuggee = DebuggeeDescription();

    ///killerbot : run there the post shell commands ?
}

void DebuggerGDB::KillConsole()
{
#ifdef __WXMSW__
    if (m_bIsConsole)
    {
        // remove the CTRL_C handler
        SetConsoleCtrlHandler(HandlerRoutine, FALSE);
        FreeConsole();
        m_bIsConsole = false;
    }
#else
    if (m_programConsoleDebug)
    {
        /* Explicit PtySession Close — do not rely on master EOF (held slave). */
        ProgramConsole::EndDebugSession();
        m_programConsoleDebug = false;
        m_bIsConsole = false;
        m_stopDebuggerConsoleClosed = false;
        m_nConsolePid = 0;
        return;
    }
    // Forced cleanup: SIGTERM → cb_debug_console_hold exits immediately (no key wait).
    // wxKill does not wait for the process; it only delivers the signal.
    if ( m_bIsConsole && (m_nConsolePid > 0) )
    {
       ::wxKill(m_nConsolePid, wxSIGTERM, nullptr, wxKILL_CHILDREN); //ticket 1571
        m_nConsolePid = 0;
        m_bIsConsole = false;
        m_stopDebuggerConsoleClosed = false;
    }
#endif
}

void DebuggerGDB::NotifyDebugConsoleFinished(bool announceExitCode, int exitCode)
{
#ifndef __WXMSW__
    if (m_programConsoleDebug)
    {
        if (announceExitCode)
            ProgramConsole::AnnounceDebugFinished(exitCode);
        ProgramConsole::EndDebugSession();
        m_programConsoleDebug = false;
        m_bIsConsole = false;
        m_stopDebuggerConsoleClosed = false;
        m_nConsolePid = 0;
        return;
    }
    wxUnusedVar(announceExitCode);
    wxUnusedVar(exitCode);
    // Soft finish: SIGUSR1 → holder may show "Press any key to close...".
    // Clear PID first so OnGDBTerminated / KillConsole will not SIGTERM a
    // helper that is legitimately waiting for keyboard input.
    // (wxSignal has no USR1; use POSIX kill.)
    if (m_bIsConsole && m_nConsolePid > 0)
    {
        const int pid = m_nConsolePid;
        m_nConsolePid = 0;
        m_bIsConsole = false;
        m_stopDebuggerConsoleClosed = false;
        if (kill(pid, SIGUSR1) != 0 && errno != ESRCH)
        {
            DebugLog(wxString::Format(
                _("Failed to signal debug console holder %d: %s"),
                pid, wxSysErrorMsg()));
        }
    }
#endif
}

void DebuggerGDB::CheckIfConsoleIsClosed()
{
#ifndef __WXMSW__
    // Detect if the console is closed by the user and if it is stop the session.
    if (m_stopDebuggerConsoleClosed && m_nConsolePid > 0 && wxKill(m_nConsolePid, wxSIGNONE) != 0)
    {
        AnnoyingDialog dialog(_("Terminal/Console closed"),
                              _("Detected that the Terminal/Console has been closed. "
                                "Do you want to stop the debugging session?"),
                              wxART_QUESTION);
        if (dialog.ShowModal() == AnnoyingDialog::rtNO)
            m_stopDebuggerConsoleClosed = false;
        else
        {
            Stop();
            m_nConsolePid = 0;
        }
    }
#endif
}

bool DebuggerGDB::ShowValueTooltip(int style)
{
    if (!m_pProcess || !IsStopped())
        return false;

    if (!m_State.HasDriver() || !m_State.GetDriver()->IsDebuggingStarted())
        return false;

    if (!GetActiveConfigEx().GetFlag(DebuggerConfiguration::EvalExpression))
        return false;
    if (style != wxSCI_C_DEFAULT && style != wxSCI_C_OPERATOR && style != wxSCI_C_IDENTIFIER &&
        style != wxSCI_C_WORD2 && style != wxSCI_C_GLOBALCLASS  && style != wxSCI_C_WXSMITH &&
        style != wxSCI_F_IDENTIFIER)
    {
        return false;
    }
    return true;
}

void DebuggerGDB::OnValueTooltip(const wxString &token, const wxRect &evalRect)
{
    m_State.GetDriver()->EvaluateSymbol(token, evalRect);
}

void DebuggerGDB::CleanupWhenProjectClosed(cbProject *project)
{
    // remove all breakpoints belonging to the closed project
    DeleteAllProjectBreakpoints(project);
    // FIXME (#obfuscated): Optimize this when multiple projects are closed
    //                      (during workspace close operation for exmaple).
    cbBreakpointsDlg *dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
    dlg->Reload();
}

void DebuggerGDB::OnIdle(wxIdleEvent& event)
{
    // Re-insert if the Debug menu was rebuilt without our item (macOS only).
    EnsureMacEnableDebuggingMenuItem();

    if (m_pProcess && ((PipedProcess*)m_pProcess)->HasInput())
        event.RequestMore();
    event.Skip();
}

void DebuggerGDB::OnTimer(cb_unused wxTimerEvent& event)
{
    // send any buffered (previous) output
    ParseOutput(wxEmptyString);

    CheckIfConsoleIsClosed();

    wxWakeUpIdle();
}

void DebuggerGDB::OnShowFile(wxCommandEvent& event)
{
    SyncEditor(event.GetString(), event.GetInt(), false);
}

void DebuggerGDB::DebuggeeContinued()
{
    m_TemporaryBreak = false;
    /* Inferior running again — Program Console may focus once cin blocks. */
    if (m_programConsoleDebug)
        ProgramConsole::NotifyDebugInferiorState(true);
}

void DebuggerGDB::OnCursorChanged(wxCommandEvent& WXUNUSED(event))
{
    if (m_TemporaryBreak)
        return;

    if (m_State.HasDriver())
    {
        const Cursor& cursor = m_State.GetDriver()->GetCursor();
        // checking if driver is stopped is redundant because it would only
        // send us this event if it was stopped anyway
        if (/*m_State.GetDriver()->IsStopped() &&*/ cursor.changed)
        {
            /* Stopped (breakpoint / step) — do not steal focus for cin yet. */
            if (m_programConsoleDebug)
                ProgramConsole::NotifyDebugInferiorState(false);

            bool autoSwitch = cbDebuggerCommonConfig::GetFlag(cbDebuggerCommonConfig::AutoSwitchFrame);

            MarkAllWatchesAsUnchanged();

            // if the cursor line is invalid and the auto switch is on,
            // we don't sync the editor, because there is no line to sync to
            // and also we are going to execute a backtrace command hoping to find a valid frame.
            if (!autoSwitch || cursor.line != -1)
                SyncEditor(cursor.file, cursor.line);

            BringCBToFront();
            if (cursor.line != -1)
                Log(wxString::Format(_("At %s:%ld"), cursor.file.wx_str(), cursor.line));
            else
                Log(wxString::Format(_("In %s (%s)"), cursor.function.wx_str(), cursor.file.wx_str()));

            // update watches
            DebuggerManager *dbg_manager = Manager::Get()->GetDebuggerManager();

            if (IsWindowReallyShown(dbg_manager->GetWatchesDialog()->GetWindow()))
                DoWatches();

            // update CPU registers
            if (dbg_manager->UpdateCPURegisters())
                RunCommand(CMD_REGISTERS);

            // update callstack
            if (dbg_manager->UpdateBacktrace())
                RunCommand(CMD_BACKTRACE);
            else
            {
                if (cursor.line == -1 && autoSwitch)
                    RunCommand(CMD_BACKTRACE);
            }

            // update disassembly
            if (dbg_manager->UpdateDisassembly())
            {
                uint64_t addr = cbDebuggerStringToAddress(cursor.address);
                //if zero addr, don't attempt disassembly
                if (addr && !dbg_manager->GetDisassemblyDialog()->SetActiveAddress(addr))
                    RunCommand(CMD_DISASSEMBLE);
            }

            // update memory examiner
            if (dbg_manager->UpdateExamineMemory())
                RunCommand(CMD_MEMORYDUMP);

            // update running threads
            if (dbg_manager->UpdateThreads())
                RunCommand(CMD_RUNNINGTHREADS);

            // Notify everybody that the debugger cursor has changed.
            // This might be used by some view windows to request the debugger plugin to update some
            // data.
            CodeBlocksEvent cursorChangeEvent(cbEVT_DEBUGGER_CURSOR_CHANGED);
            cursorChangeEvent.SetPlugin(this);
            Manager::Get()->ProcessEvent(cursorChangeEvent);
        }
    }
}

cb::shared_ptr<cbWatch> DebuggerGDB::AddWatch(const wxString& symbol, bool update)
{
    cb::shared_ptr<GDBWatch> watch(new GDBWatch(CleanStringValue(symbol)));
    m_watches.push_back(watch);
    m_mapWatchesToType[watch] = WatchType::Normal;

    if (m_pProcess && update)
        m_State.GetDriver()->UpdateWatch(m_watches.back());

    return watch;
}

cb::shared_ptr<cbWatch> DebuggerGDB::AddMemoryRange(uint64_t address, uint64_t size, const wxString &symbol, bool update)
{
    cb::shared_ptr<GDBMemoryRangeWatch> watch(new GDBMemoryRangeWatch(address, size, symbol));
    m_memoryRanges.push_back(watch);
    m_mapWatchesToType[watch] = WatchType::MemoryRange;

    if (m_pProcess && update)
        m_State.GetDriver()->UpdateMemoryRangeWatch(m_memoryRanges.back());

    return watch;
}

void DebuggerGDB::AddWatchNoUpdate(const cb::shared_ptr<GDBWatch> &watch)
{
    m_watches.push_back(watch);
    m_mapWatchesToType[watch] = WatchType::Normal;
}

void DebuggerGDB::DeleteWatch(cb::shared_ptr<cbWatch> watch)
{
    MapWatchesToType::iterator itType = m_mapWatchesToType.find(watch);
    if (itType == m_mapWatchesToType.end())
        return;

    switch (itType->second)
    {
    case WatchType::Normal:
        {
            WatchesContainer::iterator it = std::find(m_watches.begin(), m_watches.end(), watch);
            if (it != m_watches.end())
            {
                m_watches.erase(it);
                return;
            }
        }
        break;
    case WatchType::MemoryRange:
        {
            MemoryRangeWatchesContainer::iterator it = std::find(m_memoryRanges.begin(),
                                                                 m_memoryRanges.end(), watch);
            if (it != m_memoryRanges.end())
            {
                m_memoryRanges.erase(it);
                return;
            }
            break;
        }
    }

}

bool DebuggerGDB::HasWatch(cb::shared_ptr<cbWatch> watch)
{
    if (watch == m_localsWatch || watch == m_funcArgsWatch)
        return true;

    return m_mapWatchesToType.find(watch) != m_mapWatchesToType.end();
}

bool DebuggerGDB::IsMemoryRangeWatch(const cb::shared_ptr<cbWatch> &watch)
{
    MapWatchesToType::const_iterator it = m_mapWatchesToType.find(watch);
    if (it == m_mapWatchesToType.end())
        return false;
    else
        return it->second == WatchType::MemoryRange;
}

void DebuggerGDB::ShowWatchProperties(cb::shared_ptr<cbWatch> watch)
{
    // not supported for child nodes or memory ranges!
    if (watch->GetParent() || IsMemoryRangeWatch(watch))
        return;

    cb::shared_ptr<GDBWatch> real_watch = cb::static_pointer_cast<GDBWatch>(watch);
    EditWatchDlg dlg(real_watch, nullptr);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
        DoWatches();
}

bool DebuggerGDB::SetWatchValue(cb::shared_ptr<cbWatch> watch, const wxString &value)
{
    if (!m_State.HasDriver())
        return false;

    cb::shared_ptr<cbWatch> rootWatch = cbGetRootWatch(watch);
    MapWatchesToType::const_iterator itType = m_mapWatchesToType.find(rootWatch);
    if (itType == m_mapWatchesToType.end())
        return false;

    const WatchType type = itType->second;
    if (type == WatchType::MemoryRange)
    {
        DebuggerDriver* driver = m_State.GetDriver();
        cb::shared_ptr<GDBMemoryRangeWatch> temp_watch = std::static_pointer_cast<GDBMemoryRangeWatch>(watch);

        uint64_t addr = temp_watch->GetAddress();
        if (addr == 0)
        {
            driver->SetMemoryRangeValue(addr, value);
        }
        else
        {
            wxString symbol;
            temp_watch->GetSymbol(symbol);
            driver->SetMemoryRangeValue(symbol, value);
        }
    }
    else
    {
        wxString full_symbol;
        cb::shared_ptr<cbWatch> temp_watch = watch;
        if (g_DebugLanguage == dl_Cpp)
        {
            while (temp_watch)
            {
                wxString symbol;
                temp_watch->GetSymbol(symbol);
                temp_watch = temp_watch->GetParent();

                if (symbol.find('*') != wxString::npos || symbol.find('&') != wxString::npos)
                    symbol = wxT('(') + symbol + wxT(')');

                if (full_symbol.empty())
                    full_symbol = symbol;
                else
                    full_symbol = symbol + wxT('.') + full_symbol;
            }
        }
        else // Fortran language
        {
            while (temp_watch)
            {
                wxString symbol;
                temp_watch->GetSymbol(symbol);
                temp_watch = temp_watch->GetParent();

                if (full_symbol.empty())
                    full_symbol = symbol;
                else
                {
                    if (full_symbol.at(0) == '(' && symbol.at(0) == '(')
                    {
                        size_t sec = full_symbol.find(')');
                        if (sec != wxString::npos && symbol.at(symbol.size()-1) == ')')
                        {
                            full_symbol = full_symbol.substr(0,sec) + wxT(',') + symbol.substr(1,symbol.size()-2) +
                                          full_symbol.substr(sec);
                        }
                    }
                    else if (full_symbol.at(0) == '(')
                        full_symbol = symbol + full_symbol;
                    else
                        full_symbol = symbol + wxT('%') + full_symbol;
                }
            }
        }

        DebuggerDriver* driver = m_State.GetDriver();
        driver->SetVarValue(full_symbol, value);
    }

    DoWatches();
    return true;
}

void DebuggerGDB::ExpandWatch(cb_unused cb::shared_ptr<cbWatch> watch) // TODO: shouldn't this do something?
{
}

void DebuggerGDB::CollapseWatch(cb_unused cb::shared_ptr<cbWatch> watch)
{
}

void DebuggerGDB::UpdateWatch(cb::shared_ptr<cbWatch> watch)
{
    DebuggerDriver *driver = m_State.GetDriver();
    if (driver == nullptr)
        return;

    if (watch == m_localsWatch)
        driver->UpdateWatchLocalsArgs(cb::static_pointer_cast<GDBWatch>(watch), true);
    else if (watch == m_funcArgsWatch)
        driver->UpdateWatchLocalsArgs(cb::static_pointer_cast<GDBWatch>(watch), false);
    else
    {
        MapWatchesToType::const_iterator itType = m_mapWatchesToType.find(watch);
        if (itType == m_mapWatchesToType.end())
            return;
        const WatchType type = itType->second;
        switch (type)
        {
        case WatchType::Normal:
            driver->UpdateWatch(cb::static_pointer_cast<GDBWatch>(watch));
            break;
        case WatchType::MemoryRange:
            driver->UpdateMemoryRangeWatch(cb::static_pointer_cast<GDBMemoryRangeWatch>(watch));
            break;
        }
    }
}

void DebuggerGDB::UpdateWatches(const std::vector<cb::shared_ptr<cbWatch>> &watches)
{
    if (!m_State.HasDriver())
        return;

    WatchesContainer normalWatches;
    MemoryRangeWatchesContainer memoryRanges;

    cb::shared_ptr<GDBWatch> localsToUpdate, funcArgsToUpdate;

    for (const cb::shared_ptr<cbWatch> &watch : watches)
    {
        if (watch == m_localsWatch)
        {
            localsToUpdate = m_localsWatch;
            continue;
        }
        if (watch == m_funcArgsWatch)
        {
            funcArgsToUpdate = m_funcArgsWatch;
            continue;
        }

        MapWatchesToType::const_iterator itType = m_mapWatchesToType.find(watch);
        if (itType == m_mapWatchesToType.end())
            continue;

        const WatchType type = itType->second;
        switch (type)
        {
        case WatchType::Normal:
            normalWatches.push_back(cb::static_pointer_cast<GDBWatch>(watch));
            break;
        case WatchType::MemoryRange:
            memoryRanges.push_back(cb::static_pointer_cast<GDBMemoryRangeWatch>(watch));
            break;
        }
    }

    if (!normalWatches.empty())
        m_State.GetDriver()->UpdateWatches(localsToUpdate, funcArgsToUpdate, normalWatches, true);
    if (!memoryRanges.empty())
        m_State.GetDriver()->UpdateMemoryRangeWatches(memoryRanges, true);
}

void DebuggerGDB::MarkAllWatchesAsUnchanged()
{
    if (m_localsWatch)
        m_localsWatch->MarkAsChangedRecursive(false);
    if (m_funcArgsWatch)
        m_funcArgsWatch->MarkAsChangedRecursive(false);

    for (WatchesContainer::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
        (*it)->MarkAsChangedRecursive(false);
}

namespace
{
wxString createSymbolFromWatch(const cbWatch &watch)
{
    wxString symbol;
    watch.GetSymbol(symbol);

    cb::shared_ptr<const cbWatch> parentWatch = watch.GetParent();
    if(parentWatch)
    {
        wxString parent = createSymbolFromWatch(*parentWatch);
        if(!parent.IsEmpty())
            return parent + "." + symbol;
    }
    return symbol;
}
}   // anonymous namespace

void DebuggerGDB::OnWatchesContextMenu(wxMenu &menu, const cbWatch &watch, wxObject *property, int &disabledMenus)
{
    wxString type, symbol;
    watch.GetType(type);
    watch.GetSymbol(symbol);

    if (IsPointerType(type))
    {
        menu.InsertSeparator(0);
        menu.Insert(0, idMenuWatchDereference, _("Dereference ") + symbol);
        m_watchToDereferenceSymbol = symbol;
        m_watchToDereferenceProperty = property;
    }

    if (watch.GetParent())
    {
        disabledMenus = WatchesDisabledMenuItems::Rename;
        disabledMenus |= WatchesDisabledMenuItems::Properties;
        disabledMenus |= WatchesDisabledMenuItems::Delete;
        disabledMenus |= WatchesDisabledMenuItems::AddDataBreak;
        disabledMenus |= WatchesDisabledMenuItems::ExamineMemory;

        menu.InsertSeparator(0);
        menu.Insert(0, idMenuWatchSymbol, _("Watch ") + symbol);
        m_watchToAddSymbol = createSymbolFromWatch(watch);
    }
}

void DebuggerGDB::OnMenuWatchDereference(cb_unused wxCommandEvent& event)
{
    cbWatchesDlg *watches = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    if (!watches)
        return;

    watches->RenameWatch(m_watchToDereferenceProperty, "*" + m_watchToDereferenceSymbol);
    m_watchToDereferenceProperty = NULL;
    m_watchToDereferenceSymbol = wxEmptyString;
}

void DebuggerGDB::OnMenuWatchSymbol(cb_unused wxCommandEvent& event)
{
    cbWatchesDlg *watches = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    if (!watches)
        return;

    watches->AddWatch(AddWatch(m_watchToAddSymbol, true));
    m_watchToAddSymbol = wxEmptyString;
}

void DebuggerGDB::AttachToProcess(const wxString& pid)
{
    if (!pid.IsEmpty())
    {
        pid.ToLong((long*)&m_PidToAttach);
        Debug(false);
    }
}

void DebuggerGDB::DetachFromProcess()
{
    m_State.GetDriver()->Detach();
    m_PidToAttach = 0;
    m_State.GetDriver()->Stop();
}

bool DebuggerGDB::IsAttachedToProcess() const
{
    return m_PidToAttach != 0;
}

bool DebuggerGDB::CompilerFinished(bool compilerFailed, StartType startType)
{
    if (compilerFailed || startType == StartTypeUnknown)
        return false;
    if (DoDebug(startType == StartTypeStepInto) != 0)
        return false;
    return true;
}

void DebuggerGDB::OnBuildTargetSelected(CodeBlocksEvent& event)
{
    // verify that the project that sent it, is the one we 're debugging
    // and that a project is loaded
    if (m_pProject && event.GetProject() == m_pProject)
        m_ActiveBuildTarget = event.GetBuildTargetName();
}

void DebuggerGDB::DetermineLanguage()
{
    if (m_State.HasDriver())
        m_State.GetDriver()->DetermineLanguage();
}
